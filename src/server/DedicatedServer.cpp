#include "stellar/server/DedicatedServer.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

#include "stellar/authority/AuthorityBootstrap.hpp"
#include "stellar/network/Messages.hpp"
#include "stellar/network/SocketTransport.hpp"
#include "stellar/network/Transport.hpp"
#include "stellar/server/ServerRuntime.hpp"

namespace stellar::server {
namespace {

[[nodiscard]] stellar::platform::Error error(std::string message) {
    return stellar::platform::Error(std::move(message));
}

[[nodiscard]] std::expected<void, stellar::platform::Error> validate_config_values(
    const DedicatedServerConfig& config) {
    if (config.map_path.empty()) {
        return std::unexpected(error("--map is required"));
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
         stellar::authority::PreparedAuthority prepared,
         std::unique_ptr<stellar::network::ServerTransport> transport,
         std::string bound_endpoint)
        : config_(std::move(config)),
          authority_(std::move(prepared)),
          map_identity_(authority_.map_identity),
          runtime_(std::move(authority_.session), runtime_config(config_, map_identity_)),
          transport_(std::move(transport)),
          bound_endpoint_(std::move(bound_endpoint)) {}

    [[nodiscard]] std::expected<DedicatedServerPumpResult, stellar::platform::Error> pump_once(
        float delta_seconds) noexcept {
        DedicatedServerPumpResult result{.session_state = runtime_.session_state()};
        if (config_.validate_only) {
            return result;
        }
        if (!transport_) {
            return std::unexpected(error("Dedicated server transport is not initialized"));
        }

        const stellar::server::ServerRuntimePumpResult pumped = runtime_.pump(*transport_,
                                                                              delta_seconds);
        result.ticks_run = pumped.ticks_run;
        result.rejected_packets = pumped.rejected_packets;
        result.session_state = pumped.session_state;
        return result;
    }

    [[nodiscard]] DedicatedServerStatus status() const noexcept {
        return DedicatedServerStatus{.running = true,
                                     .map_id = map_identity_.map_id,
                                     .tick = runtime_.latest_snapshot().tick,
                                     .connected_clients = runtime_.connected_clients(),
                                     .bound_endpoint = bound_endpoint_};
    }

private:
    [[nodiscard]] static stellar::server::ServerRuntimeConfig runtime_config(
        const DedicatedServerConfig& config,
        const stellar::network::MapIdentity& map_identity) noexcept {
        stellar::server::ServerRuntimeConfig runtime_config;
        runtime_config.session.local_player_id = 1;
        runtime_config.session.movement.fixed_dt = 1.0F / static_cast<float>(config.tick_rate);
        runtime_config.max_ticks_per_pump = 8;
        runtime_config.snapshot_rate = config.snapshot_rate;
        runtime_config.emit_deltas = true;
        runtime_config.map_identity = map_identity;
        return runtime_config;
    }

    DedicatedServerConfig config_;
    stellar::authority::PreparedAuthority authority_;
    stellar::network::MapIdentity map_identity_;
    stellar::server::ServerRuntime runtime_;
    std::unique_ptr<stellar::network::ServerTransport> transport_;
    std::string bound_endpoint_;
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
    stellar::authority::AuthorityLoadConfig load_config;
    load_config.map_path = config.map_path;
    load_config.session_config.local_player_id = 1;
    load_config.session_config.movement.fixed_dt = 1.0F / static_cast<float>(config.tick_rate);
    if (config.script_root.has_value()) {
        load_config.script_root = *config.script_root;
    }
    auto prepared = stellar::authority::prepare_authority(load_config);
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
