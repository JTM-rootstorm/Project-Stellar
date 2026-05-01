#include "stellar/client/LocalLoopbackRuntime.hpp"

#include <SDL2/SDL.h>

#include <array>
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace {

using Vec3 = std::array<float, 3>;

constexpr float kTolerance = 2.0e-2F;

bool nearly_equal(float a, float b, float tolerance = kTolerance) {
    return std::abs(a - b) <= tolerance;
}

bool nearly_equal(Vec3 a, Vec3 b, float tolerance = kTolerance) {
    return nearly_equal(a[0], b[0], tolerance) && nearly_equal(a[1], b[1], tolerance) &&
           nearly_equal(a[2], b[2], tolerance);
}

stellar::assets::CollisionTriangle triangle(Vec3 a, Vec3 b, Vec3 c, Vec3 normal) {
    return stellar::assets::CollisionTriangle{.a = a, .b = b, .c = c, .normal = normal};
}

stellar::assets::CollisionTriangle wall_x_triangle_a(float x = 0.8F) {
    return triangle({x, -2.0F, -4.0F}, {x, 4.0F, 4.0F}, {x, -2.0F, 4.0F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::CollisionTriangle wall_x_triangle_b(float x = 0.8F) {
    return triangle({x, -2.0F, -4.0F}, {x, 4.0F, -4.0F}, {x, 4.0F, 4.0F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::WorldMarker player_spawn(Vec3 position) {
    stellar::assets::WorldMarker marker;
    marker.type = stellar::assets::WorldMarkerType::kPlayerSpawn;
    marker.name = "SPAWN_Player";
    marker.position = position;
    return marker;
}

stellar::assets::WorldMarker trigger_marker(std::string name, Vec3 position, Vec3 scale) {
    stellar::assets::WorldMarker marker;
    marker.type = stellar::assets::WorldMarkerType::kTrigger;
    marker.name = std::move(name);
    marker.position = position;
    marker.scale = scale;
    return marker;
}

stellar::assets::LevelAsset scene_with_markers(
    std::initializer_list<stellar::assets::WorldMarker> markers) {
    stellar::assets::LevelAsset scene;
    scene.world_metadata.markers.assign(markers.begin(), markers.end());
    return scene;
}

stellar::assets::LevelAsset scene_with_collision_and_markers(
    std::initializer_list<stellar::assets::CollisionTriangle> triangles,
    std::initializer_list<stellar::assets::WorldMarker> markers) {
    stellar::assets::LevelAsset scene = scene_with_markers(markers);
    stellar::assets::CollisionMesh mesh;
    mesh.name = "collision";
    mesh.triangles.assign(triangles.begin(), triangles.end());
    scene.level_collision = stellar::assets::LevelCollisionAsset{.meshes = {mesh}};
    return scene;
}

stellar::client::LocalLoopbackRuntimeConfig test_runtime_config(
    stellar::server::PlayerId player_id = 1) {
    stellar::client::LocalLoopbackRuntimeConfig config;
    config.session.local_player_id = player_id;
    config.session.movement.max_speed = 10.0F;
    config.session.movement.acceleration = 100.0F;
    config.session.movement.gravity = 0.0F;
    config.session.movement.terminal_fall_speed = 50.0F;
    config.session.movement.fixed_dt = 0.1F;
    config.session.movement.character.radius = 0.25F;
    config.session.movement.character.height = 1.0F;
    config.session.movement.character.skin_width = 0.0F;
    config.session.movement.character.ground_snap_distance = 0.0F;
    config.session.movement.character.max_slide_iterations = 4;
    config.max_ticks_per_frame = 4;
    return config;
}

const stellar::server::PlayerSnapshot& only_player(const stellar::server::WorldSnapshot& snapshot) {
    assert(snapshot.players.size() == 1);
    return snapshot.players[0];
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

void assert_same_snapshot(const stellar::server::WorldSnapshot& a,
                          const stellar::server::WorldSnapshot& b) {
    assert(a.tick == b.tick);
    assert(a.players.size() == b.players.size());
    for (std::size_t i = 0; i < a.players.size(); ++i) {
        assert(a.players[i].player_id == b.players[i].player_id);
        assert(nearly_equal(a.players[i].position, b.players[i].position, 0.0F));
        assert(nearly_equal(a.players[i].velocity, b.players[i].velocity, 0.0F));
        assert(a.players[i].rotation == b.players[i].rotation);
        assert(a.players[i].grounded == b.players[i].grounded);
    }
    assert(a.trigger_events.size() == b.trigger_events.size());
    for (std::size_t i = 0; i < a.trigger_events.size(); ++i) {
        assert(a.trigger_events[i].trigger_name == b.trigger_events[i].trigger_name);
        assert(a.trigger_events[i].entered == b.trigger_events[i].entered);
        assert(a.trigger_events[i].stayed == b.trigger_events[i].stayed);
        assert(a.trigger_events[i].exited == b.trigger_events[i].exited);
    }
    assert(a.object_collider_events.size() == b.object_collider_events.size());
    for (std::size_t i = 0; i < a.object_collider_events.size(); ++i) {
        assert(a.object_collider_events[i].player_id == b.object_collider_events[i].player_id);
        assert(a.object_collider_events[i].collider_id == b.object_collider_events[i].collider_id);
        assert(a.object_collider_events[i].name == b.object_collider_events[i].name);
        assert(a.object_collider_events[i].archetype == b.object_collider_events[i].archetype);
        assert(a.object_collider_events[i].entered == b.object_collider_events[i].entered);
        assert(a.object_collider_events[i].stayed == b.object_collider_events[i].stayed);
        assert(a.object_collider_events[i].exited == b.object_collider_events[i].exited);
    }
}

stellar::world::ObjectCollider sphere_collider(std::uint32_t id,
                                               std::string name,
                                               std::string archetype,
                                               Vec3 center,
                                               float radius = 0.25F) {
    return stellar::world::ObjectCollider{
        .id = id,
        .name = std::move(name),
        .archetype = std::move(archetype),
        .shape = {.type = stellar::world::ObjectColliderShapeType::kSphere,
                  .center = center,
                  .radius = radius},
        .enabled = true};
}

void initial_snapshot_matches_spawn() {
    const auto scene = scene_with_markers({player_spawn({2.0F, 3.0F, 4.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    const stellar::client::LocalLoopbackRuntime runtime(world, test_runtime_config(7));

    const auto& snapshot = runtime.latest_snapshot();

    assert(snapshot.tick == 0);
    assert(snapshot.trigger_events.empty());
    assert(only_player(snapshot).player_id == 7);
    assert(nearly_equal(only_player(snapshot).position, {2.0F, 3.0F, 4.0F}, 0.0F));
}

void zero_delta_runs_no_ticks() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::LocalLoopbackRuntime runtime(world, test_runtime_config());
    const stellar::platform::Input input;

    const auto result = runtime.update(input, 0.0F);

    assert(result.ticks_run == 0);
    assert(!result.dropped_excess_time);
    assert(result.snapshot.tick == 0);
}

void small_delta_accumulates_until_fixed_tick() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::LocalLoopbackRuntime runtime(world, test_runtime_config());
    const stellar::platform::Input input;

    const auto first = runtime.update(input, 0.04F);
    const auto second = runtime.update(input, 0.05F);
    const auto third = runtime.update(input, 0.01F);

    assert(first.ticks_run == 0);
    assert(second.ticks_run == 0);
    assert(third.ticks_run == 1);
    assert(third.snapshot.tick == 1);
}

void one_fixed_tick_applies_input_command() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::LocalLoopbackRuntime runtime(world, test_runtime_config());

    const auto result = runtime.update(input_with_key(SDL_SCANCODE_D), 0.1F);

    assert(result.ticks_run == 1);
    assert(result.snapshot.tick == 1);
    assert(only_player(result.snapshot).position[0] > 0.9F);
    assert(only_player(result.snapshot).velocity[0] > 9.9F);
}

void large_delta_clamps_ticks_per_frame() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    auto config = test_runtime_config();
    config.max_ticks_per_frame = 2;
    stellar::client::LocalLoopbackRuntime runtime(world, config);
    const stellar::platform::Input input;

    const auto first = runtime.update(input, 1.0F);
    const auto second = runtime.update(input, 0.0F);

    assert(first.ticks_run == 2);
    assert(first.dropped_excess_time);
    assert(first.snapshot.tick == 2);
    assert(second.ticks_run == 0);
    assert(second.snapshot.tick == 2);
}

void loopback_uses_authoritative_collision() {
    const auto scene = scene_with_collision_and_markers({wall_x_triangle_a(), wall_x_triangle_b()},
                                                        {player_spawn({0.0F, 0.5F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::LocalLoopbackRuntime runtime(world, test_runtime_config());

    const auto result = runtime.update(input_with_key(SDL_SCANCODE_D), 0.1F);

    assert(result.ticks_run == 1);
    assert(only_player(result.snapshot).position[0] < 0.56F);
}

void trigger_events_are_forwarded_in_frame_result() {
    const auto scene = scene_with_markers({player_spawn({-2.0F, 0.0F, 0.0F}),
                                           trigger_marker("Gate", {0.0F, 0.0F, 0.0F},
                                                          {0.25F, 0.25F, 0.25F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::LocalLoopbackRuntime runtime(world, test_runtime_config());

    const auto result = runtime.update(input_with_key(SDL_SCANCODE_D), 0.2F);

    assert(result.ticks_run == 2);
    assert(result.snapshot.trigger_events.size() == 1);
    assert(result.snapshot.trigger_events[0].trigger_name == "Gate");
    assert(result.snapshot.trigger_events[0].entered);
    assert(runtime.latest_snapshot().trigger_events.empty());
    assert(runtime.latest_snapshot().object_collider_events.empty());
}

void object_collider_events_are_forwarded_in_frame_result() {
    const auto scene = scene_with_markers({player_spawn({-2.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::LocalLoopbackRuntime runtime(world, test_runtime_config(6));
    const std::array colliders{sphere_collider(20, "Pickup", "coin", {0.0F, 0.0F, 0.0F})};
    runtime.set_object_colliders(colliders);

    const auto result = runtime.update(input_with_key(SDL_SCANCODE_D), 0.2F);

    assert(result.ticks_run == 2);
    assert(result.snapshot.object_collider_events.size() == 1);
    assert(result.snapshot.object_collider_events[0].player_id == 6);
    assert(result.snapshot.object_collider_events[0].collider_id == 20);
    assert(result.snapshot.object_collider_events[0].name == "Pickup");
    assert(result.snapshot.object_collider_events[0].archetype == "coin");
    assert(result.snapshot.object_collider_events[0].entered);
    assert(runtime.latest_snapshot().object_collider_events.empty());
}

void loopback_object_collider_mutations_return_exits() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::LocalLoopbackRuntime runtime(world, test_runtime_config(9));
    const stellar::platform::Input input;
    const std::array colliders{sphere_collider(21, "Hazard", "fire", {0.0F, 0.0F, 0.0F})};
    runtime.set_object_colliders(colliders);
    assert(runtime.update(input, 0.1F).snapshot.object_collider_events[0].entered);

    const auto disabled = runtime.set_object_collider_enabled(21, false);

    assert(disabled.applied);
    assert(disabled.object_collider_events.size() == 1);
    assert(disabled.object_collider_events[0].player_id == 9);
    assert(disabled.object_collider_events[0].collider_id == 21);
    assert(disabled.object_collider_events[0].exited);
    assert(runtime.latest_snapshot().object_collider_events.empty());
}

void latest_snapshot_updates_after_ticks() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::LocalLoopbackRuntime runtime(world, test_runtime_config());

    const auto before = runtime.latest_snapshot();
    const auto result = runtime.update(input_with_key(SDL_SCANCODE_D), 0.1F);
    const auto& after = runtime.latest_snapshot();

    assert(result.ticks_run == 1);
    assert(before.tick == 0);
    assert(after.tick == 1);
    assert(only_player(after).position[0] > only_player(before).position[0]);
}

void missing_collision_world_still_ticks_deterministically() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::LocalLoopbackRuntime first(world, test_runtime_config());
    stellar::client::LocalLoopbackRuntime second(world, test_runtime_config());

    const auto first_result = first.update(input_with_key(SDL_SCANCODE_D), 0.1F);
    const auto second_result = second.update(input_with_key(SDL_SCANCODE_D), 0.1F);

    assert(!world.collision_world.has_value());
    assert_same_snapshot(first_result.snapshot, second_result.snapshot);
}

void same_input_sequence_same_snapshots() {
    const auto scene = scene_with_markers({player_spawn({-2.0F, 0.0F, 0.0F}),
                                           trigger_marker("Gate", {0.0F, 0.0F, 0.0F},
                                                          {0.25F, 0.25F, 0.25F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::LocalLoopbackRuntime first(world, test_runtime_config(3));
    stellar::client::LocalLoopbackRuntime second(world, test_runtime_config(3));

    assert_same_snapshot(first.update(input_with_key(SDL_SCANCODE_D), 0.05F).snapshot,
                         second.update(input_with_key(SDL_SCANCODE_D), 0.05F).snapshot);
    assert_same_snapshot(first.update(input_with_key(SDL_SCANCODE_D), 0.15F).snapshot,
                         second.update(input_with_key(SDL_SCANCODE_D), 0.15F).snapshot);
    const stellar::platform::Input empty_input;
    assert_same_snapshot(first.update(empty_input, 0.1F).snapshot,
                         second.update(empty_input, 0.1F).snapshot);
    assert_same_snapshot(first.latest_snapshot(), second.latest_snapshot());
}

} // namespace

int main() {
    initial_snapshot_matches_spawn();
    zero_delta_runs_no_ticks();
    small_delta_accumulates_until_fixed_tick();
    one_fixed_tick_applies_input_command();
    large_delta_clamps_ticks_per_frame();
    loopback_uses_authoritative_collision();
    trigger_events_are_forwarded_in_frame_result();
    object_collider_events_are_forwarded_in_frame_result();
    loopback_object_collider_mutations_return_exits();
    latest_snapshot_updates_after_ticks();
    missing_collision_world_still_ticks_deterministically();
    same_input_sequence_same_snapshots();
    return 0;
}
