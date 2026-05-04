#include "stellar/client/Application.hpp"
#include "stellar/client/ListenHostRuntime.hpp"

#include "../fixtures/BspFixture.hpp"

#include <SDL2/SDL.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>

namespace {

void write_text_file(const std::filesystem::path& path, const std::string& source) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(source.data(), static_cast<std::streamsize>(source.size()));
}

void synthesize_key(stellar::platform::Input& input, SDL_Scancode scancode, Uint32 type) {
    SDL_Event event{};
    event.type = type;
    event.key.type = type;
    event.key.keysym.scancode = scancode;
    input.process_event(event);
}

stellar::platform::Input input_with_key(SDL_Scancode scancode) {
    stellar::platform::Input input;
    synthesize_key(input, scancode, SDL_KEYDOWN);
    return input;
}

void assert_parse_fails(std::initializer_list<const char*> args, const std::string& expected) {
    const auto parsed = stellar::client::parse_application_config(
        static_cast<int>(args.size()), args.begin());
    assert(!parsed.has_value());
    assert(parsed.error().message.find(expected) != std::string::npos);
}

void cli_conflicts_are_enforced(const std::filesystem::path& root) {
    assert_parse_fails({"stellar-client", "--host"}, "--host requires --map");
    assert_parse_fails({"stellar-client", "--host", "--connect", "127.0.0.1:29070"},
                       "--host conflicts with --connect");
    assert_parse_fails({"stellar-client", "--listen", "127.0.0.1:29070"},
                       "--listen applies only to --host");

    const auto bsp_path = root / "scripted_map.bsp";
    stellar::tests::fixtures::write_bsp_fixture(bsp_path, "scripted_interaction_zup");
    write_text_file(root / "scripts" / "gate.lua",
                    "Door = {}\n"
                    "function Door.on_trigger_enter(event)\n"
                    "  stellar.emit_event('collision.set_mesh_enabled', "
                    "{mesh = 'GateDoor', enabled = false})\n"
                    "end\n");
    write_text_file(root / "scripts" / "pickup.lua",
                    "PickupGem = {}\n"
                    "function PickupGem.on_object_collider_enter(event)\n"
                    "  stellar.emit_event('gameplay.collect_pickup', "
                    "{name = event.collider_name})\n"
                    "end\n");

    const std::string map = bsp_path.string();
    const std::string script_root = root.string();
    const char* argv[] = {"stellar-client", "--host", "--map", map.c_str(),
                          "--listen", "127.0.0.1:0", "--script-root",
                          script_root.c_str(), "--validate-config"};
    const auto parsed = stellar::client::parse_application_config(9, argv);
    assert(parsed.has_value());
    assert(parsed->host);
    assert(parsed->map_path == map);
    assert(parsed->listen_endpoint == "127.0.0.1:0");
    assert(parsed->script_root == script_root);

    const auto prepared = stellar::client::prepare_application_runtime(*parsed);
    assert(prepared.has_value());
    assert(prepared->validation != nullptr);
    assert(prepared->validation->scripted_runtime_enabled);
    assert(prepared->validation->loaded_script_ids.size() == 2);
}

void listen_host_setup_snapshot_movement_and_status(const std::filesystem::path& root) {
    const auto bsp_path = root / "host_map.bsp";
    stellar::tests::fixtures::write_bsp_fixture(bsp_path, "minimal_zup_room");

    stellar::client::ApplicationConfig config;
    config.host = true;
    config.map_path = bsp_path.string();
    config.listen_endpoint = "127.0.0.1:0";

    auto prepared = stellar::client::prepare_application_runtime(config);
    assert(prepared.has_value());
    assert(prepared->listen_host_runtime != nullptr);
    assert(prepared->single_player_runtime == nullptr);
    assert(prepared->active_client_runtime == prepared->listen_host_runtime.get());
    assert(prepared->active_client_runtime->mode() == stellar::client::ClientRuntimeMode::kListenHost);

    stellar::client::ListenHostRuntime& runtime = *prepared->listen_host_runtime;
    stellar::platform::Input neutral_input;
    const auto first = runtime.update(neutral_input, 0.0F);
    assert(first.rejected_packets == 0);
    assert(first.session_state == stellar::network::SessionState::kConnected);
    assert(first.snapshot.has_value());
    assert(first.snapshot->players.size() == 1);
    assert(first.local_player_id == 1);

    const auto initial_y = first.snapshot->players[0].position[1];
    const auto moved = runtime.update(input_with_key(SDL_SCANCODE_W), 0.1F);
    assert(moved.session_state == stellar::network::SessionState::kConnected);
    assert(moved.ticks_run > 0);
    assert(moved.snapshot.has_value());
    assert(moved.snapshot->players[0].position[1] > initial_y);

    const auto status = runtime.status();
    assert(status.running);
    assert(!status.bound_endpoint.empty());
    assert(status.bound_endpoint.rfind("127.0.0.1:", 0) == 0);
    assert(status.accepted_clients == 1);
    assert(status.session_state == stellar::network::SessionState::kConnected);
    assert(!status.map_id.empty());
    assert(status.tick == moved.snapshot->tick);
    assert(status.host_player_uses_loopback_slot);
    assert(status.remote_clients_deferred);
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "stellar_listen_server_host_test";
    std::filesystem::create_directories(root);
    cli_conflicts_are_enforced(root);
    listen_host_setup_snapshot_movement_and_status(root);
    return 0;
}
