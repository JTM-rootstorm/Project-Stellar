#include "stellar/server/MovementSimulation.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <limits>
#include <string>

namespace {

using Vec3 = std::array<float, 3>;

constexpr float kTolerance = 2.0e-2F;

template <typename T>
concept HasPositionMember = requires(T value) { value.position; };

bool nearly_equal(float a, float b, float tolerance = kTolerance) {
    return std::abs(a - b) <= tolerance;
}

bool nearly_equal(Vec3 a, Vec3 b, float tolerance = kTolerance) {
    return nearly_equal(a[0], b[0], tolerance) && nearly_equal(a[1], b[1], tolerance) &&
           nearly_equal(a[2], b[2], tolerance);
}

bool finite(Vec3 value) {
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

stellar::assets::CollisionTriangle triangle(Vec3 a, Vec3 b, Vec3 c, Vec3 normal) {
    return stellar::assets::CollisionTriangle{.a = a, .b = b, .c = c, .normal = normal};
}

stellar::assets::CollisionTriangle floor_triangle_a(float y = 0.0F) {
    return triangle({-10.0F, y, -10.0F}, {10.0F, y, -10.0F}, {10.0F, y, 10.0F},
                    {0.0F, 1.0F, 0.0F});
}

stellar::assets::CollisionTriangle floor_triangle_b(float y = 0.0F) {
    return triangle({-10.0F, y, -10.0F}, {10.0F, y, 10.0F}, {-10.0F, y, 10.0F},
                    {0.0F, 1.0F, 0.0F});
}

stellar::assets::CollisionTriangle wall_x_triangle_a(float x = 1.0F) {
    return triangle({x, -2.0F, -4.0F}, {x, 4.0F, 4.0F}, {x, -2.0F, 4.0F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::CollisionTriangle wall_x_triangle_b(float x = 1.0F) {
    return triangle({x, -2.0F, -4.0F}, {x, 4.0F, -4.0F}, {x, 4.0F, 4.0F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::LevelAsset scene_with_metadata(
    std::initializer_list<stellar::assets::WorldMarker> markers) {
    stellar::assets::LevelAsset scene;
    scene.world_metadata.markers.assign(markers.begin(), markers.end());
    return scene;
}

stellar::assets::WorldMarker player_spawn(Vec3 position) {
    stellar::assets::WorldMarker marker;
    marker.type = stellar::assets::WorldMarkerType::kPlayerSpawn;
    marker.name = "SPAWN_Player";
    marker.position = position;
    return marker;
}

stellar::assets::LevelAsset scene_with_collision(
    std::initializer_list<stellar::assets::CollisionTriangle> triangles) {
    stellar::assets::LevelAsset scene;
    stellar::assets::CollisionMesh mesh;
    mesh.name = "collision";
    mesh.triangles.assign(triangles.begin(), triangles.end());
    scene.level_collision = stellar::assets::LevelCollisionAsset{.meshes = {mesh}};
    scene.world_metadata.markers.push_back(player_spawn({0.0F, 0.5F, 0.0F}));
    return scene;
}

stellar::assets::LevelAsset scene_with_named_collision_meshes() {
    stellar::assets::LevelAsset scene;
    stellar::assets::CollisionMesh wall;
    wall.name = "Wall";
    wall.triangles = {wall_x_triangle_a(), wall_x_triangle_b()};
    stellar::assets::CollisionMesh floor;
    floor.name = "Floor";
    floor.triangles = {floor_triangle_a(-5.0F), floor_triangle_b(-5.0F)};
    scene.level_collision = stellar::assets::LevelCollisionAsset{.meshes = {wall, floor}};
    scene.world_metadata.markers.push_back(player_spawn({0.0F, 1.0F, 0.0F}));
    return scene;
}

stellar::server::MovementSimulationConfig test_config() {
    stellar::server::MovementSimulationConfig config;
    config.max_speed = 6.0F;
    config.acceleration = 60.0F;
    config.gravity = 24.0F;
    config.terminal_fall_speed = 50.0F;
    config.fixed_dt = 1.0F / 60.0F;
    config.character.radius = 0.5F;
    config.character.height = 1.0F;
    config.character.skin_width = 0.0F;
    config.character.ground_snap_distance = 0.2F;
    config.character.max_slide_iterations = 4;
    return config;
}

void spawn_state_uses_player_spawn_marker() {
    const auto scene = scene_with_metadata({player_spawn({2.0F, 3.0F, 4.0F})});
    const auto world = stellar::world::build_runtime_world(scene);

    const auto state = stellar::server::make_spawn_movement_state(world);

    assert(nearly_equal(state.position, {2.0F, 3.0F, 4.0F}, 0.0F));
    assert(nearly_equal(state.velocity, {0.0F, 0.0F, 0.0F}, 0.0F));
    assert(!state.grounded);
}

void spawn_state_defaults_to_origin_without_player_spawn() {
    const stellar::assets::LevelAsset scene;
    const auto world = stellar::world::build_runtime_world(scene);

    const auto state = stellar::server::make_spawn_movement_state(world);

    assert(nearly_equal(state.position, {0.0F, 0.0F, 0.0F}, 0.0F));
}

void empty_world_moves_without_collision() {
    const stellar::assets::LevelAsset scene;
    const auto world = stellar::world::build_runtime_world(scene);
    auto config = test_config();
    config.acceleration = 100.0F;
    stellar::server::MovementState previous;
    previous.position = {1.0F, 2.0F, 3.0F};
    previous.grounded = true;

    const auto result = stellar::server::simulate_movement_tick(
        world, previous, {.wish_direction = {1.0F, 0.0F, 0.0F}}, config);

    assert(!result.collision.hit);
    assert(!result.state.grounded);
    assert(result.state.position[0] > previous.position[0]);
    assert(nearly_equal(result.state.position[1], previous.position[1]));
    assert(finite(result.state.position));
}

void floor_world_applies_gravity_and_becomes_grounded() {
    const auto scene = scene_with_collision({floor_triangle_a(), floor_triangle_b()});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::MovementState previous;
    previous.position = {0.0F, 0.55F, 0.0F};
    previous.grounded = false;

    const auto result = stellar::server::simulate_movement_tick(world, previous, {}, test_config());

    assert(result.state.grounded);
    assert(result.collision.grounded);
    assert(nearly_equal(result.state.position[1], 0.5F, 0.04F));
    assert(nearly_equal(result.state.velocity[1], 0.0F, 0.01F));
}

void wall_world_blocks_authoritative_movement() {
    const auto scene = scene_with_collision({wall_x_triangle_a(), wall_x_triangle_b()});
    const auto world = stellar::world::build_runtime_world(scene);
    auto config = test_config();
    config.fixed_dt = 0.1F;
    config.acceleration = 100.0F;
    stellar::server::MovementState previous;
    previous.position = {0.0F, 1.0F, 0.0F};
    previous.grounded = true;

    const auto result = stellar::server::simulate_movement_tick(
        world, previous, {.wish_direction = {1.0F, 0.0F, 0.0F}}, config);

    assert(result.collision.hit);
    assert(result.state.position[0] < 0.52F);
    assert(finite(result.state.position));
}

void wish_direction_is_clamped() {
    const stellar::assets::LevelAsset scene;
    const auto world = stellar::world::build_runtime_world(scene);
    auto config = test_config();
    config.acceleration = 1000.0F;
    config.fixed_dt = 0.1F;

    const auto result = stellar::server::simulate_movement_tick(
        world, {}, {.wish_direction = {10.0F, 5.0F, 0.0F}}, config);

    assert(result.command_was_sanitized);
    assert(result.state.velocity[0] <= config.max_speed + 0.001F);
    assert(nearly_equal(result.state.velocity[2], 0.0F));
}

void nan_input_is_sanitized() {
    const stellar::assets::LevelAsset scene;
    const auto world = stellar::world::build_runtime_world(scene);
    const float nan = std::numeric_limits<float>::quiet_NaN();

    const auto result = stellar::server::simulate_movement_tick(
        world, {}, {.wish_direction = {nan, 0.0F, std::numeric_limits<float>::infinity()}},
        test_config());

    assert(result.command_was_sanitized);
    assert(finite(result.state.position));
    assert(finite(result.state.velocity));
}

void terminal_fall_speed_is_clamped() {
    const stellar::assets::LevelAsset scene;
    const auto world = stellar::world::build_runtime_world(scene);
    auto config = test_config();
    config.terminal_fall_speed = 10.0F;
    stellar::server::MovementState previous;
    previous.velocity = {0.0F, -1000.0F, 0.0F};
    previous.grounded = false;

    const auto result = stellar::server::simulate_movement_tick(world, previous, {}, config);

    assert(result.state.velocity[1] >= -10.001F);
}

void client_position_is_not_part_of_command() {
    static_assert(!HasPositionMember<stellar::server::MovementCommand>);
}

void runtime_world_collision_optional_is_handled() {
    stellar::assets::LevelAsset scene;
    scene.level_collision = stellar::assets::LevelCollisionAsset{};
    const auto world = stellar::world::build_runtime_world(scene);

    const auto result = stellar::server::simulate_movement_tick(world, {}, {}, test_config());

    assert(!world.collision_world.has_value());
    assert(!result.collision.hit);
    assert(finite(result.state.position));
}

void fixed_dt_repeatability_same_inputs_same_outputs() {
    const auto scene = scene_with_collision({floor_triangle_a(), floor_triangle_b()});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::MovementState previous;
    previous.position = {0.0F, 0.55F, 0.0F};

    const auto a = stellar::server::simulate_movement_tick(
        world, previous, {.wish_direction = {0.75F, 0.0F, 0.25F}}, test_config());
    const auto b = stellar::server::simulate_movement_tick(
        world, previous, {.wish_direction = {0.75F, 0.0F, 0.25F}}, test_config());

    assert(nearly_equal(a.state.position, b.state.position, 0.0F));
    assert(nearly_equal(a.state.velocity, b.state.velocity, 0.0F));
    assert(a.state.grounded == b.state.grounded);
    assert(a.collision.hit == b.collision.hit);
}

void collision_state_filter_affects_authoritative_movement() {
    const auto scene = scene_with_named_collision_meshes();
    const auto world = stellar::world::build_runtime_world(scene);
    auto state = stellar::world::RuntimeCollisionState::from_world(world);
    const auto mutation = state.set_mesh_enabled("Wall", false);
    assert(mutation.applied);
    auto config = test_config();
    config.fixed_dt = 0.1F;
    config.acceleration = 100.0F;
    stellar::server::MovementState previous;
    previous.position = {0.0F, 1.0F, 0.0F};
    previous.grounded = true;

    const auto filtered = stellar::server::simulate_movement_tick(
        world, previous, {.wish_direction = {1.0F, 0.0F, 0.0F}}, config, &state);
    const auto unfiltered = stellar::server::simulate_movement_tick(
        world, previous, {.wish_direction = {1.0F, 0.0F, 0.0F}}, config);

    assert(filtered.state.position[0] > 0.55F);
    assert(unfiltered.state.position[0] < 0.52F);
}

} // namespace

int main() {
    spawn_state_uses_player_spawn_marker();
    spawn_state_defaults_to_origin_without_player_spawn();
    empty_world_moves_without_collision();
    floor_world_applies_gravity_and_becomes_grounded();
    wall_world_blocks_authoritative_movement();
    wish_direction_is_clamped();
    nan_input_is_sanitized();
    terminal_fall_speed_is_clamped();
    client_position_is_not_part_of_command();
    runtime_world_collision_optional_is_handled();
    fixed_dt_repeatability_same_inputs_same_outputs();
    collision_state_filter_affects_authoritative_movement();
    return 0;
}
