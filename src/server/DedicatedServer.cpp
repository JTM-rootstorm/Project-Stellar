#include "stellar/server/DedicatedServer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "stellar/assets/WorldMetadataAsset.hpp"
#include "stellar/import/bsp/Diagnostics.hpp"
#include "stellar/import/bsp/Validation.hpp"
#include "stellar/network/Messages.hpp"
#include "stellar/network/SnapshotCodec.hpp"
#include "stellar/network/SnapshotDelta.hpp"
#include "stellar/network/SocketTransport.hpp"
#include "stellar/network/Transport.hpp"
#include "stellar/scripting/ScriptedWorldSession.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::server {
namespace {

struct ScriptRegistryLoadResult {
    stellar::scripting::ScriptRegistry registry;
    std::vector<std::string> loaded_script_ids;
};

struct PreparedAuthority {
    std::unique_ptr<stellar::assets::LevelAsset> level;
    std::unique_ptr<stellar::world::RuntimeWorld> world;
    stellar::network::MapIdentity map_identity;
    std::variant<stellar::server::WorldSession, stellar::scripting::ScriptedWorldSession> session;
};

[[nodiscard]] stellar::platform::Error error(std::string message) {
    return stellar::platform::Error(std::move(message));
}

[[nodiscard]] std::string lowercase_extension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension;
}

[[nodiscard]] bool is_ascii_alpha(char value) noexcept {
    return std::isalpha(static_cast<unsigned char>(value)) != 0;
}

[[nodiscard]] bool script_id_has_path_escape(std::string_view script_id) noexcept {
    if (script_id.empty() || script_id.front() == '/' || script_id.front() == '\\') {
        return true;
    }
    if (script_id.size() >= 2 && is_ascii_alpha(script_id[0]) && script_id[1] == ':') {
        return true;
    }
    std::size_t segment_begin = 0;
    for (std::size_t index = 0; index <= script_id.size(); ++index) {
        if (index == script_id.size() || script_id[index] == '/' || script_id[index] == '\\') {
            if (script_id.substr(segment_begin, index - segment_begin) == "..") {
                return true;
            }
            segment_begin = index + 1;
        }
    }
    return false;
}

[[nodiscard]] bool relative_path_stays_within_root(const std::filesystem::path& relative) {
    if (relative.is_absolute()) {
        return false;
    }
    for (const auto& component : relative.lexically_normal()) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::vector<std::string> script_ids_for_world(
    const stellar::world::RuntimeWorld& world) {
    std::vector<std::string> ids;
    for (const stellar::assets::WorldMarker& marker : world.world_metadata.markers) {
        if ((marker.type != stellar::assets::WorldMarkerType::kTrigger &&
             marker.type != stellar::assets::WorldMarkerType::kObjectCollider) ||
            !marker.script.has_value()) {
            continue;
        }
        if (std::ranges::find(ids, marker.script->script_id) == ids.end()) {
            ids.push_back(marker.script->script_id);
        }
    }
    return ids;
}

[[nodiscard]] bool world_has_script_bindings(const stellar::world::RuntimeWorld& world) noexcept {
    for (const stellar::assets::WorldMarker& marker : world.world_metadata.markers) {
        if ((marker.type == stellar::assets::WorldMarkerType::kTrigger ||
             marker.type == stellar::assets::WorldMarkerType::kObjectCollider) &&
            marker.script.has_value()) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::expected<std::string, stellar::platform::Error> read_text_file(
    const std::filesystem::path& path,
    const std::string& script_id) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::unexpected(error("Missing script source for id: " + script_id + " at " +
                                     path.string()));
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    if (!file.good() && !file.eof()) {
        return std::unexpected(error("Failed to read script source for id: " + script_id + " at " +
                                     path.string()));
    }
    return stream.str();
}

[[nodiscard]] std::expected<ScriptRegistryLoadResult, stellar::platform::Error>
load_script_registry_for_world(const stellar::world::RuntimeWorld& world,
                               const DedicatedServerConfig& config) {
    ScriptRegistryLoadResult result;
    const std::vector<std::string> script_ids = script_ids_for_world(world);
    if (script_ids.empty()) {
        return result;
    }

    const std::filesystem::path root = std::filesystem::absolute(
                                           config.script_root.has_value()
                                               ? std::filesystem::path(*config.script_root)
                                               : std::filesystem::path(config.map_path).parent_path())
                                           .lexically_normal();
    for (const std::string& script_id : script_ids) {
        if (script_id_has_path_escape(script_id)) {
            return std::unexpected(error("Script id escapes configured script root: " + script_id));
        }
        const std::filesystem::path relative = std::filesystem::path(script_id).lexically_normal();
        if (!relative_path_stays_within_root(relative)) {
            return std::unexpected(
                error("Script id escapes configured script root after normalization: " + script_id));
        }
        const std::filesystem::path script_path = (root / relative).lexically_normal();
        const std::filesystem::path relative_to_root = script_path.lexically_relative(root);
        if (relative_to_root.empty() || !relative_path_stays_within_root(relative_to_root)) {
            return std::unexpected(
                error("Script source path escapes configured script root: " + script_id));
        }
        auto source = read_text_file(script_path, script_id);
        if (!source) {
            return std::unexpected(source.error());
        }
        result.registry.set_script(script_id, std::move(*source));
        result.loaded_script_ids.push_back(script_id);
    }
    return result;
}

[[nodiscard]] std::expected<void, stellar::platform::Error> validate_config_values(
    const DedicatedServerConfig& config) {
    if (config.map_path.empty()) {
        return std::unexpected(error("--map is required"));
    }
    if (lowercase_extension(config.map_path) != ".bsp") {
        return std::unexpected(error("Unsupported map extension for --map (expected .bsp)"));
    }
    if (auto parsed = stellar::network::parse_socket_address(config.listen_endpoint); !parsed) {
        return std::unexpected(error("Invalid --listen endpoint: " + parsed.error().message));
    }
    if (config.tick_rate <= 0 || config.tick_rate > 1000) {
        return std::unexpected(error("--tick-rate must be a positive integer up to 1000"));
    }
    if (config.snapshot_rate <= 0 || config.snapshot_rate > 1000) {
        return std::unexpected(error("--snapshot-rate must be a positive integer up to 1000"));
    }
    if (config.max_clients != 1) {
        return std::unexpected(error("ST-5 dedicated server supports --max-clients 1 only"));
    }
    return {};
}

[[nodiscard]] std::expected<PreparedAuthority, stellar::platform::Error> prepare_authority(
    const DedicatedServerConfig& config) {
    if (auto valid = validate_config_values(config); !valid) {
        return std::unexpected(valid.error());
    }

    auto validation = stellar::import::bsp::validate_level(config.map_path);
    if (!validation) {
        return std::unexpected(validation.error());
    }
    if (!validation->valid || !validation->loaded_level.has_value()) {
        for (const auto& diagnostic : validation->report.diagnostics) {
            if (diagnostic.severity == stellar::import::bsp::DiagnosticSeverity::kError) {
                return std::unexpected(error(diagnostic.message));
            }
        }
        return std::unexpected(error("BSP map validation failed for --map: " + config.map_path));
    }

    auto level = std::make_unique<stellar::assets::LevelAsset>(
        std::move(validation->loaded_level->asset));
    auto world = std::make_unique<stellar::world::RuntimeWorld>(
        stellar::world::build_runtime_world(*level));
    stellar::network::MapIdentity identity = stellar::network::make_map_identity(*world);

    if (world_has_script_bindings(*world)) {
        auto registry = load_script_registry_for_world(*world, config);
        if (!registry) {
            return std::unexpected(registry.error());
        }
        auto scripted = stellar::scripting::ScriptedWorldSession::create(
            *world, stellar::server::WorldSessionConfig{}, std::move(registry->registry));
        if (!scripted) {
            return std::unexpected(error("Failed to load authoritative scripts for --map: " +
                                         scripted.error().message));
        }
        return PreparedAuthority{.level = std::move(level),
                                 .world = std::move(world),
                                 .map_identity = std::move(identity),
                                 .session = std::variant<stellar::server::WorldSession,
                                                         stellar::scripting::ScriptedWorldSession>(
                                     std::in_place_type<stellar::scripting::ScriptedWorldSession>,
                                     std::move(*scripted))};
    }

    std::variant<stellar::server::WorldSession, stellar::scripting::ScriptedWorldSession> session(
        std::in_place_type<stellar::server::WorldSession>, *world,
        stellar::server::WorldSessionConfig{});
    return PreparedAuthority{.level = std::move(level),
                             .world = std::move(world),
                             .map_identity = std::move(identity),
                             .session = std::move(session)};
}

[[nodiscard]] float fixed_dt(const DedicatedServerConfig& config) noexcept {
    return 1.0F / static_cast<float>(config.tick_rate);
}

[[nodiscard]] float snapshot_dt(const DedicatedServerConfig& config) noexcept {
    return 1.0F / static_cast<float>(config.snapshot_rate);
}

[[nodiscard]] stellar::network::ServerWelcome accepted_welcome(
    const stellar::network::MapIdentity& map_identity) {
    return stellar::network::ServerWelcome{.accepted = true,
                                           .protocol_version = stellar::network::kCurrentProtocolVersion,
                                           .session_id = 1,
                                           .assigned_player_id = 1,
                                           .map_id = map_identity.map_id,
                                           .rejection_code = {},
                                           .message = "session accepted"};
}

[[nodiscard]] stellar::network::ServerWelcome rejected_welcome(
    const stellar::network::MapIdentity& map_identity,
    std::string code,
    std::string message) {
    return stellar::network::ServerWelcome{.accepted = false,
                                           .protocol_version = stellar::network::kCurrentProtocolVersion,
                                           .session_id = 0,
                                           .assigned_player_id = 0,
                                           .map_id = map_identity.map_id,
                                           .rejection_code = std::move(code),
                                           .message = std::move(message)};
}

[[nodiscard]] std::expected<void, stellar::network::TransportError> send_bytes(
    stellar::network::ServerTransport& transport,
    std::vector<std::uint8_t> bytes) {
    return transport.send_to_client(stellar::network::TransportPacket{
        .channel = stellar::network::TransportChannel::kReliable,
        .payload = stellar::network::to_payload(bytes),
    });
}

[[nodiscard]] bool parse_positive_int(std::string_view text, int& out) {
    if (text.empty()) {
        return false;
    }
    int value = 0;
    for (const char ch : text) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
        value = value * 10 + (ch - '0');
        if (value > 100000) {
            return false;
        }
    }
    out = value;
    return value > 0;
}

} // namespace

class DedicatedServer::Impl {
public:
    Impl(DedicatedServerConfig config,
         PreparedAuthority prepared,
         std::unique_ptr<stellar::network::ServerTransport> transport,
         std::string bound_endpoint)
        : config_(std::move(config)),
          prepared_(std::move(prepared)),
          transport_(std::move(transport)),
          bound_endpoint_(std::move(bound_endpoint)),
          latest_snapshot_(stellar::network::make_network_snapshot(snapshot())) {}

    [[nodiscard]] std::expected<DedicatedServerPumpResult, stellar::platform::Error> pump_once(
        float delta_seconds) noexcept {
        DedicatedServerPumpResult result{.session_state = session_state_};
        if (config_.validate_only) {
            return result;
        }
        if (!transport_) {
            return std::unexpected(error("Dedicated server transport is not initialized"));
        }

        for (const stellar::network::TransportPacket& packet : transport_->receive_from_client()) {
            const std::vector<std::uint8_t> bytes = stellar::network::from_payload(packet.payload);
            if (auto hello = stellar::network::decode_client_hello(bytes)) {
                handle_hello(*hello, result);
                continue;
            }

            auto command = stellar::network::decode_player_command(bytes);
            if (!command) {
                ++result.rejected_packets;
                continue;
            }
            if (session_state_ != stellar::network::SessionState::kConnected) {
                ++result.rejected_packets;
                continue;
            }
            command->player_id = config_.max_clients == 1 ? 1 : command->player_id;
            pending_command_ = *command;
        }

        result.session_state = session_state_;
        if (session_state_ != stellar::network::SessionState::kConnected) {
            return result;
        }

        if (std::isfinite(delta_seconds) && delta_seconds > 0.0F) {
            accumulated_seconds_ += delta_seconds;
            snapshot_seconds_ += delta_seconds;
        }

        std::vector<stellar::scripting::ScriptOutputEvent> script_events;
        std::vector<stellar::scripting::ScriptError> script_errors;
        std::vector<stellar::scripting::ScriptCommandResult> command_results;
        const float step = fixed_dt(config_);
        while (accumulated_seconds_ >= step && result.ticks_run < 8) {
            const std::array<stellar::server::PlayerCommand, 1> commands{
                stellar::server::PlayerCommand{.player_id = 1,
                                               .movement = pending_command_.movement}};
            if (auto* scripted = std::get_if<stellar::scripting::ScriptedWorldSession>(
                    &prepared_.session)) {
                stellar::scripting::ScriptedWorldFrame frame = scripted->tick(commands);
                script_events.insert(script_events.end(),
                                     std::make_move_iterator(frame.script_events.begin()),
                                     std::make_move_iterator(frame.script_events.end()));
                script_errors.insert(script_errors.end(),
                                     std::make_move_iterator(frame.script_errors.begin()),
                                     std::make_move_iterator(frame.script_errors.end()));
                command_results.insert(command_results.end(),
                                       std::make_move_iterator(frame.command_results.begin()),
                                       std::make_move_iterator(frame.command_results.end()));
            } else {
                [[maybe_unused]] const stellar::server::WorldSnapshot tick_snapshot =
                    std::get<stellar::server::WorldSession>(prepared_.session).tick(commands);
            }
            accumulated_seconds_ -= step;
            ++result.ticks_run;
        }
        if (accumulated_seconds_ >= step) {
            accumulated_seconds_ = 0.0F;
        }

        if (!send_snapshot_and_events_if_due(script_events, command_results, script_errors,
                                             result.rejected_packets)) {
            session_state_ = stellar::network::SessionState::kDisconnected;
            connected_clients_ = 0;
        }
        result.session_state = session_state_;
        return result;
    }

    [[nodiscard]] DedicatedServerStatus status() const noexcept {
        return DedicatedServerStatus{.running = true,
                                     .map_id = prepared_.map_identity.map_id,
                                     .tick = latest_snapshot_.tick,
                                     .connected_clients = connected_clients_,
                                     .bound_endpoint = bound_endpoint_};
    }

private:
    [[nodiscard]] stellar::server::WorldSnapshot snapshot() const {
        return std::visit([](const auto& session) { return session.snapshot(); }, prepared_.session);
    }

    void handle_hello(const stellar::network::ClientHello& hello,
                      DedicatedServerPumpResult& result) noexcept {
        stellar::network::ServerWelcome welcome;
        if (hello.protocol_version != stellar::network::kCurrentProtocolVersion) {
            welcome = rejected_welcome(prepared_.map_identity, "protocol_mismatch",
                                       "Client protocol version is unsupported");
            session_state_ = stellar::network::SessionState::kRejected;
            ++result.rejected_packets;
        } else if (hello.requested_map_id != prepared_.map_identity.map_id) {
            welcome = rejected_welcome(prepared_.map_identity, "map_mismatch",
                                       "Client requested map does not match server map");
            session_state_ = stellar::network::SessionState::kRejected;
            ++result.rejected_packets;
        } else {
            welcome = accepted_welcome(prepared_.map_identity);
            session_state_ = stellar::network::SessionState::kConnected;
            connected_clients_ = 1;
        }

        auto encoded = stellar::network::encode_server_welcome(welcome);
        if (!encoded || !send_bytes(*transport_, std::move(*encoded))) {
            ++result.rejected_packets;
        }
    }

    [[nodiscard]] bool send_snapshot_and_events_if_due(
        const std::vector<stellar::scripting::ScriptOutputEvent>& script_events,
        const std::vector<stellar::scripting::ScriptCommandResult>& command_results,
        const std::vector<stellar::scripting::ScriptError>& script_errors,
        std::uint32_t& rejected_packets) noexcept {
        const stellar::network::NetworkWorldSnapshot next_snapshot =
            stellar::network::make_network_snapshot(snapshot());
        const bool snapshot_due = !has_sent_snapshot_ || snapshot_seconds_ >= snapshot_dt(config_) ||
                                  next_snapshot.tick != latest_snapshot_.tick;
        if (snapshot_due) {
            std::expected<std::vector<std::uint8_t>, stellar::network::CodecError> encoded_state;
            if (has_sent_snapshot_) {
                encoded_state = stellar::network::encode_snapshot_delta(
                    stellar::network::make_snapshot_delta(latest_snapshot_, next_snapshot));
            } else {
                encoded_state = stellar::network::encode_snapshot(next_snapshot);
            }
            if (!encoded_state || !send_bytes(*transport_, std::move(*encoded_state))) {
                ++rejected_packets;
                return false;
            }
            latest_snapshot_ = next_snapshot;
            has_sent_snapshot_ = true;
            snapshot_seconds_ = 0.0F;
        }

        for (const stellar::network::GameplayEvent& event : stellar::network::make_gameplay_events(
                 latest_snapshot_.tick, script_events, command_results, script_errors)) {
            auto encoded_event = stellar::network::encode_gameplay_event(event);
            if (!encoded_event || !send_bytes(*transport_, std::move(*encoded_event))) {
                ++rejected_packets;
                return false;
            }
        }
        return true;
    }

    DedicatedServerConfig config_;
    PreparedAuthority prepared_;
    std::unique_ptr<stellar::network::ServerTransport> transport_;
    std::string bound_endpoint_;
    stellar::network::NetworkPlayerCommand pending_command_{.player_id = 1};
    stellar::network::NetworkWorldSnapshot latest_snapshot_;
    stellar::network::SessionState session_state_ = stellar::network::SessionState::kConnecting;
    std::uint32_t connected_clients_ = 0;
    float accumulated_seconds_ = 0.0F;
    float snapshot_seconds_ = 0.0F;
    bool has_sent_snapshot_ = false;
};

std::expected<DedicatedServerConfig, stellar::platform::Error> DedicatedServer::parse_cli(
    int argc,
    const char* const argv[]) {
    DedicatedServerConfig config;
    bool saw_map = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view arg = argv[index];
        auto require_value = [&](std::string_view name) -> std::expected<std::string_view, stellar::platform::Error> {
            if (index + 1 >= argc) {
                return std::unexpected(error(std::string(name) + " requires a value"));
            }
            return std::string_view(argv[++index]);
        };

        if (arg == "--validate-config") {
            config.validate_only = true;
        } else if (arg == "--map") {
            auto value = require_value(arg);
            if (!value) {
                return std::unexpected(value.error());
            }
            config.map_path = std::string(*value);
            saw_map = true;
        } else if (arg == "--script-root") {
            auto value = require_value(arg);
            if (!value) {
                return std::unexpected(value.error());
            }
            config.script_root = std::string(*value);
        } else if (arg == "--listen") {
            auto value = require_value(arg);
            if (!value) {
                return std::unexpected(value.error());
            }
            config.listen_endpoint = std::string(*value);
        } else if (arg == "--tick-rate") {
            auto value = require_value(arg);
            if (!value || !parse_positive_int(*value, config.tick_rate)) {
                return std::unexpected(error("--tick-rate requires a positive integer"));
            }
        } else if (arg == "--snapshot-rate") {
            auto value = require_value(arg);
            if (!value || !parse_positive_int(*value, config.snapshot_rate)) {
                return std::unexpected(error("--snapshot-rate requires a positive integer"));
            }
        } else if (arg == "--max-clients") {
            auto value = require_value(arg);
            if (!value || !parse_positive_int(*value, config.max_clients)) {
                return std::unexpected(error("--max-clients requires a positive integer"));
            }
        } else {
            return std::unexpected(error("Unknown stellar-server argument: " + std::string(arg)));
        }
    }
    if (!saw_map) {
        return std::unexpected(error("--map is required"));
    }
    if (auto valid = validate_config_values(config); !valid) {
        return std::unexpected(valid.error());
    }
    return config;
}

std::expected<DedicatedServer, stellar::platform::Error> DedicatedServer::create(
    DedicatedServerConfig config) {
    auto prepared = prepare_authority(config);
    if (!prepared) {
        return std::unexpected(prepared.error());
    }

    std::unique_ptr<stellar::network::ServerTransport> transport;
    std::string bound_endpoint;
    if (!config.validate_only) {
        auto handle = stellar::network::listen_tcp_server_once_with_bound_address(config.listen_endpoint);
        if (!handle) {
            return std::unexpected(error("Failed to listen on --listen endpoint: " +
                                         handle.error().message));
        }
        bound_endpoint = stellar::network::format_socket_address(handle->bound_address);
        transport = std::move(handle->server);
    }

    return DedicatedServer(std::make_unique<Impl>(std::move(config), std::move(*prepared),
                                                  std::move(transport), std::move(bound_endpoint)));
}

DedicatedServer::DedicatedServer(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
DedicatedServer::DedicatedServer(DedicatedServer&& other) noexcept = default;
DedicatedServer& DedicatedServer::operator=(DedicatedServer&& other) noexcept = default;
DedicatedServer::~DedicatedServer() = default;

std::expected<void, stellar::platform::Error> DedicatedServer::run() {
    while (true) {
        auto pumped = pump_once(1.0F / 60.0F);
        if (!pumped) {
            return std::unexpected(pumped.error());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::expected<DedicatedServerPumpResult, stellar::platform::Error> DedicatedServer::pump_once(
    float delta_seconds) noexcept {
    if (!impl_) {
        return std::unexpected(error("Dedicated server is not initialized"));
    }
    return impl_->pump_once(delta_seconds);
}

DedicatedServerStatus DedicatedServer::status() const noexcept {
    if (!impl_) {
        return {};
    }
    return impl_->status();
}

} // namespace stellar::server
