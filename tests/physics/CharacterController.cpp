#include "stellar/physics/CharacterController.hpp"

#include "stellar/core/WorldAxes.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace {

using Vec3 = std::array<float, 3>;

constexpr float kTolerance = 2.0e-2F;
constexpr Vec3 kZUpFixtureUp = stellar::core::kWorldUp;

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

stellar::assets::LevelCollisionAsset make_asset_from_vector(
    const std::vector<stellar::assets::CollisionTriangle>& triangles) {
    stellar::assets::LevelCollisionAsset asset;
    stellar::assets::CollisionMesh mesh;
    mesh.name = "test";
    mesh.triangles = triangles;
    asset.meshes.push_back(mesh);
    return asset;
}

stellar::assets::LevelCollisionAsset make_two_mesh_asset(
    std::initializer_list<stellar::assets::CollisionTriangle> first,
    std::initializer_list<stellar::assets::CollisionTriangle> second) {
    stellar::assets::LevelCollisionAsset asset;
    stellar::assets::CollisionMesh mesh_a;
    mesh_a.name = "a";
    mesh_a.triangles.assign(first.begin(), first.end());
    stellar::assets::CollisionMesh mesh_b;
    mesh_b.name = "b";
    mesh_b.triangles.assign(second.begin(), second.end());
    asset.meshes.push_back(mesh_a);
    asset.meshes.push_back(mesh_b);
    return asset;
}

stellar::assets::CollisionTriangle floor_triangle(float z = 0.0F) {
    return triangle({-10.0F, -10.0F, z}, {10.0F, -10.0F, z}, {0.0F, 10.0F, z},
                    stellar::core::kWorldUp);
}

stellar::assets::CollisionTriangle wall_x_triangle(float x = 2.0F) {
    return triangle({x, -3.0F, -2.0F}, {x, 3.0F, 3.0F}, {x, 3.0F, -2.0F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::CollisionTriangle wall_x_triangle_2(float x = 2.0F) {
    return triangle({x, -3.0F, -2.0F}, {x, -3.0F, 3.0F}, {x, 3.0F, 3.0F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::CollisionTriangle wall_y_triangle(float y = 2.0F) {
    return triangle({-3.0F, y, -2.0F}, {3.0F, y, -2.0F}, {3.0F, y, 3.0F},
                    {0.0F, -1.0F, 0.0F});
}

stellar::assets::CollisionTriangle angled_wall_triangle() {
    return triangle({2.0F, -2.0F, -2.0F}, {0.0F, 2.0F, 3.0F}, {0.0F, 2.0F, -2.0F},
                    {-0.7071067F, -0.7071067F, 0.0F});
}

stellar::assets::CollisionTriangle slope_triangle(float z1, float z2, Vec3 normal) {
    return triangle({-4.0F, -4.0F, z1}, {4.0F, -4.0F, z1}, {0.0F, 4.0F, z2}, normal);
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

void default_controller_config_uses_inch_scale_player_capsule() {
    const stellar::physics::CharacterControllerConfig cfg;

    assert(nearly_equal(cfg.radius, 16.0F, 0.0F));
    assert(nearly_equal(cfg.height, 72.0F, 0.0F));
    assert(nearly_equal(cfg.skin_width, 0.5F, 0.0F));
    assert(nearly_equal(cfg.step_height, 18.0F, 0.0F));
    assert(nearly_equal(cfg.ground_snap_distance, 4.0F, 0.0F));
}

stellar::physics::CharacterMoveResult move(const stellar::assets::LevelCollisionAsset& asset,
                                           Vec3 position,
                                           Vec3 displacement,
                                           stellar::physics::CharacterControllerConfig cfg = config()) {
    stellar::physics::CollisionWorld world(asset);
    stellar::physics::CharacterController controller(world);
    return controller.move({.position = position,
                            .displacement = displacement,
                            .up = kZUpFixtureUp},
                           cfg);
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
    const auto moved = move(asset, {0.0F, 0.0F, 0.55F}, {0.5F, 0.0F, 0.0F});
    assert(moved.grounded);
    assert(nearly_equal(moved.position[2], 0.9F, 0.03F));
}

void start_overlap_with_floor_recovers() {
    const auto asset = make_asset({floor_triangle()});
    const auto moved = move(asset, {0.0F, 0.0F, 0.3F}, {0.0F, 0.0F, 0.0F});
    assert(moved.started_overlapping);
    assert(moved.position[2] >= 0.89F);
    assert(finite(moved.position));
}

void start_overlap_with_wall_recovers() {
    const auto asset = make_asset({wall_x_triangle(), wall_x_triangle_2()});
    const auto moved = move(asset, {1.7F, 0.0F, 1.0F}, {0.0F, 0.0F, 0.0F});
    assert(moved.started_overlapping);
    assert(moved.position[0] <= 1.51F);
    assert(finite(moved.position));
}

void horizontal_wall_stop() {
    const auto asset = make_asset({wall_x_triangle(), wall_x_triangle_2()});
    const auto moved = move(asset, {0.0F, 0.0F, 1.0F}, {5.0F, 0.0F, 0.0F});
    assert(moved.hit);
    assert(moved.position[0] < 1.52F);
}

void slide_along_axis_aligned_wall() {
    const auto asset = make_asset({wall_x_triangle(), wall_x_triangle_2()});
    const auto moved = move(asset, {0.0F, 0.0F, 1.0F}, {5.0F, 1.0F, 0.0F});
    assert(moved.hit);
    assert(moved.position[0] < 1.52F);
    assert(moved.position[1] > 0.8F);
}

void slide_along_angled_wall() {
    const auto asset = make_asset({angled_wall_triangle()});
    const auto moved = move(asset, {-1.0F, -1.0F, 1.0F}, {4.0F, 4.0F, 0.0F});
    assert(moved.hit);
    assert(finite(moved.position));
    assert(moved.position[0] < 1.8F || moved.position[1] < 1.8F);
}

void corner_collision_stops_cleanly() {
    const auto asset = make_asset({wall_x_triangle(), wall_x_triangle_2(), wall_y_triangle()});
    const auto moved = move(asset, {0.0F, 0.0F, 1.0F}, {5.0F, 5.0F, 0.0F});
    assert(moved.hit);
    assert(moved.position[0] < 1.52F);
    assert(moved.position[1] < 1.52F);
    assert(finite(moved.remaining_displacement));
}

void sphere_catches_triangle_edge() {
    const auto edge = triangle({2.0F, -0.1F, 0.0F}, {2.0F, -0.1F, 2.0F}, {2.0F, 0.1F, 0.0F},
                                {-1.0F, 0.0F, 0.0F});
    const auto asset = make_asset({edge});
    const auto moved = move(asset, {0.0F, 0.3F, 1.8F}, {4.0F, 0.0F, 0.0F});
    assert(moved.hit);
    assert(finite(moved.position));
}

void sphere_catches_triangle_vertex() {
    const auto corner = triangle({2.0F, 0.0F, 0.0F}, {2.0F, 0.0F, 1.0F}, {2.0F, 1.0F, 0.0F},
                                  {-1.0F, 0.0F, 0.0F});
    const auto asset = make_asset({corner});
    const auto moved = move(asset, {0.0F, 0.0F, 1.0F}, {4.0F, 0.0F, 0.0F});
    assert(moved.hit);
    assert(finite(moved.position));
}

void walkable_slope_remains_ground() {
    const auto asset = make_asset({slope_triangle(0.0F, 2.0F, {0.0F, -0.2425356F, 0.9701425F})});
    const auto moved = move(asset, {0.0F, 0.0F, 1.1F}, {0.2F, 0.0F, 0.0F});
    assert(moved.grounded);
    assert(moved.ground_normal[2] > 0.9F);
}

void too_steep_slope_is_not_ground() {
    const auto asset = make_asset({slope_triangle(0.0F, 8.0F, {0.0F, -0.8944272F, 0.4472136F})});
    const auto moved = move(asset, {0.0F, 0.0F, 4.5F}, {0.0F, 0.0F, 0.0F});
    assert(!moved.grounded);
}

void ground_snap_keeps_character_grounded_over_small_drop() {
    const auto asset = make_asset({floor_triangle(0.0F)});
    const auto moved = move(asset, {0.0F, 0.0F, 0.62F}, {0.5F, 0.0F, 0.0F});
    assert(moved.grounded);
    assert(nearly_equal(moved.position[2], 0.9F, 0.03F));
}

stellar::assets::LevelCollisionAsset block_asset(float height) {
    return make_asset({triangle({1.0F, -1.0F, 0.0F}, {1.0F, 1.0F, height},
                                {1.0F, 1.0F, 0.0F}, {-1.0F, 0.0F, 0.0F}),
                       triangle({1.0F, -1.0F, height}, {2.0F, -1.0F, height},
                                {2.0F, 1.0F, height}, stellar::core::kWorldUp),
                       triangle({1.0F, -1.0F, height}, {2.0F, 1.0F, height},
                                {1.0F, 1.0F, height}, stellar::core::kWorldUp)});
}

void step_up_succeeds_for_low_obstacle() {
    const auto asset = block_asset(0.25F);
    const auto moved = move(asset, {0.0F, 0.0F, 0.9F}, {1.6F, 0.0F, 0.0F});
    assert(moved.stepped);
    assert(moved.grounded);
    assert(moved.position[0] > 1.0F);
    assert(nearly_equal(moved.position[2], 1.15F, 0.04F));
}

void step_up_fails_for_high_obstacle() {
    const auto asset = block_asset(0.6F);
    const auto moved = move(asset, {0.0F, 0.0F, 0.9F}, {1.6F, 0.0F, 0.0F});
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

stellar::assets::LevelCollisionAsset many_triangle_wall_world() {
    std::vector<stellar::assets::CollisionTriangle> triangles;
    for (int index = 0; index < 96; ++index) {
        const float x = 50.0F + static_cast<float>(index) * 3.0F;
        triangles.push_back(triangle({x, -1.0F, 0.0F}, {x + 1.0F, 1.0F, 0.0F},
                                     {x + 2.0F, -1.0F, 0.0F}, stellar::core::kWorldUp));
    }
    triangles.push_back(wall_x_triangle());
    triangles.push_back(wall_x_triangle_2());
    return make_asset_from_vector(triangles);
}

void many_triangle_world_matches_local_relevant_result() {
    const auto local_asset = make_asset({wall_x_triangle(), wall_x_triangle_2()});
    const auto many_asset = many_triangle_wall_world();

    const auto local = move(local_asset, {0.0F, 0.0F, 1.0F}, {5.0F, 0.0F, 0.0F});
    const auto many = move(many_asset, {0.0F, 0.0F, 1.0F}, {5.0F, 0.0F, 0.0F});

    assert(many.hit == local.hit);
    assert(nearly_equal(many.position[0], local.position[0]));
    assert(nearly_equal(many.position[1], local.position[1]));
    assert(nearly_equal(many.position[2], local.position[2]));
}

void many_triangle_world_reports_pruned_character_candidates() {
    const auto asset = many_triangle_wall_world();
    stellar::physics::CollisionWorld world(asset);
    stellar::physics::CharacterController controller(world);

    const auto moved = controller.move({.position = {0.0F, 0.0F, 1.0F},
                                        .displacement = {5.0F, 0.0F, 0.0F},
                                        .up = kZUpFixtureUp},
                                       config());
    assert(moved.hit);
    assert(world.stats().triangle_count == 98);
    assert(world.stats().last_query_candidate_count > 0);
    assert(world.stats().last_query_candidate_count < world.stats().triangle_count / 4);
}

void degenerate_triangles_remain_finite_with_candidate_queries() {
    const auto degenerate = triangle({20.0F, 20.0F, 20.0F}, {20.0F, 20.0F, 20.0F},
                                     {20.0F, 20.0F, 20.0F}, {0.0F, 0.0F, 0.0F});
    const auto asset = make_asset({degenerate, wall_x_triangle(), wall_x_triangle_2()});
    const auto moved = move(asset, {0.0F, 0.0F, 1.0F}, {5.0F, 0.0F, 0.0F});
    assert(moved.hit);
    assert(finite(moved.position));
    assert(finite(moved.remaining_displacement));
}

stellar::assets::CollisionTriangle ceiling_triangle(float z = 1.6F) {
    return triangle({-10.0F, -10.0F, z}, {0.0F, 10.0F, z}, {10.0F, -10.0F, z},
                    stellar::core::kWorldDown);
}

stellar::assets::CollisionTriangle high_wall_band_1(float x = 2.0F) {
    return triangle({x, -2.0F, 1.45F}, {x, 2.0F, 2.5F}, {x, 2.0F, 1.45F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::CollisionTriangle high_wall_band_2(float x = 2.0F) {
    return triangle({x, -2.0F, 1.45F}, {x, -2.0F, 2.5F}, {x, 2.0F, 2.5F},
                    {-1.0F, 0.0F, 0.0F});
}

void height_equal_to_two_radii_matches_sphere_like_behavior() {
    auto sphere_cfg = config();
    sphere_cfg.height = sphere_cfg.radius * 2.0F;
    const auto asset = make_asset({floor_triangle()});
    const auto moved = move(asset, {0.0F, 0.0F, 0.55F}, {0.5F, 0.0F, 0.0F}, sphere_cfg);
    assert(moved.grounded);
    assert(nearly_equal(moved.position[2], 0.5F, 0.03F));
}

void capsule_stands_on_floor_with_center_above_ground() {
    const auto asset = make_asset({floor_triangle()});
    const auto moved = move(asset, {0.0F, 0.0F, 0.9F}, {0.0F, 0.0F, 0.0F});
    assert(moved.grounded);
    assert(nearly_equal(moved.position[2], 0.9F, 0.03F));
}

void capsule_wall_collision_uses_body_height_not_only_center() {
    const auto asset = make_asset({high_wall_band_1(), high_wall_band_2()});
    const auto moved = move(asset, {0.0F, 0.0F, 0.8F}, {4.0F, 0.0F, 0.0F});
    assert(moved.hit);
    assert(finite(moved.position));
}

void capsule_ceiling_blocks_upward_movement() {
    const auto asset = make_asset({ceiling_triangle(1.6F)});
    const auto moved = move(asset, {0.0F, 0.0F, 0.6F}, {0.0F, 0.0F, 1.0F});
    assert(moved.hit);
    assert(moved.position[2] < 0.72F);
}

void capsule_start_overlap_with_ceiling_recovers_down_or_stably_out() {
    const auto asset = make_asset({ceiling_triangle(1.6F)});
    const auto moved = move(asset, {0.0F, 0.0F, 0.75F}, {0.0F, 0.0F, 0.0F});
    assert(moved.started_overlapping);
    assert(moved.position[2] <= 0.71F || moved.position[2] >= 2.49F);
    assert(finite(moved.position));
}

void capsule_ground_snap_uses_bottom_endpoint() {
    const auto asset = make_asset({floor_triangle()});
    const auto moved = move(asset, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 0.0F});
    assert(moved.grounded);
    assert(nearly_equal(moved.position[2], 0.9F, 0.03F));
}

void capsule_step_up_does_not_enter_low_ceiling() {
    const auto asset = make_asset({block_asset(0.25F).meshes[0].triangles[0],
                                   block_asset(0.25F).meshes[0].triangles[1],
                                   block_asset(0.25F).meshes[0].triangles[2],
                                   ceiling_triangle(1.95F)});
    const auto moved = move(asset, {0.0F, 0.0F, 0.9F}, {1.6F, 0.0F, 0.0F});
    assert(!moved.stepped);
    assert(moved.position[2] < 1.1F);
}

void capsule_too_tall_for_tunnel_is_blocked() {
    const auto asset = make_asset({floor_triangle(), ceiling_triangle(1.4F)});
    const auto moved = move(asset, {0.0F, 0.0F, 0.9F}, {2.0F, 0.0F, 0.0F});
    assert(moved.started_overlapping || moved.hit);
    assert(finite(moved.position));
}

void capsule_degenerate_height_and_radius_remain_finite() {
    auto cfg = config();
    cfg.radius = -1.0F;
    cfg.height = -1.0F;
    const auto asset = make_asset({floor_triangle()});
    const auto moved = move(asset, {0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, cfg);
    assert(finite(moved.position));
    assert(finite(moved.remaining_displacement));
}

void capsule_slide_into_corner_stops_cleanly() {
    const auto asset = make_asset({wall_x_triangle(), wall_x_triangle_2(), wall_y_triangle()});
    const auto moved = move(asset, {0.0F, 0.0F, 0.9F}, {5.0F, 5.0F, 0.0F});
    assert(moved.hit);
    assert(moved.position[0] < 1.52F);
    assert(moved.position[1] < 1.52F);
    assert(finite(moved.remaining_displacement));
}

void disabled_wall_no_longer_blocks_character() {
    const auto asset = make_two_mesh_asset({wall_x_triangle(), wall_x_triangle_2()}, {});
    stellar::physics::CollisionWorld world(asset);
    stellar::physics::CharacterController controller(world);
    const std::vector<bool> enabled{false, true};

    const auto moved = controller.move({.position = {0.0F, 0.0F, 1.0F},
                                        .displacement = {5.0F, 0.0F, 0.0F},
                                        .up = kZUpFixtureUp},
                                       config(), {.enabled_meshes = &enabled});

    assert(!moved.hit);
    assert(moved.position[0] > 4.9F);
}

void disabled_floor_no_longer_grounds_character() {
    const auto asset = make_two_mesh_asset({floor_triangle()}, {});
    stellar::physics::CollisionWorld world(asset);
    stellar::physics::CharacterController controller(world);
    const std::vector<bool> enabled{false, true};

    const auto moved = controller.move({.position = {0.0F, 0.0F, 0.9F},
                                        .displacement = {0.0F, 0.0F, 0.0F},
                                        .up = kZUpFixtureUp},
                                       config(), {.enabled_meshes = &enabled});

    assert(!moved.grounded);
    assert(!moved.started_overlapping);
}

void step_up_ignores_disabled_step() {
    const auto block = block_asset(0.25F);
    const auto asset = make_two_mesh_asset({block.meshes[0].triangles[0], block.meshes[0].triangles[1],
                                            block.meshes[0].triangles[2]},
                                           {});
    stellar::physics::CollisionWorld world(asset);
    stellar::physics::CharacterController controller(world);
    const std::vector<bool> enabled{false, true};

    const auto moved = controller.move({.position = {0.0F, 0.0F, 0.9F},
                                        .displacement = {1.6F, 0.0F, 0.0F},
                                        .up = kZUpFixtureUp},
                                       config(), {.enabled_meshes = &enabled});

    assert(!moved.stepped);
    assert(!moved.hit);
    assert(moved.position[0] > 1.5F);
}

void all_enabled_filter_matches_existing_behavior() {
    const auto asset = make_asset({wall_x_triangle(), wall_x_triangle_2()});
    stellar::physics::CollisionWorld world(asset);
    stellar::physics::CharacterController controller(world);
    const std::vector<bool> enabled{true};

    const auto unfiltered = controller.move({.position = {0.0F, 0.0F, 1.0F},
                                            .displacement = {5.0F, 0.0F, 0.0F},
                                            .up = kZUpFixtureUp},
                                           config());
    const auto filtered = controller.move({.position = {0.0F, 0.0F, 1.0F},
                                           .displacement = {5.0F, 0.0F, 0.0F},
                                           .up = kZUpFixtureUp},
                                          config(), {.enabled_meshes = &enabled});

    assert(filtered.hit == unfiltered.hit);
    assert(nearly_equal(filtered.position[0], unfiltered.position[0], 0.0F));
    assert(nearly_equal(filtered.position[1], unfiltered.position[1], 0.0F));
    assert(nearly_equal(filtered.position[2], unfiltered.position[2], 0.0F));
}

} // namespace

int main() {
    default_controller_config_uses_inch_scale_player_capsule();
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
    many_triangle_world_matches_local_relevant_result();
    many_triangle_world_reports_pruned_character_candidates();
    degenerate_triangles_remain_finite_with_candidate_queries();
    height_equal_to_two_radii_matches_sphere_like_behavior();
    capsule_stands_on_floor_with_center_above_ground();
    capsule_wall_collision_uses_body_height_not_only_center();
    capsule_ceiling_blocks_upward_movement();
    capsule_start_overlap_with_ceiling_recovers_down_or_stably_out();
    capsule_ground_snap_uses_bottom_endpoint();
    capsule_step_up_does_not_enter_low_ceiling();
    capsule_too_tall_for_tunnel_is_blocked();
    capsule_degenerate_height_and_radius_remain_finite();
    capsule_slide_into_corner_stops_cleanly();
    disabled_wall_no_longer_blocks_character();
    disabled_floor_no_longer_grounds_character();
    step_up_ignores_disabled_step();
    all_enabled_filter_matches_existing_behavior();
    return 0;
}
