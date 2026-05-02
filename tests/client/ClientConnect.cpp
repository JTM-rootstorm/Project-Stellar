#include "stellar/client/Application.hpp"
#include "stellar/client/NetworkedClientRuntime.hpp"
#include "stellar/client/PlayerPresentation.hpp"
#include "stellar/network/SnapshotCodec.hpp"
#include "stellar/network/Transport.hpp"
#include "stellar/server/DedicatedServer.hpp"

#include "BspFixture.hpp"

#include <SDL2/SDL.h>

#include <cassert>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

stellar::network::TransportPacket reliable_packet(std::vector<std::uint8_t> bytes) {
    return stellar::network::TransportPacket{
        .channel = stellar::network::TransportChannel::kReliable,
        .payload = stellar::network::to_payload(bytes),
    };
}

class FakeClientTransport final : public stellar::network::ClientTransport {
public:
    std::expected<void, stellar::network::TransportError> send_to_server(
        stellar::network::TransportPacket packet) override {
        sent_to_server.push_back(std::move(packet));
        return {};
    }

    std::vector<stellar::network::TransportPacket> receive_from_server() override {
        std::vector<stellar::network::TransportPacket> out;
        out.swap(pending_from_server);
        return out;
    }

    std::vector<stellar::network::TransportPacket> sent_to_server;
    std::vector<stellar::network::TransportPacket> pending_from_server;
};

std::unique_ptr<FakeClientTransport> fake_transport() {
    return std::make_unique<FakeClientTransport>();
}

stellar::network::ServerWelcome accepted_welcome() {
    return stellar::network::ServerWelcome{.accepted = true,
                                           .protocol_version = stellar::network::kCurrentProtocolVersion,
                                           .session_id = 1,
                                           .assigned_player_id = 42,
                                           .map_id = "server-map",
                                           .rejection_code = {},
                                           .message = "accepted"};
}

stellar::network::NetworkWorldSnapshot snapshot_for_player(stellar::server::PlayerId player_id) {
    stellar::network::NetworkWorldSnapshot snapshot{};
    snapshot.tick = 9;
    stellar::server::PlayerSnapshot player{};
    player.player_id = player_id;
    player.position = {1.0F, 2.0F, 3.0F};
    snapshot.players.push_back(player);
    return snapshot;
}

stellar::network::GameplayEvent pickup_event() {
    return stellar::network::GameplayEvent{.kind = stellar::network::GameplayEventKind::kPickupCollected,
                                           .tick = 9,
                                           .entity_id = 5,
                                           .player_id = 42,
                                           .code = "pickup",
                                           .message = "Picked up test item"};
}

void cli_parse_accepts_connect_and_rejects_conflicts() {
    const char* connect[] = {"stellar-client", "--validate-config", "--connect", "127.0.0.1:29070",
                             "--client-name", "Mike"};
    auto parsed = stellar::client::parse_application_config(6, connect);
    assert(parsed.has_value());
    assert(parsed->validate_only);
    assert(parsed->connect_endpoint == "127.0.0.1:29070");
    assert(parsed->client_name == "Mike");

    const char* ambiguous[] = {"stellar-client", "--connect", "127.0.0.1:29070", "--map",
                               "room.bsp"};
    assert(!stellar::client::parse_application_config(5, ambiguous));

    const char* bad_endpoint[] = {"stellar-client", "--connect", "not-a-valid-endpoint"};
    assert(!stellar::client::parse_application_config(3, bad_endpoint));

    const char* script_root[] = {"stellar-client", "--connect", "127.0.0.1:29070",
                                 "--script-root", "scripts"};
    assert(!stellar::client::parse_application_config(5, script_root));
}

void validate_connect_mode_constructs_no_local_authority() {
    stellar::client::ApplicationConfig config;
    config.validate_only = true;
    config.connect_endpoint = "127.0.0.1:29070";
    auto prepared = stellar::client::prepare_application_runtime(config);
    assert(prepared.has_value());
    assert(!prepared->runtime_world);
    assert(!prepared->networked_runtime);
    assert(!prepared->local_loopback_runtime);
    assert(!prepared->remote_runtime);
    assert(!prepared->validation->level.has_value());
    assert(!prepared->validation->scripted_runtime_enabled);
    assert(prepared->validation->loaded_script_ids.empty());
}

void remote_runtime_sends_hello_and_gates_input_until_welcome() {
    auto transport = fake_transport();
    FakeClientTransport* raw = transport.get();
    stellar::client::RemoteClientRuntimeConfig config;
    config.client_name = "remote-test";
    stellar::client::RemoteClientRuntime runtime(std::move(transport), config);

    assert(raw->sent_to_server.size() == 1);
    auto hello = stellar::network::decode_client_hello(
        stellar::network::from_payload(raw->sent_to_server[0].payload));
    assert(hello.has_value());
    assert(hello->client_name == "remote-test");
    assert(hello->requested_map_id.empty());

    stellar::platform::Input input;
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.type = SDL_KEYDOWN;
    event.key.keysym.scancode = SDL_SCANCODE_W;
    input.process_event(event);
    auto before_welcome = runtime.update(input, 0.0F);
    assert(before_welcome.session_state == stellar::network::SessionState::kConnecting);
    assert(raw->sent_to_server.size() == 1);

    auto welcome_bytes = stellar::network::encode_server_welcome(accepted_welcome());
    assert(welcome_bytes.has_value());
    raw->pending_from_server.push_back(reliable_packet(*welcome_bytes));
    auto after_welcome = runtime.update(input, 0.0F);
    assert(after_welcome.session_state == stellar::network::SessionState::kConnected);
    assert(runtime.local_player_id() == 42);
    assert(raw->sent_to_server.size() == 2);
    auto command = stellar::network::decode_player_command(
        stellar::network::from_payload(raw->sent_to_server[1].payload));
    assert(command.has_value());
    assert(command->player_id == 42);
    assert(command->movement.wish_direction[0] == 0.0F);
    assert(command->movement.wish_direction[1] == 1.0F);
    assert(command->movement.wish_direction[2] == 0.0F);
}

void remote_runtime_exposes_snapshot_player_and_events() {
    auto transport = fake_transport();
    FakeClientTransport* raw = transport.get();
    stellar::client::RemoteClientRuntime runtime(std::move(transport), {});

    auto welcome_bytes = stellar::network::encode_server_welcome(accepted_welcome());
    auto snapshot_bytes = stellar::network::encode_snapshot(snapshot_for_player(42));
    auto event_bytes = stellar::network::encode_gameplay_event(pickup_event());
    assert(welcome_bytes.has_value());
    assert(snapshot_bytes.has_value());
    assert(event_bytes.has_value());
    raw->pending_from_server.push_back(reliable_packet(*welcome_bytes));
    raw->pending_from_server.push_back(reliable_packet(*snapshot_bytes));
    raw->pending_from_server.push_back(reliable_packet(*event_bytes));

    stellar::platform::Input input;
    const auto frame = runtime.update(input, 0.0F);
    assert(frame.session_state == stellar::network::SessionState::kConnected);
    assert(runtime.latest_snapshot().has_value());
    assert(runtime.latest_snapshot()->tick == 9);
    const auto presentation = stellar::client::make_player_presentation_state(
        *runtime.latest_snapshot(), runtime.local_player_id());
    assert(presentation.has_value());
    assert(presentation->position[0] == 1.0F);
    assert(presentation->position[1] == 2.0F);
    assert(presentation->position[2] == 3.0F);
    assert(frame.events.size() == 1);
    assert(frame.events[0].kind == stellar::network::GameplayEventKind::kPickupCollected);
}

std::filesystem::path temp_root() {
    const std::filesystem::path root = std::filesystem::temp_directory_path() /
                                      "stellar_client_connect_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void localhost_remote_runtime_integrates_with_dedicated_server() {
    const std::filesystem::path root = temp_root();
    const std::filesystem::path map = root / "room.bsp";
    stellar::tests::fixtures::write_bsp_fixture(map, "gameplay_room");

    stellar::server::DedicatedServerConfig server_config;
    server_config.map_path = map.string();
    server_config.listen_endpoint = "127.0.0.1:0";
    server_config.snapshot_rate = 60;
    auto server = stellar::server::DedicatedServer::create(server_config);
    assert(server.has_value());

    stellar::client::RemoteClientRuntimeConfig client_config;
    client_config.connect_endpoint = server->status().bound_endpoint;
    client_config.client_name = "localhost-test";
    auto remote = stellar::client::RemoteClientRuntime::connect(std::move(client_config));
    assert(remote.has_value());

    stellar::platform::Input input;
    for (int attempt = 0; attempt < 64 && !remote->latest_snapshot().has_value(); ++attempt) {
        auto pump = server->pump_once(1.0F / 60.0F);
        assert(pump.has_value());
        [[maybe_unused]] const auto frame = remote->update(input, 1.0F / 60.0F);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    assert(remote->session_state() == stellar::network::SessionState::kConnected);
    assert(remote->local_player_id() == 1);
    assert(remote->latest_snapshot().has_value());
    assert(!remote->latest_snapshot()->players.empty());
}

} // namespace

int main() {
    cli_parse_accepts_connect_and_rejects_conflicts();
    validate_connect_mode_constructs_no_local_authority();
    remote_runtime_sends_hello_and_gates_input_until_welcome();
    remote_runtime_exposes_snapshot_player_and_events();
    localhost_remote_runtime_integrates_with_dedicated_server();
    return 0;
}
