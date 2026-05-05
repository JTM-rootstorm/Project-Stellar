#include "stellar/client/SinglePlayerRuntime.hpp"

#include "stellar/assets/LevelAsset.hpp"
#include "stellar/authority/NetworkConversion.hpp"
#include "stellar/scripting/ScriptRegistry.hpp"
#include "stellar/scripting/ScriptedWorldSession.hpp"
#include "stellar/world/RuntimeWorld.hpp"

#include <SDL2/SDL.h>

#include <array>
#include <cassert>
#include <initializer_list>
#include <memory>
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

stellar::assets::WorldMarker object_collider_marker(std::string name,
                                                    Vec3 position,
                                                    Vec3 scale,
                                                    std::string archetype) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kObjectCollider;
    marker.name = std::move(name);
    marker.position = position;
    marker.scale = scale;
    marker.archetype = std::move(archetype);
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

stellar::assets::LevelAsset scene_with_trigger_wall_and_pickup() {
    auto scene = scene_with_markers({
        player_spawn({0.0F, 0.5F, 0.0F}),
        trigger_marker("DoorOpen", {0.0F, 0.5F, 0.0F}, {0.5F, 0.5F, 0.5F}, "door", "Door"),
        object_collider_marker("Gem", {0.0F, 0.5F, 0.0F}, {1.0F, 1.0F, 1.0F}, "pickup"),
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

stellar::assets::LevelAsset scene_with_footstep_floor() {
    auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.55F})});
    auto floor_a = triangle({-10.0F, -10.0F, 0.0F}, {10.0F, -10.0F, 0.0F},
                            {10.0F, 10.0F, 0.0F}, {0.0F, 0.0F, 1.0F});
    auto floor_b = triangle({-10.0F, -10.0F, 0.0F}, {10.0F, 10.0F, 0.0F},
                            {-10.0F, 10.0F, 0.0F}, {0.0F, 0.0F, 1.0F});
    floor_a.surface.source_material_name = "metal/grate01";
    floor_a.surface.footstep_surface_id = "metal";
    floor_b.surface = floor_a.surface;
    stellar::assets::CollisionMesh floor;
    floor.name = "Floor";
    floor.triangles = {floor_a, floor_b};
    scene.level_collision = stellar::assets::LevelCollisionAsset{.meshes = {floor}};
    return scene;
}

stellar::server::WorldSessionConfig test_session_config() {
    stellar::server::WorldSessionConfig config{};
    config.local_player_id = 7;
    config.movement.max_speed = 10.0F;
    config.movement.acceleration = 100.0F;
    config.movement.gravity = 0.0F;
    config.movement.terminal_fall_speed = 50.0F;
    config.movement.fixed_dt = 0.1F;
    config.movement.character.radius = 0.25F;
    config.movement.character.height = 1.0F;
    config.movement.character.skin_width = 0.0F;
    config.movement.character.ground_snap_distance = 0.0F;
    config.movement.character.max_slide_iterations = 4;
    return config;
}

stellar::server::WorldSessionConfig footstep_session_config() {
    stellar::server::WorldSessionConfig config = test_session_config();
    config.movement.gravity = 24.0F;
    config.movement.character.ground_snap_distance = 0.2F;
    config.footsteps.min_horizontal_speed = 0.0F;
    config.footsteps.walk_step_distance = 0.01F;
    config.footsteps.run_step_distance = 0.01F;
    return config;
}

stellar::client::SinglePlayerRuntimeConfig test_runtime_config() {
    stellar::client::SinglePlayerRuntimeConfig config{};
    config.max_ticks_per_update = 4;
    config.fixed_dt_seconds = 0.1F;
    return config;
}

stellar::authority::PreparedAuthority plain_authority(
    stellar::assets::LevelAsset scene,
    stellar::server::WorldSessionConfig session_config = test_session_config()) {
    auto level = std::make_unique<stellar::assets::LevelAsset>(std::move(scene));
    auto world = std::make_unique<stellar::world::RuntimeWorld>(
        stellar::world::build_runtime_world(*level));
    const stellar::network::MapIdentity map_identity = stellar::authority::make_map_identity(*world);
    stellar::server::WorldSession session(*world, session_config);
    return stellar::authority::PreparedAuthority{
        .level = std::move(level),
        .world = std::move(world),
        .map_identity = map_identity,
        .session = std::move(session),
    };
}

stellar::authority::PreparedAuthority scripted_authority(stellar::assets::LevelAsset scene) {
    auto level = std::make_unique<stellar::assets::LevelAsset>(std::move(scene));
    auto world = std::make_unique<stellar::world::RuntimeWorld>(
        stellar::world::build_runtime_world(*level));
    const stellar::network::MapIdentity map_identity = stellar::authority::make_map_identity(*world);
    stellar::scripting::ScriptRegistry registry;
    registry.set_script("door",
                        "Door = {}\n"
                        "function Door.on_trigger_enter(event)\n"
                        "  stellar.emit_event('collision.set_mesh_enabled', "
                        "{mesh = 'DoorBlocker', enabled = false})\n"
                        "end\n");
    auto scripted = stellar::scripting::ScriptedWorldSession::create(
        *world, test_session_config(), std::move(registry));
    assert(scripted.has_value());
    return stellar::authority::PreparedAuthority{
        .level = std::move(level),
        .world = std::move(world),
        .map_identity = map_identity,
        .session = std::move(*scripted),
        .scripted_runtime_enabled = true,
    };
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

void first_frame_emits_initial_snapshot_without_tick() {
    stellar::client::SinglePlayerRuntime runtime(
        plain_authority(scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})})),
        test_runtime_config());
    stellar::platform::Input input;

    const auto frame = runtime.update(input, 0.0F);

    assert(runtime.mode() == stellar::client::ClientRuntimeMode::kSinglePlayer);
    assert(frame.ticks_run == 0);
    assert(frame.snapshot.has_value());
    assert(frame.snapshot->tick == 0);
    assert(frame.snapshot->players.size() == 1);
    assert(frame.local_player_id == 7);
    assert(frame.session_state == stellar::network::SessionState::kConnected);
}

void movement_input_changes_player_snapshot() {
    stellar::client::SinglePlayerRuntime runtime(
        plain_authority(scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})})),
        test_runtime_config());

    const auto frame = runtime.update(input_with_key(SDL_SCANCODE_W), 0.1F);

    assert(frame.ticks_run == 1);
    assert(frame.snapshot.has_value());
    assert(frame.snapshot->players[0].position[1] > 0.9F);
}

void footstep_events_surface_as_presentation_events() {
    stellar::client::SinglePlayerRuntime runtime(
        plain_authority(scene_with_footstep_floor(), footstep_session_config()),
        test_runtime_config());

    const auto frame = runtime.update(input_with_key(SDL_SCANCODE_W), 0.3F);

    bool saw_footstep = false;
    for (const auto& event : frame.events) {
        if (event.kind == stellar::network::GameplayEventKind::kFootstep &&
            event.code == "metal" && event.message == "metal/grate01") {
            saw_footstep = true;
        }
    }
    assert(saw_footstep);
}

void scripted_events_surface_as_presentation_events() {
    stellar::client::SinglePlayerRuntime runtime(
        scripted_authority(scene_with_trigger_wall_and_pickup()), test_runtime_config());
    stellar::platform::Input input;

    const auto frame = runtime.update(input, 0.1F);

    bool saw_door = false;
    bool saw_pickup = false;
    for (const auto& event : frame.events) {
        if (event.kind == stellar::network::GameplayEventKind::kDoorStateChanged &&
            event.code == "DoorBlocker") {
            saw_door = true;
        }
        if (event.kind == stellar::network::GameplayEventKind::kPickupCollected &&
            event.code == "collected") {
            saw_pickup = true;
        }
    }
    assert(saw_door);
    assert(saw_pickup);
}

void no_handshake_or_welcome_diagnostics_are_generated() {
    stellar::client::SinglePlayerRuntime runtime(
        plain_authority(scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})})),
        test_runtime_config());
    stellar::platform::Input input;

    const auto frame = runtime.update(input, 0.1F);

    assert(frame.rejected_packets == 0);
    assert(frame.diagnostics.empty());
}

} // namespace

int main() {
    first_frame_emits_initial_snapshot_without_tick();
    movement_input_changes_player_snapshot();
    footstep_events_surface_as_presentation_events();
    scripted_events_surface_as_presentation_events();
    no_handshake_or_welcome_diagnostics_are_generated();
    return 0;
}
