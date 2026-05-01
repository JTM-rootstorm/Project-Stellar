#include "stellar/physics/CharacterController.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <initializer_list>

namespace {

using Vec3 = std::array<float, 3>;

constexpr float kTolerance = 2.0e-2F;

bool nearly_equal(float a, float b, float tolerance = kTolerance) {
    return std::abs(a - b) <= tolerance;
}

bool finite(Vec3 value) {
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

stellar::assets::CollisionTriangle triangle(Vec3 a, Vec3 b, Vec3 c, Vec3 normal) {
    return stellar::assets::CollisionTriangle{.a = a, .b = b, .c = c, .normal = normal};
}

stellar::assets::LevelCollisionAsset make_asset(
    std::initializer_list<stellar::assets::CollisionTriangle> triangles) {
    stellar::assets::LevelCollisionAsset asset;
    stellar::assets::CollisionMesh mesh;
    mesh.name = "test";
    mesh.triangles.assign(triangles.begin(), triangles.end());
    asset.meshes.push_back(mesh);
    return asset;
}

stellar::assets::CollisionTriangle floor_triangle(float y = 0.0F) {
    return triangle({-10.0F, y, -10.0F}, {10.0F, y, -10.0F}, {0.0F, y, 10.0F},
                    {0.0F, 1.0F, 0.0F});
}

stellar::assets::CollisionTriangle wall_x_triangle(float x = 2.0F) {
    return triangle({x, -2.0F, -3.0F}, {x, 3.0F, 3.0F}, {x, -2.0F, 3.0F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::CollisionTriangle wall_x_triangle_2(float x = 2.0F) {
    return triangle({x, -2.0F, -3.0F}, {x, 3.0F, -3.0F}, {x, 3.0F, 3.0F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::CollisionTriangle wall_z_triangle(float z = 2.0F) {
    return triangle({-3.0F, -2.0F, z}, {3.0F, -2.0F, z}, {3.0F, 3.0F, z},
                    {0.0F, 0.0F, -1.0F});
}

stellar::assets::CollisionTriangle angled_wall_triangle() {
    return triangle({2.0F, -2.0F, -2.0F}, {0.0F, 3.0F, 2.0F}, {0.0F, -2.0F, 2.0F},
                    {-0.7071067F, 0.0F, -0.7071067F});
}

stellar::assets::CollisionTriangle slope_triangle(float y1, float y2, Vec3 normal) {
    return triangle({-4.0F, y1, -4.0F}, {4.0F, y1, -4.0F}, {0.0F, y2, 4.0F}, normal);
}

stellar::physics::CharacterControllerConfig config() {
    stellar::physics::CharacterControllerConfig cfg;
    cfg.radius = 0.5F;
    cfg.height = 1.8F;
    cfg.skin_width = 0.0F;
    cfg.max_slope_degrees = 50.0F;
    cfg.step_height = 0.35F;
    cfg.ground_snap_distance = 0.2F;
    cfg.max_slide_iterations = 4;
    return cfg;
}

stellar::physics::CharacterMoveResult move(const stellar::assets::LevelCollisionAsset& asset,
                                           Vec3 position,
                                           Vec3 displacement,
                                           stellar::physics::CharacterControllerConfig cfg = config()) {
    stellar::physics::CollisionWorld world(asset);
    stellar::physics::CharacterController controller(world);
    return controller.move({.position = position, .displacement = displacement}, cfg);
}

void empty_world_movement_passes_through_unchanged() {
    stellar::assets::LevelCollisionAsset asset;
    const auto moved = move(asset, {1.0F, 2.0F, 3.0F}, {4.0F, 5.0F, 6.0F});
    assert(!moved.hit);
    assert(nearly_equal(moved.position[0], 5.0F));
    assert(nearly_equal(moved.position[1], 7.0F));
    assert(nearly_equal(moved.position[2], 9.0F));
}

void grounded_state_detected_on_floor() {
    const auto asset = make_asset({floor_triangle()});
    const auto moved = move(asset, {0.0F, 0.55F, 0.0F}, {0.5F, 0.0F, 0.0F});
    assert(moved.grounded);
    assert(nearly_equal(moved.position[1], 0.5F, 0.03F));
}

void start_overlap_with_floor_recovers() {
    const auto asset = make_asset({floor_triangle()});
    const auto moved = move(asset, {0.0F, 0.3F, 0.0F}, {0.0F, 0.0F, 0.0F});
    assert(moved.started_overlapping);
    assert(moved.position[1] >= 0.49F);
    assert(finite(moved.position));
}

void start_overlap_with_wall_recovers() {
    const auto asset = make_asset({wall_x_triangle(), wall_x_triangle_2()});
    const auto moved = move(asset, {1.7F, 1.0F, 0.0F}, {0.0F, 0.0F, 0.0F});
    assert(moved.started_overlapping);
    assert(moved.position[0] <= 1.51F);
    assert(finite(moved.position));
}

void horizontal_wall_stop() {
    const auto asset = make_asset({wall_x_triangle(), wall_x_triangle_2()});
    const auto moved = move(asset, {0.0F, 1.0F, 0.0F}, {5.0F, 0.0F, 0.0F});
    assert(moved.hit);
    assert(moved.position[0] < 1.52F);
}

void slide_along_axis_aligned_wall() {
    const auto asset = make_asset({wall_x_triangle(), wall_x_triangle_2()});
    const auto moved = move(asset, {0.0F, 1.0F, 0.0F}, {5.0F, 0.0F, 1.0F});
    assert(moved.hit);
    assert(moved.position[0] < 1.52F);
    assert(moved.position[2] > 0.8F);
}

void slide_along_angled_wall() {
    const auto asset = make_asset({angled_wall_triangle()});
    const auto moved = move(asset, {-1.0F, 1.0F, -1.0F}, {4.0F, 0.0F, 4.0F});
    assert(moved.hit);
    assert(finite(moved.position));
    assert(moved.position[0] < 1.8F || moved.position[2] < 1.8F);
}

void corner_collision_stops_cleanly() {
    const auto asset = make_asset({wall_x_triangle(), wall_x_triangle_2(), wall_z_triangle()});
    const auto moved = move(asset, {0.0F, 1.0F, 0.0F}, {5.0F, 0.0F, 5.0F});
    assert(moved.hit);
    assert(moved.position[0] < 1.52F);
    assert(moved.position[2] < 1.52F);
    assert(finite(moved.remaining_displacement));
}

void sphere_catches_triangle_edge() {
    const auto edge = triangle({2.0F, 0.0F, -0.1F}, {2.0F, 2.0F, -0.1F}, {2.0F, 0.0F, 0.1F},
                               {-1.0F, 0.0F, 0.0F});
    const auto asset = make_asset({edge});
    const auto moved = move(asset, {0.0F, 1.8F, 0.3F}, {4.0F, 0.0F, 0.0F});
    assert(moved.hit);
    assert(finite(moved.position));
}

void sphere_catches_triangle_vertex() {
    const auto corner = triangle({2.0F, 0.0F, 0.0F}, {2.0F, 1.0F, 0.0F}, {2.0F, 0.0F, 1.0F},
                                 {-1.0F, 0.0F, 0.0F});
    const auto asset = make_asset({corner});
    const auto moved = move(asset, {0.0F, 1.0F, 0.0F}, {4.0F, 0.0F, 0.0F});
    assert(moved.hit);
    assert(finite(moved.position));
}

void walkable_slope_remains_ground() {
    const auto asset = make_asset({slope_triangle(0.0F, 2.0F, {0.0F, 0.9701425F, -0.2425356F})});
    const auto moved = move(asset, {0.0F, 1.1F, 0.0F}, {0.2F, 0.0F, 0.0F});
    assert(moved.grounded);
    assert(moved.ground_normal[1] > 0.9F);
}

void too_steep_slope_is_not_ground() {
    const auto asset = make_asset({slope_triangle(0.0F, 8.0F, {0.0F, 0.4472136F, -0.8944272F})});
    const auto moved = move(asset, {0.0F, 4.5F, 0.0F}, {0.0F, 0.0F, 0.0F});
    assert(!moved.grounded);
}

void ground_snap_keeps_character_grounded_over_small_drop() {
    const auto asset = make_asset({floor_triangle(0.0F)});
    const auto moved = move(asset, {0.0F, 0.62F, 0.0F}, {0.5F, 0.0F, 0.0F});
    assert(moved.grounded);
    assert(nearly_equal(moved.position[1], 0.5F, 0.03F));
}

stellar::assets::LevelCollisionAsset block_asset(float height) {
    return make_asset({triangle({1.0F, 0.0F, -1.0F}, {1.0F, height, 1.0F},
                                {1.0F, 0.0F, 1.0F}, {-1.0F, 0.0F, 0.0F}),
                       triangle({1.0F, height, -1.0F}, {2.0F, height, -1.0F},
                                {2.0F, height, 1.0F}, {0.0F, 1.0F, 0.0F}),
                       triangle({1.0F, height, -1.0F}, {2.0F, height, 1.0F},
                                {1.0F, height, 1.0F}, {0.0F, 1.0F, 0.0F})});
}

void step_up_succeeds_for_low_obstacle() {
    const auto asset = block_asset(0.25F);
    const auto moved = move(asset, {0.0F, 0.5F, 0.0F}, {1.6F, 0.0F, 0.0F});
    assert(moved.stepped);
    assert(moved.grounded);
    assert(moved.position[0] > 1.0F);
    assert(nearly_equal(moved.position[1], 0.75F, 0.04F));
}

void step_up_fails_for_high_obstacle() {
    const auto asset = block_asset(0.6F);
    const auto moved = move(asset, {0.0F, 0.5F, 0.0F}, {1.6F, 0.0F, 0.0F});
    assert(!moved.stepped);
    assert(moved.position[0] < 0.55F);
}

void degenerate_triangles_and_zero_displacement_stay_finite() {
    const auto degenerate = triangle({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F},
                                     {0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F});
    const auto asset = make_asset({degenerate});
    const auto moved = move(asset, {0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F});
    assert(finite(moved.position));
    assert(finite(moved.remaining_displacement));
}

} // namespace

int main() {
    empty_world_movement_passes_through_unchanged();
    grounded_state_detected_on_floor();
    start_overlap_with_floor_recovers();
    start_overlap_with_wall_recovers();
    horizontal_wall_stop();
    slide_along_axis_aligned_wall();
    slide_along_angled_wall();
    corner_collision_stops_cleanly();
    sphere_catches_triangle_edge();
    sphere_catches_triangle_vertex();
    walkable_slope_remains_ground();
    too_steep_slope_is_not_ground();
    ground_snap_keeps_character_grounded_over_small_drop();
    step_up_succeeds_for_low_obstacle();
    step_up_fails_for_high_obstacle();
    degenerate_triangles_and_zero_displacement_stay_finite();
    return 0;
}
