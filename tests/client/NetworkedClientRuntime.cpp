#include "stellar/client/NetworkedClientRuntime.hpp"
#include "stellar/network/Transport.hpp"
#include "stellar/scripting/ScriptRegistry.hpp"
#include "stellar/scripting/ScriptedWorldSession.hpp"
#include "stellar/world/RuntimeWorld.hpp"

#include <SDL2/SDL.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <utility>

namespace {

using Vec3 = std::array<float, 3>;

stellar::assets::WorldMarker player_spawn(Vec3 position) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kPlayerSpawn;
    marker.name = "SPAWN_Player";
    marker.position = position;
    return marker;
}

stellar::assets::WorldMarker trigger_marker(std::string name,
                                            Vec3 position,
                                            Vec3 scale,
                                            std::string script_id,
                                            std::string table_name) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kTrigger;
    marker.name = std::move(name);
    marker.position = position;
    marker.scale = scale;
    marker.script = stellar::assets::WorldScriptBinding{std::move(script_id),
                                                        std::move(table_name)};
    return marker;
}

stellar::assets::CollisionTriangle triangle(Vec3 a, Vec3 b, Vec3 c, Vec3 normal) {
    return stellar::assets::CollisionTriangle{.a = a, .b = b, .c = c, .normal = normal};
}

stellar::assets::LevelAsset scene_with_markers(
    std::initializer_list<stellar::assets::WorldMarker> markers) {
    stellar::assets::LevelAsset scene{};
    scene.world_metadata.markers.assign(markers.begin(), markers.end());
    return scene;
}

stellar::assets::LevelAsset scene_with_trigger_and_wall() {
    auto scene = scene_with_markers({
        player_spawn({0.0F, 0.5F, 0.0F}),
        trigger_marker("DoorOpen", {0.0F, 0.5F, 0.0F}, {0.5F, 0.5F, 0.5F}, "door", "Door"),
    });
    stellar::assets::CollisionMesh wall;
    wall.name = "DoorBlocker";
    wall.triangles = {
        triangle({0.8F, -2.0F, -4.0F}, {0.8F, 4.0F, 4.0F}, {0.8F, -2.0F, 4.0F},
                 {-1.0F, 0.0F, 0.0F}),
        triangle({0.8F, -2.0F, -4.0F}, {0.8F, 4.0F, -4.0F}, {0.8F, 4.0F, 4.0F},
                 {-1.0F, 0.0F, 0.0F}),
    };
    scene.level_collision = stellar::assets::LevelCollisionAsset{.meshes = {wall}};
    return scene;
}

stellar::client::NetworkedClientRuntimeConfig test_config() {
    stellar::client::NetworkedClientRuntimeConfig config{};
    config.bridge.session.local_player_id = 7;
    config.bridge.session.movement.max_speed = 10.0F;
    config.bridge.session.movement.acceleration = 100.0F;
    config.bridge.session.movement.gravity = 0.0F;
    config.bridge.session.movement.terminal_fall_speed = 50.0F;
    config.bridge.session.movement.fixed_dt = 0.1F;
    config.bridge.session.movement.character.radius = 0.25F;
    config.bridge.session.movement.character.height = 1.0F;
    config.bridge.session.movement.character.skin_width = 0.0F;
    config.bridge.session.movement.character.ground_snap_distance = 0.0F;
    config.bridge.session.movement.character.max_slide_iterations = 4;
    config.bridge.max_ticks_per_pump = 4;
    return config;
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

stellar::network::TransportPacket malformed_packet() {
    return stellar::network::TransportPacket{
        .channel = stellar::network::TransportChannel::kReliable,
        .payload = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}},
    };
}

void local_runtime_emits_first_snapshot_after_frame() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::NetworkedClientRuntime runtime(world, test_config());
    stellar::platform::Input input;

    const auto frame = runtime.update(input, 0.1F);

    assert(frame.ticks_run == 1);
    assert(frame.rejected_packets == 0);
    assert(runtime.latest_snapshot().has_value());
    assert(runtime.latest_snapshot()->tick == 1);
    assert(runtime.latest_snapshot()->players[0].player_id == 7);
}

void movement_input_changes_authoritative_network_snapshot() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::NetworkedClientRuntime runtime(world, test_config());

    const auto frame = runtime.update(input_with_key(SDL_SCANCODE_D), 0.1F);

    assert(frame.ticks_run == 1);
    assert(runtime.latest_snapshot().has_value());
    assert(runtime.latest_snapshot()->players[0].position[0] > 0.9F);
}

void script_events_queue_through_receiver() {
    const auto scene = scene_with_trigger_and_wall();
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::scripting::ScriptRegistry registry;
    registry.set_script("door",
                        "Door = {}\n"
                        "function Door.on_trigger_enter(event)\n"
                        "  stellar.emit_event('collision.set_mesh_enabled', "
                        "{mesh = 'DoorBlocker', enabled = false})\n"
                        "end\n");
    auto scripted = stellar::scripting::ScriptedWorldSession::create(
        world, test_config().bridge.session, std::move(registry));
    assert(scripted.has_value());
    stellar::client::NetworkedClientRuntime runtime(std::move(*scripted), test_config());
    stellar::platform::Input input;

    const auto frame = runtime.update(input, 0.1F);

    assert(frame.events.size() == 1);
    assert(frame.events[0].kind == stellar::network::GameplayEventKind::kDoorStateChanged);
    assert(frame.events[0].code == "DoorBlocker");
}

void malformed_packets_are_rejected_without_crash() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::NetworkedClientRuntime runtime(world, test_config());

    const auto result = runtime.accept_authoritative_packet_for_test(malformed_packet());

    assert(result.rejected_packets == 1);
    assert(!runtime.latest_snapshot().has_value());
}

void command_sequence_increments_deterministically() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::NetworkedClientRuntime runtime(world, test_config());
    stellar::platform::Input input;

    assert(runtime.next_command_sequence() == 1);
    [[maybe_unused]] const auto first = runtime.update(input, 0.0F);
    assert(runtime.next_command_sequence() == 2);
    [[maybe_unused]] const auto second = runtime.update(input, 0.0F);
    assert(runtime.next_command_sequence() == 3);
}

} // namespace

int main() {
    local_runtime_emits_first_snapshot_after_frame();
    movement_input_changes_authoritative_network_snapshot();
    script_events_queue_through_receiver();
    malformed_packets_are_rejected_without_crash();
    command_sequence_increments_deterministically();
    return 0;
}
