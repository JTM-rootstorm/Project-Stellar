#include "stellar/server/WorldSession.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <limits>
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

bool nearly_equal_quat(std::array<float, 4> a,
                       std::array<float, 4> b,
                       float tolerance = kTolerance) {
    return nearly_equal(a[0], b[0], tolerance) && nearly_equal(a[1], b[1], tolerance) &&
           nearly_equal(a[2], b[2], tolerance) && nearly_equal(a[3], b[3], tolerance);
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

stellar::assets::LevelAsset scene_with_named_collision_meshes(
    std::string wall_name,
    float wall_x,
    Vec3 spawn_position) {
    stellar::assets::LevelAsset scene = scene_with_markers({player_spawn(spawn_position)});
    stellar::assets::CollisionMesh wall;
    wall.name = std::move(wall_name);
    wall.triangles = {wall_x_triangle_a(wall_x), wall_x_triangle_b(wall_x)};
    stellar::assets::CollisionMesh floor;
    floor.name = "Floor";
    floor.triangles = {triangle({-10.0F, -10.0F, -5.0F}, {10.0F, -10.0F, -5.0F},
                                {10.0F, 10.0F, -5.0F}, {0.0F, 0.0F, 1.0F})};
    scene.level_collision = stellar::assets::LevelCollisionAsset{.meshes = {wall, floor}};
    return scene;
}

stellar::server::WorldSessionConfig test_session_config(
    stellar::server::PlayerId player_id = 1) {
    stellar::server::WorldSessionConfig config;
    config.local_player_id = player_id;
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

const stellar::server::PlayerSnapshot& only_player(const stellar::server::WorldSnapshot& snapshot) {
    assert(snapshot.players.size() == 1);
    return snapshot.players[0];
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
    assert(a.footstep_events.size() == b.footstep_events.size());
    for (std::size_t i = 0; i < a.footstep_events.size(); ++i) {
        assert(a.footstep_events[i].tick == b.footstep_events[i].tick);
        assert(a.footstep_events[i].player_id == b.footstep_events[i].player_id);
        assert(a.footstep_events[i].entity_id == b.footstep_events[i].entity_id);
        assert(a.footstep_events[i].surface_id == b.footstep_events[i].surface_id);
        assert(a.footstep_events[i].source_material_name ==
               b.footstep_events[i].source_material_name);
    }
}

stellar::world::ObjectCollider sphere_collider(std::uint32_t id,
                                               std::string name,
                                               std::string archetype,
                                               Vec3 center,
                                               float radius = 0.25F,
                                               bool enabled = true) {
    return stellar::world::ObjectCollider{
        .id = id,
        .name = std::move(name),
        .archetype = std::move(archetype),
        .shape = {.type = stellar::world::ObjectColliderShapeType::kSphere,
                  .center = center,
                  .radius = radius},
        .enabled = enabled};
}

void session_initial_snapshot_uses_player_spawn() {
    const auto scene = scene_with_markers({player_spawn({2.0F, 3.0F, 4.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    const stellar::server::WorldSession session(world, test_session_config(7));

    const auto snapshot = session.snapshot();

    assert(snapshot.tick == 0);
    assert(snapshot.trigger_events.empty());
    assert(only_player(snapshot).player_id == 7);
    assert(nearly_equal(only_player(snapshot).position, {2.0F, 3.0F, 4.0F}, 0.0F));
    assert(nearly_equal(only_player(snapshot).velocity, {0.0F, 0.0F, 0.0F}, 0.0F));
    assert(only_player(snapshot).rotation == (std::array<float, 4>{0.0F, 0.0F, 0.0F, 1.0F}));
}

void session_defaults_to_origin_without_player_spawn() {
    const stellar::assets::LevelAsset scene;
    const auto world = stellar::world::build_runtime_world(scene);
    const stellar::server::WorldSession session(world, test_session_config());

    const auto snapshot = session.snapshot();

    assert(nearly_equal(only_player(snapshot).position, {0.0F, 0.0F, 0.0F}, 0.0F));
}

void tick_without_command_advances_deterministically() {
    const auto scene = scene_with_markers({player_spawn({1.0F, 2.0F, 3.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config());

    const auto first = session.tick({});
    const auto second = session.tick({});

    assert(first.tick == 1);
    assert(second.tick == 2);
    assert(session.tick_index() == 2);
    assert(nearly_equal(only_player(first).position, {1.0F, 2.0F, 3.0F}, 0.0F));
    assert(nearly_equal(only_player(second).position, {1.0F, 2.0F, 3.0F}, 0.0F));
}

void tick_with_unknown_player_command_is_ignored() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession unknown_session(world, test_session_config(1));
    stellar::server::WorldSession empty_session(world, test_session_config(1));
    const std::vector<stellar::server::PlayerCommand> commands{
        {.player_id = 99,
         .movement = {.wish_direction = {1.0F, 0.0F, 0.0F},
                      .view_yaw_degrees = 90.0F,
                      .view_pitch_degrees = 45.0F,
                      .has_view_angles = true}}};

    const auto unknown = unknown_session.tick(commands);
    const auto empty = empty_session.tick({});

    assert_same_snapshot(unknown, empty);
}

void tick_with_local_player_command_updates_player_snapshot() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config(5));
    const std::vector<stellar::server::PlayerCommand> commands{
        {.player_id = 5, .movement = {.wish_direction = {1.0F, 0.0F, 0.0F}}}};

    const auto snapshot = session.tick(commands);

    assert(snapshot.tick == 1);
    assert(only_player(snapshot).position[0] > 0.9F);
    assert(only_player(snapshot).velocity[0] > 9.9F);
}

void tick_with_local_player_command_updates_authoritative_view_rotation() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config(5));
    const std::vector<stellar::server::PlayerCommand> commands{
        {.player_id = 5,
         .movement = {.view_yaw_degrees = 90.0F,
                      .view_pitch_degrees = 0.0F,
                      .has_view_angles = true}}};

    const auto snapshot = session.tick(commands);
    const auto preserved = session.tick({});

    constexpr float kSinCosHalfTurn = 0.70710678118654752440F;
    const std::array<float, 4> expected{0.0F, 0.0F, kSinCosHalfTurn, kSinCosHalfTurn};
    assert(nearly_equal_quat(only_player(snapshot).rotation, expected));
    assert(nearly_equal_quat(only_player(preserved).rotation, expected));
}

void non_finite_view_angles_preserve_previous_authoritative_view() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config(5));
    const std::vector<stellar::server::PlayerCommand> finite{
        {.player_id = 5,
         .movement = {.view_yaw_degrees = 90.0F,
                      .view_pitch_degrees = 0.0F,
                      .has_view_angles = true}}};
    const auto first = session.tick(finite);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const std::vector<stellar::server::PlayerCommand> invalid{
        {.player_id = 5,
         .movement = {.view_yaw_degrees = nan,
                      .view_pitch_degrees = std::numeric_limits<float>::infinity(),
                      .has_view_angles = true}}};
    const auto second = session.tick(invalid);

    assert(nearly_equal_quat(only_player(second).rotation, only_player(first).rotation));
}

void wall_collision_is_authoritative_in_snapshot() {
    const auto scene = scene_with_collision_and_markers({wall_x_triangle_a(), wall_x_triangle_b()},
                                                        {player_spawn({0.0F, 0.0F, 0.5F})});
    const auto world = stellar::world::build_runtime_world(scene);
    auto config = test_session_config();
    config.movement.character.radius = 0.25F;
    stellar::server::WorldSession session(world, config);
    const std::vector<stellar::server::PlayerCommand> commands{
        {.player_id = 1, .movement = {.wish_direction = {1.0F, 0.0F, 0.0F}}}};

    const auto snapshot = session.tick(commands);

    assert(only_player(snapshot).position[0] < 0.56F);
}

void trigger_enter_stay_exit_events_are_reported_once_per_tick() {
    const auto scene = scene_with_markers({player_spawn({-2.0F, 0.0F, 0.0F}),
                                           trigger_marker("Gate", {0.0F, 0.0F, 0.0F},
                                                          {0.25F, 0.25F, 0.25F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config());
    const std::vector<stellar::server::PlayerCommand> move_right{
        {.player_id = 1, .movement = {.wish_direction = {1.0F, 0.0F, 0.0F}}}};

    assert(session.tick(move_right).trigger_events.empty());
    const auto enter = session.tick(move_right);
    const auto stay = session.tick({});
    const auto exit = session.tick(move_right);

    assert(enter.trigger_events.size() == 1);
    assert(enter.trigger_events[0].trigger_name == "Gate");
    assert(enter.trigger_events[0].entered);
    assert(!enter.trigger_events[0].stayed);
    assert(!enter.trigger_events[0].exited);
    assert(stay.trigger_events.size() == 1);
    assert(!stay.trigger_events[0].entered);
    assert(stay.trigger_events[0].stayed);
    assert(!stay.trigger_events[0].exited);
    assert(exit.trigger_events.size() == 1);
    assert(!exit.trigger_events[0].entered);
    assert(!exit.trigger_events[0].stayed);
    assert(exit.trigger_events[0].exited);
}

void snapshot_does_not_replay_previous_trigger_events() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F}),
                                           trigger_marker("Start", {0.0F, 0.0F, 0.0F},
                                                          {0.25F, 0.25F, 0.25F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config());

    const auto tick_snapshot = session.tick({});
    const auto pure_snapshot = session.snapshot();

    assert(tick_snapshot.trigger_events.size() == 1);
    assert(tick_snapshot.trigger_events[0].entered);
    assert(pure_snapshot.tick == tick_snapshot.tick);
    assert(pure_snapshot.trigger_events.empty());
    assert(pure_snapshot.object_collider_events.empty());
}

void object_collider_enter_stay_exit_events_follow_authoritative_player() {
    const auto scene = scene_with_markers({player_spawn({-2.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config(4));
    const std::vector<stellar::server::PlayerCommand> move_right{
        {.player_id = 4, .movement = {.wish_direction = {1.0F, 0.0F, 0.0F}}}};
    const std::array colliders{sphere_collider(10, "Pickup", "coin", {0.0F, 0.0F, 0.0F})};
    session.set_object_colliders(colliders);

    assert(session.tick(move_right).object_collider_events.empty());
    const auto enter = session.tick(move_right);
    const auto stay = session.tick({});
    const auto exit = session.tick(move_right);

    assert(enter.object_collider_events.size() == 1);
    assert(enter.object_collider_events[0].player_id == 4);
    assert(enter.object_collider_events[0].collider_id == 10);
    assert(enter.object_collider_events[0].name == "Pickup");
    assert(enter.object_collider_events[0].archetype == "coin");
    assert(enter.object_collider_events[0].entered);
    assert(!enter.object_collider_events[0].stayed);
    assert(!enter.object_collider_events[0].exited);
    assert(stay.object_collider_events.size() == 1);
    assert(!stay.object_collider_events[0].entered);
    assert(stay.object_collider_events[0].stayed);
    assert(!stay.object_collider_events[0].exited);
    assert(exit.object_collider_events.size() == 1);
    assert(!exit.object_collider_events[0].entered);
    assert(!exit.object_collider_events[0].stayed);
    assert(exit.object_collider_events[0].exited);
}

void snapshot_does_not_replay_previous_object_collider_events() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config());
    const std::array colliders{sphere_collider(11, "StartPickup", "coin", {0.0F, 0.0F, 0.0F})};
    session.set_object_colliders(colliders);

    const auto tick_snapshot = session.tick({});
    const auto pure_snapshot = session.snapshot();

    assert(tick_snapshot.object_collider_events.size() == 1);
    assert(tick_snapshot.object_collider_events[0].entered);
    assert(pure_snapshot.tick == tick_snapshot.tick);
    assert(pure_snapshot.object_collider_events.empty());
}

void object_collider_mutations_return_synchronous_exit_events() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config(8));
    const std::array colliders{sphere_collider(12, "Hazard", "fire", {0.0F, 0.0F, 0.0F})};
    session.set_object_colliders(colliders);
    assert(session.tick({}).object_collider_events[0].entered);

    const auto disabled = session.set_object_collider_enabled(12, false);
    const auto pure_snapshot = session.snapshot();

    assert(disabled.applied);
    assert(disabled.object_collider_events.size() == 1);
    assert(disabled.object_collider_events[0].player_id == 8);
    assert(disabled.object_collider_events[0].collider_id == 12);
    assert(disabled.object_collider_events[0].name == "Hazard");
    assert(disabled.object_collider_events[0].archetype == "fire");
    assert(disabled.object_collider_events[0].exited);
    assert(pure_snapshot.object_collider_events.empty());
    assert(session.tick({}).object_collider_events.empty());
}

void object_collider_replace_preserving_overlaps_returns_removed_exits() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config());
    const std::array initial{sphere_collider(13, "Key", "pickup", {0.0F, 0.0F, 0.0F})};
    const std::array replacement{sphere_collider(14, "Other", "pickup", {3.0F, 0.0F, 0.0F})};
    session.set_object_colliders(initial);
    assert(session.tick({}).object_collider_events[0].entered);

    const auto exits = session.replace_object_colliders_preserving_overlaps(replacement);

    assert(exits.size() == 1);
    assert(exits[0].collider_id == 13);
    assert(exits[0].name == "Key");
    assert(exits[0].archetype == "pickup");
    assert(exits[0].exited);
}

void reset_reinitializes_spawn_state_tick_and_trigger_tracker() {
    const auto first_scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F}),
                                                 trigger_marker("First", {0.0F, 0.0F, 0.0F},
                                                                {0.25F, 0.25F, 0.25F})});
    const auto first_world = stellar::world::build_runtime_world(first_scene);
    const auto second_scene = scene_with_markers({player_spawn({3.0F, 0.0F, 0.0F}),
                                                  trigger_marker("Second", {3.0F, 0.0F, 0.0F},
                                                                 {0.25F, 0.25F, 0.25F})});
    const auto second_world = stellar::world::build_runtime_world(second_scene);
    stellar::server::WorldSession session(first_world, test_session_config(1));

    assert(session.tick({}).trigger_events[0].entered);
    assert(session.tick({}).trigger_events[0].stayed);
    session.reset(second_world, test_session_config(2));
    const auto reset_snapshot = session.snapshot();
    const auto reset_tick = session.tick({});

    assert(reset_snapshot.tick == 0);
    assert(session.tick_index() == 1);
    assert(only_player(reset_snapshot).player_id == 2);
    assert(nearly_equal(only_player(reset_snapshot).position, {3.0F, 0.0F, 0.0F}, 0.0F));
    assert(reset_tick.trigger_events.size() == 1);
    assert(reset_tick.trigger_events[0].trigger_name == "Second");
    assert(reset_tick.trigger_events[0].entered);
    assert(!reset_tick.trigger_events[0].stayed);
}

void same_inputs_produce_same_snapshots() {
    const auto scene = scene_with_markers({player_spawn({-2.0F, 0.0F, 0.0F}),
                                           trigger_marker("Gate", {0.0F, 0.0F, 0.0F},
                                                          {0.25F, 0.25F, 0.25F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession a(world, test_session_config(3));
    stellar::server::WorldSession b(world, test_session_config(3));
    const std::vector<stellar::server::PlayerCommand> move_right{
        {.player_id = 3, .movement = {.wish_direction = {1.0F, 0.0F, 0.0F}}}};

    assert_same_snapshot(a.tick(move_right), b.tick(move_right));
    assert_same_snapshot(a.tick(move_right), b.tick(move_right));
    assert_same_snapshot(a.tick({}), b.tick({}));
    assert_same_snapshot(a.snapshot(), b.snapshot());
}

void world_session_disabled_mesh_affects_next_tick() {
    const auto scene = scene_with_named_collision_meshes("Gate", 0.8F, {0.0F, 0.0F, 0.5F});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config());
    const std::vector<stellar::server::PlayerCommand> commands{
        {.player_id = 1, .movement = {.wish_direction = {1.0F, 0.0F, 0.0F}}}};

    const auto mutation = session.set_collision_mesh_enabled("Gate", false);
    const auto snapshot = session.tick(commands);

    assert(mutation.applied);
    assert(mutation.code == "ok");
    assert(only_player(snapshot).position[0] > 0.9F);
}

void world_session_collision_state_resets_with_world() {
    const auto first_scene = scene_with_named_collision_meshes("GateA", 0.8F, {0.0F, 0.0F, 0.5F});
    const auto first_world = stellar::world::build_runtime_world(first_scene);
    const auto second_scene = scene_with_named_collision_meshes("GateB", 0.8F, {0.0F, 0.0F, 0.5F});
    const auto second_world = stellar::world::build_runtime_world(second_scene);
    stellar::server::WorldSession session(first_world, test_session_config());
    assert(session.set_collision_mesh_enabled("GateA", false).applied);

    session.reset(second_world, test_session_config());
    const auto old_name = session.set_collision_mesh_enabled("GateA", false);
    const auto new_name = session.set_collision_mesh_enabled("GateB", false);

    assert(!old_name.applied);
    assert(old_name.code == "not_found");
    assert(new_name.applied);
}

void grounded_movement_emits_footstep_surface_event_once_per_tick() {
    auto floor = triangle({-10.0F, -10.0F, 0.0F}, {10.0F, -10.0F, 0.0F},
                          {10.0F, 10.0F, 0.0F}, {0.0F, 0.0F, 1.0F});
    auto floor_b = triangle({-10.0F, -10.0F, 0.0F}, {10.0F, 10.0F, 0.0F},
                            {-10.0F, 10.0F, 0.0F}, {0.0F, 0.0F, 1.0F});
    floor.surface.source_material_name = "wood/plank_01";
    floor.surface.footstep_surface_id = "wood";
    floor_b.surface = floor.surface;
    const auto scene =
        scene_with_collision_and_markers({floor, floor_b}, {player_spawn({0.0F, 0.0F, 0.55F})});
    const auto world = stellar::world::build_runtime_world(scene);
    auto config = test_session_config();
    config.movement.gravity = 24.0F;
    config.movement.character.ground_snap_distance = 0.2F;
    config.footsteps.min_horizontal_speed = 0.0F;
    config.footsteps.walk_step_distance = 0.01F;
    config.footsteps.run_step_distance = 0.01F;
    stellar::server::WorldSession session(world, config);
    const std::array commands{stellar::server::PlayerCommand{
        .player_id = config.local_player_id,
        .movement = stellar::server::MovementCommand{.wish_direction = {1.0F, 0.0F, 0.0F}}}};

    stellar::server::WorldSnapshot moved;
    for (int tick = 0; tick < 3 && moved.footstep_events.empty(); ++tick) {
        moved = session.tick(commands);
    }

    assert(moved.footstep_events.size() == 1);
    assert(moved.footstep_events[0].tick == moved.tick);
    assert(moved.footstep_events[0].player_id == config.local_player_id);
    assert(moved.footstep_events[0].surface_id == "wood");
    assert(moved.footstep_events[0].source_material_name == "wood/plank_01");
    assert(session.snapshot().footstep_events.empty());
}

void snapshot_does_not_apply_or_replay_collision_mutation() {
    const auto scene = scene_with_named_collision_meshes("Gate", 0.8F, {0.0F, 0.0F, 0.5F});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config());
    const auto before = session.snapshot();
    const auto mutation = session.set_collision_mesh_enabled("Gate", false);
    const auto after = session.snapshot();

    assert(mutation.applied);
    assert_same_snapshot(before, after);
}

void same_collision_state_and_inputs_produce_same_snapshots() {
    const auto scene = scene_with_named_collision_meshes("Gate", 0.8F, {0.0F, 0.0F, 0.5F});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession a(world, test_session_config());
    stellar::server::WorldSession b(world, test_session_config());
    assert(a.set_collision_mesh_enabled("Gate", false).applied);
    assert(b.set_collision_mesh_enabled("Gate", false).applied);
    const std::vector<stellar::server::PlayerCommand> commands{
        {.player_id = 1, .movement = {.wish_direction = {1.0F, 0.0F, 0.0F}}}};

    assert_same_snapshot(a.tick(commands), b.tick(commands));
    assert_same_snapshot(a.tick({}), b.tick({}));
}

stellar::assets::LevelAsset scene_with_func_door() {
    auto scene = scene_with_named_collision_meshes("DoorA", 0.8F, {0.0F, 0.0F, 0.5F});
    scene.geometry.brush_entities.push_back(stellar::assets::LevelBrushEntity{
        .name = "DoorA",
        .classname = "func_door",
        .targetname = "DoorA",
        .bsp_model_index = 0,
        .collision_mesh_name = "DoorA",
        .bounds_min = {0.8F, -2.0F, -4.0F},
        .bounds_max = {8.8F, 4.0F, 4.0F},
        .properties = {{.key = "angle", .value = "0"},
                       {.key = "speed", .value = "80"},
                       {.key = "wait", .value = "-1"},
                       {.key = "lip", .value = "0"}}});
    stellar::assets::WorldMarker door_marker;
    door_marker.type = stellar::assets::WorldMarkerType::kEntitySpawn;
    door_marker.name = "DoorA";
    door_marker.archetype = "func_door";
    door_marker.position = {0.8F, 0.0F, 0.0F};
    door_marker.scale = {0.0F, 6.0F, 8.0F};
    scene.world_metadata.markers.push_back(door_marker);
    return scene;
}

void func_door_target_moves_collision_and_snapshot_transform() {
    const auto scene = scene_with_func_door();
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config());

    assert(session.fire_target("DoorA"));
    const auto moved = session.tick({});

    assert(moved.brush_movers.size() == 1);
    assert(moved.brush_movers[0].name == "DoorA");
    assert(moved.brush_movers[0].translation[0] > 0.0F);
    assert(!moved.gameplay_world.entities.empty());
    bool saw_moved_door = false;
    for (const auto& entity : moved.gameplay_world.entities) {
        if (entity.metadata.name == "DoorA") {
            saw_moved_door = saw_moved_door || entity.transform.position[0] > 0.8F;
        }
    }
    assert(saw_moved_door);
}

void fire_target_applies_to_duplicate_canonical_brush_names() {
    auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.5F})});
    stellar::assets::CollisionMesh first;
    first.name = "DoorMeshA";
    first.triangles = {wall_x_triangle_a(0.8F), wall_x_triangle_b(0.8F)};
    stellar::assets::CollisionMesh second;
    second.name = "DoorMeshB";
    second.triangles = {wall_x_triangle_a(2.0F), wall_x_triangle_b(2.0F)};
    scene.level_collision = stellar::assets::LevelCollisionAsset{.meshes = {first, second}};
    scene.geometry.brush_entities.push_back(stellar::assets::LevelBrushEntity{
        .name = "SharedDoor",
        .classname = "func_door",
        .targetname = "SharedDoor",
        .bsp_model_index = 0,
        .collision_mesh_name = "DoorMeshA",
        .bounds_min = {0.8F, -2.0F, -4.0F},
        .bounds_max = {8.8F, 4.0F, 4.0F},
        .properties = {{.key = "angle", .value = "0"},
                       {.key = "speed", .value = "80"},
                       {.key = "wait", .value = "-1"},
                       {.key = "lip", .value = "0"}}});
    scene.geometry.brush_entities.push_back(stellar::assets::LevelBrushEntity{
        .name = "SharedDoor",
        .classname = "func_door",
        .targetname = "SharedDoor",
        .bsp_model_index = 1,
        .collision_mesh_name = "DoorMeshB",
        .bounds_min = {2.0F, -2.0F, -4.0F},
        .bounds_max = {10.0F, 4.0F, 4.0F},
        .properties = {{.key = "angle", .value = "0"},
                       {.key = "speed", .value = "80"},
                       {.key = "wait", .value = "-1"},
                       {.key = "lip", .value = "0"}}});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config());

    assert(session.fire_target("SharedDoor"));
    const auto moved = session.tick({});

    assert(moved.brush_movers.size() == 2);
    assert(moved.brush_movers[0].name == "SharedDoor");
    assert(moved.brush_movers[1].name == "SharedDoor");
    assert(moved.brush_movers[0].translation[0] > 0.0F);
    assert(moved.brush_movers[1].translation[0] > 0.0F);
}

void target_relay_fire_routes_to_downstream_target() {
    auto scene = scene_with_func_door();
    stellar::assets::WorldMarker relay;
    relay.type = stellar::assets::WorldMarkerType::kEntitySpawn;
    relay.name = "RelayA";
    relay.archetype = "target_stellar_relay";
    relay.properties = {{.key = "target", .value = "DoorA"}};
    scene.world_metadata.markers.push_back(relay);
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config());

    assert(session.fire_target("RelayA"));
    const auto moved = session.tick({});

    assert(moved.brush_movers.size() == 1);
    assert(moved.brush_movers[0].translation[0] > 0.0F);
}

void target_relay_delay_schedules_downstream_target() {
    auto scene = scene_with_func_door();
    stellar::assets::WorldMarker relay;
    relay.type = stellar::assets::WorldMarkerType::kEntitySpawn;
    relay.name = "RelayDelay";
    relay.archetype = "target_stellar_relay";
    relay.properties = {{.key = "target", .value = "DoorA"},
                        {.key = "delay", .value = "0.2"}};
    scene.world_metadata.markers.push_back(relay);
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config());

    assert(session.fire_target("RelayDelay"));
    const auto before_delay = session.tick({});
    const auto after_delay = session.tick({});

    assert(before_delay.brush_movers.size() == 1);
    assert(before_delay.brush_movers[0].translation[0] == 0.0F);
    assert(after_delay.brush_movers[0].translation[0] > 0.0F);
}

void missing_target_is_diagnostic_not_crash() {
    const auto scene = scene_with_func_door();
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world, test_session_config());

    assert(!session.fire_target("MissingDoor"));
    assert(session.target_diagnostics().size() == 1);
}

} // namespace

int main() {
    session_initial_snapshot_uses_player_spawn();
    session_defaults_to_origin_without_player_spawn();
    tick_without_command_advances_deterministically();
    tick_with_unknown_player_command_is_ignored();
    tick_with_local_player_command_updates_player_snapshot();
    tick_with_local_player_command_updates_authoritative_view_rotation();
    non_finite_view_angles_preserve_previous_authoritative_view();
    wall_collision_is_authoritative_in_snapshot();
    trigger_enter_stay_exit_events_are_reported_once_per_tick();
    snapshot_does_not_replay_previous_trigger_events();
    object_collider_enter_stay_exit_events_follow_authoritative_player();
    snapshot_does_not_replay_previous_object_collider_events();
    object_collider_mutations_return_synchronous_exit_events();
    object_collider_replace_preserving_overlaps_returns_removed_exits();
    reset_reinitializes_spawn_state_tick_and_trigger_tracker();
    same_inputs_produce_same_snapshots();
    world_session_disabled_mesh_affects_next_tick();
    world_session_collision_state_resets_with_world();
    grounded_movement_emits_footstep_surface_event_once_per_tick();
    snapshot_does_not_apply_or_replay_collision_mutation();
    same_collision_state_and_inputs_produce_same_snapshots();
    func_door_target_moves_collision_and_snapshot_transform();
    fire_target_applies_to_duplicate_canonical_brush_names();
    target_relay_fire_routes_to_downstream_target();
    target_relay_delay_schedules_downstream_target();
    missing_target_is_diagnostic_not_crash();
    return 0;
}
