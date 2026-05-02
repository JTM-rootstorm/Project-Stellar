#include "stellar/physics/CharacterController.hpp"
#include "stellar/physics/CollisionWorld.hpp"

#include "stellar/core/WorldAxes.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
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
    std::vector<stellar::assets::CollisionTriangle> triangles, std::string name = "test") {
    stellar::assets::LevelCollisionAsset asset;
    stellar::assets::CollisionMesh mesh;
    mesh.name = std::move(name);
    mesh.triangles = std::move(triangles);
    asset.meshes.push_back(std::move(mesh));
    return asset;
}

stellar::assets::CollisionTriangle floor_a(float x0, float y0, float size = 1.0F) {
    return triangle({x0, y0, 0.0F}, {x0 + size, y0, 0.0F}, {x0, y0 + size, 0.0F},
                    stellar::core::kWorldUp);
}

stellar::assets::CollisionTriangle floor_b(float x0, float y0, float size = 1.0F) {
    return triangle({x0 + size, y0, 0.0F}, {x0 + size, y0 + size, 0.0F},
                    {x0, y0 + size, 0.0F}, stellar::core::kWorldUp);
}

stellar::assets::CollisionTriangle wall_x_a(float x = 2.0F) {
    return triangle({x, -3.0F, -1.0F}, {x, 3.0F, 3.0F}, {x, 3.0F, -1.0F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::CollisionTriangle wall_x_b(float x = 2.0F) {
    return triangle({x, -3.0F, -1.0F}, {x, -3.0F, 3.0F}, {x, 3.0F, 3.0F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::LevelCollisionAsset make_grid_floor(int side) {
    std::vector<stellar::assets::CollisionTriangle> triangles;
    triangles.reserve(static_cast<std::size_t>(side * side * 2));
    const float origin = -static_cast<float>(side) * 0.5F;
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            const float x0 = origin + static_cast<float>(x);
            const float y0 = origin + static_cast<float>(y);
            triangles.push_back(floor_a(x0, y0));
            triangles.push_back(floor_b(x0, y0));
        }
    }
    return make_asset_from_vector(std::move(triangles), "grid_floor");
}

std::vector<stellar::assets::CollisionTriangle> distant_triangle_cloud(int count) {
    std::vector<stellar::assets::CollisionTriangle> triangles;
    triangles.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        const float x = 100.0F + static_cast<float>(index % 32) * 3.0F;
        const float y = 100.0F + static_cast<float>(index / 32) * 3.0F;
        triangles.push_back(floor_a(x, y, 1.0F));
    }
    return triangles;
}

stellar::assets::LevelCollisionAsset make_corridor_with_distant_cloud(int distant_count) {
    std::vector<stellar::assets::CollisionTriangle> triangles;
    triangles.push_back(floor_a(-4.0F, -4.0F, 8.0F));
    triangles.push_back(floor_b(-4.0F, -4.0F, 8.0F));
    triangles.push_back(wall_x_a());
    triangles.push_back(wall_x_b());
    auto distant = distant_triangle_cloud(distant_count);
    triangles.insert(triangles.end(), distant.begin(), distant.end());
    return make_asset_from_vector(std::move(triangles), "corridor_cloud");
}

stellar::assets::LevelCollisionAsset make_degenerate_set() {
    std::vector<stellar::assets::CollisionTriangle> triangles;
    for (int index = 0; index < 64; ++index) {
        const float value = static_cast<float>(index);
        triangles.push_back(triangle({value, value, value}, {value, value, value},
                                     {value, value, value}, {0.0F, 0.0F, 0.0F}));
    }
    triangles.push_back(floor_a(-1.0F, -1.0F, 2.0F));
    triangles.push_back(floor_b(-1.0F, -1.0F, 2.0F));
    return make_asset_from_vector(std::move(triangles), "degenerate_set");
}

stellar::physics::CharacterControllerConfig character_config() {
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

void raycast_large_scene_prunes_triangle_tests() {
    const auto asset = make_grid_floor(48);
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({0.25F, 0.25F, 5.0F}, {0.0F, 0.0F, -10.0F});
    const auto stats = world.stats();

    assert(hit.hit);
    assert(nearly_equal(hit.position[2], 0.0F));
    assert(stats.triangle_count == 48U * 48U * 2U);
    assert(stats.last_query_triangle_tests > 0);
    assert(stats.last_query_triangle_tests < stats.triangle_count / 4U);
}

void raycast_equal_hits_remain_deterministic() {
    const auto asset = make_asset({floor_a(-1.0F, -1.0F, 2.0F), floor_a(-1.0F, -1.0F, 2.0F)});
    stellar::physics::CollisionWorld world(asset);

    const auto first = world.raycast({0.0F, 0.0F, 2.0F}, {0.0F, 0.0F, -4.0F});
    const auto second = world.raycast({0.0F, 0.0F, 2.0F}, {0.0F, 0.0F, -4.0F});

    assert(first.hit);
    assert(second.hit);
    assert(first.mesh_index == 0);
    assert(first.triangle_index == 0);
    assert(second.mesh_index == first.mesh_index);
    assert(second.triangle_index == first.triangle_index);
    assert(nearly_equal(second.t, first.t));
    assert(nearly_equal(second.position, first.position));
}

void character_move_large_scene_prunes_candidate_tests() {
    const auto asset = make_corridor_with_distant_cloud(512);
    stellar::physics::CollisionWorld world(asset);
    stellar::physics::CharacterController controller(world);

    const auto moved = controller.move({.position = {0.0F, 0.0F, 0.9F},
                                        .displacement = {5.0F, 0.0F, 0.0F}},
                                       character_config());
    const auto stats = world.stats();

    assert(moved.hit);
    assert(moved.position[0] < 1.52F);
    assert(stats.triangle_count == 516U);
    assert(stats.last_query_candidate_count > 0);
    assert(stats.last_query_candidate_count < stats.triangle_count / 4U);
}

void character_move_result_stable_across_repeated_runs() {
    const auto asset = make_corridor_with_distant_cloud(256);
    stellar::physics::CollisionWorld world(asset);
    stellar::physics::CharacterController controller(world);
    const stellar::physics::CharacterMoveInput input{.position = {0.0F, 0.0F, 0.9F},
                                                     .displacement = {5.0F, 1.0F, 0.0F}};

    const auto first = controller.move(input, character_config());
    const auto first_stats = world.stats();
    const auto second = controller.move(input, character_config());
    const auto second_stats = world.stats();

    assert(first.hit == second.hit);
    assert(first.grounded == second.grounded);
    assert(first.stepped == second.stepped);
    assert(first.started_overlapping == second.started_overlapping);
    assert(first.iterations == second.iterations);
    assert(nearly_equal(first.position, second.position));
    assert(nearly_equal(first.remaining_displacement, second.remaining_displacement));
    assert(first_stats.last_query_candidate_count == second_stats.last_query_candidate_count);
}

void degenerate_triangle_cloud_does_not_poison_stats() {
    const auto asset = make_degenerate_set();
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({0.0F, 0.0F, 2.0F}, {0.0F, 0.0F, -4.0F});
    const auto stats = world.stats();

    assert(hit.hit);
    assert(finite(hit.position));
    assert(stats.triangle_count == 66U);
    assert(stats.last_query_triangle_tests > 0);
    assert(stats.last_query_triangle_tests <= stats.triangle_count);

    const auto candidates = world.query_triangles({.min = {-0.5F, -0.5F, -0.1F},
                                                   .max = {0.5F, 0.5F, 0.1F}});
    assert(!candidates.empty());
    assert(world.stats().last_query_candidate_count == candidates.size());
    assert(world.stats().last_query_triangle_tests == candidates.size());
}

void empty_world_stats_are_stable() {
    stellar::assets::LevelCollisionAsset asset;
    stellar::physics::CollisionWorld world(asset);

    const auto miss = world.raycast({0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, -2.0F});
    const auto after_raycast = world.stats();
    const auto candidates = world.query_triangles({.min = {-1.0F, -1.0F, -1.0F},
                                                   .max = {1.0F, 1.0F, 1.0F}});
    const auto after_query = world.stats();

    assert(!miss.hit);
    assert(candidates.empty());
    assert(after_raycast.mesh_count == 0);
    assert(after_raycast.triangle_count == 0);
    assert(after_raycast.last_query_triangle_tests == 0);
    assert(after_query.mesh_count == 0);
    assert(after_query.triangle_count == 0);
    assert(after_query.last_query_candidate_count == 0);
    assert(after_query.last_query_triangle_tests == 0);
}

void single_triangle_world_stats_are_stable() {
    const auto asset = make_asset({floor_a(-1.0F, -1.0F, 2.0F)});
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({0.0F, 0.0F, 2.0F}, {0.0F, 0.0F, -4.0F});
    const auto first = world.stats();
    const auto repeated = world.raycast({0.0F, 0.0F, 2.0F}, {0.0F, 0.0F, -4.0F});
    const auto second = world.stats();

    assert(hit.hit);
    assert(repeated.hit);
    assert(first.mesh_count == 1);
    assert(first.triangle_count == 1);
    assert(first.last_query_triangle_tests == 1);
    assert(second.last_query_triangle_tests == first.last_query_triangle_tests);
    assert(second.last_query_candidate_count == first.last_query_candidate_count);
}

void distant_geometry_does_not_affect_local_movement_result() {
    const auto local_asset = make_corridor_with_distant_cloud(0);
    const auto cloud_asset = make_corridor_with_distant_cloud(768);
    stellar::physics::CollisionWorld local_world(local_asset);
    stellar::physics::CollisionWorld cloud_world(cloud_asset);
    stellar::physics::CharacterController local_controller(local_world);
    stellar::physics::CharacterController cloud_controller(cloud_world);
    const stellar::physics::CharacterMoveInput input{.position = {0.0F, 0.0F, 0.9F},
                                                     .displacement = {5.0F, 1.0F, 0.0F}};

    const auto local = local_controller.move(input, character_config());
    const auto cloud = cloud_controller.move(input, character_config());

    assert(local.hit == cloud.hit);
    assert(local.grounded == cloud.grounded);
    assert(local.stepped == cloud.stepped);
    assert(local.started_overlapping == cloud.started_overlapping);
    assert(local.iterations == cloud.iterations);
    assert(nearly_equal(local.position, cloud.position));
    assert(nearly_equal(local.remaining_displacement, cloud.remaining_displacement));
}

} // namespace

int main() {
    raycast_large_scene_prunes_triangle_tests();
    raycast_equal_hits_remain_deterministic();
    character_move_large_scene_prunes_candidate_tests();
    character_move_result_stable_across_repeated_runs();
    degenerate_triangle_cloud_does_not_poison_stats();
    empty_world_stats_are_stable();
    single_triangle_world_stats_are_stable();
    distant_geometry_does_not_affect_local_movement_result();
    return 0;
}
