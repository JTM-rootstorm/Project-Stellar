#include "stellar/physics/CollisionWorld.hpp"

#include "stellar/core/WorldAxes.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

namespace {

using Vec3 = std::array<float, 3>;

constexpr float kTolerance = 1.0e-3F;

bool nearly_equal(float a, float b, float tolerance = kTolerance) {
    return std::abs(a - b) <= tolerance;
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

stellar::assets::CollisionTriangle floor_triangle(float z = 0.0F) {
    return triangle({-10.0F, -10.0F, z}, {0.0F, 10.0F, z}, {10.0F, -10.0F, z},
                    stellar::core::kWorldUp);
}

stellar::assets::CollisionTriangle wall_x_triangle(float x = 2.0F) {
    return triangle({x, -2.0F, 0.0F}, {x, 0.0F, 2.0F}, {x, 2.0F, 0.0F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::LevelCollisionAsset make_two_mesh_asset(
    stellar::assets::CollisionTriangle first,
    stellar::assets::CollisionTriangle second) {
    stellar::assets::LevelCollisionAsset asset;
    stellar::assets::CollisionMesh mesh_a;
    mesh_a.name = "a";
    mesh_a.triangles.push_back(first);
    stellar::assets::CollisionMesh mesh_b;
    mesh_b.name = "b";
    mesh_b.triangles.push_back(second);
    asset.meshes.push_back(mesh_a);
    asset.meshes.push_back(mesh_b);
    return asset;
}

stellar::assets::LevelCollisionAsset make_many_floor_asset(int count) {
    stellar::assets::LevelCollisionAsset asset;
    stellar::assets::CollisionMesh mesh;
    mesh.name = "many";
    for (int index = 0; index < count; ++index) {
        const float x = static_cast<float>(index) * 4.0F;
        mesh.triangles.push_back(triangle({x, -1.0F, 0.0F}, {x + 1.0F, 1.0F, 0.0F},
                                          {x + 2.0F, -1.0F, 0.0F},
                                          stellar::core::kWorldUp));
    }
    asset.meshes.push_back(mesh);
    return asset;
}

void ray_misses_empty_collision_world() {
    stellar::assets::LevelCollisionAsset asset;
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, -2.0F});
    assert(!hit.hit);

    const auto stats = world.stats();
    assert(stats.mesh_count == 0);
    assert(stats.triangle_count == 0);
    assert(stats.broadphase_node_count == 0);
    assert(stats.last_query_triangle_tests == 0);
}

void query_triangles_empty_world_returns_empty() {
    stellar::assets::LevelCollisionAsset asset;
    stellar::physics::CollisionWorld world(asset);

    const auto candidates = world.query_triangles({.min = {-1.0F, -1.0F, -1.0F},
                                                   .max = {1.0F, 1.0F, 1.0F}});
    assert(candidates.empty());
    assert(world.stats().last_query_candidate_count == 0);
    assert(world.stats().last_query_triangle_tests == 0);
}

void query_triangles_returns_only_intersecting_bounds() {
    const auto asset = make_many_floor_asset(8);
    stellar::physics::CollisionWorld world(asset);

    const auto candidates = world.query_triangles({.min = {3.5F, -2.0F, -0.1F},
                                                   .max = {6.5F, 2.0F, 0.1F}});
    assert(candidates.size() == 1);
    assert(candidates[0].mesh_index == 0);
    assert(candidates[0].triangle_index == 1);
    assert(world.stats().last_query_candidate_count == 1);
    assert(world.stats().last_query_triangle_tests == 1);
}

void query_triangles_order_is_deterministic() {
    stellar::assets::LevelCollisionAsset asset;
    stellar::assets::CollisionMesh mesh_a;
    mesh_a.name = "a";
    mesh_a.triangles.push_back(floor_triangle(0.0F));
    mesh_a.triangles.push_back(floor_triangle(0.0F));
    stellar::assets::CollisionMesh mesh_b;
    mesh_b.name = "b";
    mesh_b.triangles.push_back(floor_triangle(0.0F));
    asset.meshes.push_back(mesh_a);
    asset.meshes.push_back(mesh_b);
    stellar::physics::CollisionWorld world(asset);

    const auto first = world.query_triangles({.min = {-20.0F, -20.0F, -0.1F},
                                              .max = {20.0F, 20.0F, 0.1F}});
    const auto second = world.query_triangles({.min = {-20.0F, -20.0F, -0.1F},
                                               .max = {20.0F, 20.0F, 0.1F}});
    assert(first.size() == 3);
    assert(second.size() == first.size());
    assert(first[0].mesh_index == 0 && first[0].triangle_index == 0);
    assert(first[1].mesh_index == 0 && first[1].triangle_index == 1);
    assert(first[2].mesh_index == 1 && first[2].triangle_index == 0);
    for (std::size_t index = 0; index < first.size(); ++index) {
        assert(first[index].mesh_index == second[index].mesh_index);
        assert(first[index].triangle_index == second[index].triangle_index);
    }
}

void query_triangles_handles_degenerate_bounds() {
    const auto asset = make_asset({floor_triangle()});
    stellar::physics::CollisionWorld world(asset);

    const auto touching = world.query_triangles({.min = {0.0F, 0.0F, 0.0F},
                                                 .max = {0.0F, 0.0F, 0.0F}});
    assert(touching.size() == 1);

    const auto inverted = world.query_triangles({.min = {1.0F, 1.0F, 1.0F},
                                                 .max = {0.0F, 0.0F, 0.0F}});
    assert(inverted.empty());

    const float infinity = std::numeric_limits<float>::infinity();
    const auto non_finite = world.query_triangles({.min = {-infinity, -1.0F, -1.0F},
                                                   .max = {1.0F, 1.0F, 1.0F}});
    assert(non_finite.empty());
}

void ray_hits_floor_triangle() {
    const auto asset = make_asset({floor_triangle()});
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({0.0F, 0.0F, 2.0F}, {0.0F, 0.0F, -4.0F});
    assert(hit.hit);
    assert(nearly_equal(hit.t, 0.5F));
    assert(nearly_equal(hit.position[2], 0.0F));
    assert(nearly_equal(hit.normal[2], 1.0F));

    const auto stats = world.stats();
    assert(stats.mesh_count == 1);
    assert(stats.triangle_count == 1);
    assert(stats.broadphase_node_count == 1);
    assert(stats.last_query_triangle_tests == 1);
}

void bvh_raycast_matches_bruteforce_order_for_equal_hits() {
    const auto first = floor_triangle(0.0F);
    const auto second = floor_triangle(0.0F);
    const auto asset = make_asset({second, first});
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({0.0F, 0.0F, 2.0F}, {0.0F, 0.0F, -4.0F});
    assert(hit.hit);
    assert(nearly_equal(hit.t, 0.5F));
    assert(hit.mesh_index == 0);
    assert(hit.triangle_index == 0);
}

void ray_returns_nearest_hit() {
    const auto near_floor = floor_triangle(0.0F);
    const auto far_floor = floor_triangle(-2.0F);
    const auto asset = make_asset({far_floor, near_floor});
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({0.0F, 0.0F, 3.0F}, {0.0F, 0.0F, -6.0F});
    assert(hit.hit);
    assert(nearly_equal(hit.position[2], 0.0F));
    assert(hit.triangle_index == 1);
}

void ray_misses_outside_triangle_bounds() {
    const auto asset = make_asset({floor_triangle()});
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({20.0F, 20.0F, 2.0F}, {0.0F, 0.0F, -4.0F});
    assert(!hit.hit);
}

void horizontal_movement_stops_before_wall() {
    const auto asset = make_asset({wall_x_triangle()});
    stellar::physics::CollisionWorld world(asset);

    const auto moved = world.move_sphere({0.0F, 0.0F, 1.0F}, {5.0F, 0.0F, 0.0F}, 0.5F);
    assert(moved.hit);
    assert(moved.position[0] < 1.501F);
    assert(moved.position[0] > 1.49F);
    assert(nearly_equal(moved.position[1], 0.0F));
}

void horizontal_movement_slides_along_axis_aligned_wall() {
    const auto asset = make_asset({wall_x_triangle()});
    stellar::physics::CollisionWorld world(asset);

    const auto moved = world.move_sphere({0.0F, 0.0F, 1.0F}, {5.0F, 1.0F, 0.0F}, 0.5F);
    assert(moved.hit);
    assert(moved.position[0] < 1.501F);
    assert(moved.position[1] > 0.9F);
}

void downward_ground_probe_detects_floor() {
    auto floor = floor_triangle();
    floor.surface.source_material_name = "metal/grate01";
    floor.surface.footstep_surface_id = "metal";
    const auto asset = make_asset({floor});
    stellar::physics::CollisionWorld world(asset);

    const auto ground = world.probe_ground({0.0F, 0.0F, 3.0F}, 5.0F);
    assert(ground.hit);
    assert(nearly_equal(ground.distance, 3.0F));
    assert(nearly_equal(ground.raycast.position[2], 0.0F));
    assert(ground.raycast.mesh_index == 0);
    assert(ground.raycast.triangle_index == 0);
    const auto &hit_triangle =
        world.asset().meshes[ground.raycast.mesh_index].triangles[ground.raycast.triangle_index];
    assert(hit_triangle.surface.source_material_name == "metal/grate01");
    assert(hit_triangle.surface.footstep_surface_id == "metal");
}

void empty_collision_world_movement_passes_through_unchanged() {
    stellar::assets::LevelCollisionAsset asset;
    stellar::physics::CollisionWorld world(asset);

    const auto moved = world.move_sphere({1.0F, 2.0F, 3.0F}, {4.0F, 5.0F, 6.0F}, 0.5F);
    assert(!moved.hit);
    assert(nearly_equal(moved.position[0], 5.0F));
    assert(nearly_equal(moved.position[1], 7.0F));
    assert(nearly_equal(moved.position[2], 9.0F));
    assert(nearly_equal(moved.velocity[0], 0.0F));
    assert(nearly_equal(moved.velocity[1], 0.0F));
    assert(nearly_equal(moved.velocity[2], 0.0F));
}

void bvh_handles_many_triangles_deterministically_and_prunes() {
    const auto asset = make_many_floor_asset(64);
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({5.0F, 0.0F, 2.0F}, {0.0F, 0.0F, -4.0F});
    assert(hit.hit);
    assert(hit.triangle_index == 1);
    assert(nearly_equal(hit.position[2], 0.0F));

    const auto first_stats = world.stats();
    assert(first_stats.triangle_count == 64);
    assert(first_stats.broadphase_node_count > 1);
    assert(first_stats.last_query_triangle_tests > 0);
    assert(first_stats.last_query_triangle_tests < first_stats.triangle_count);

    const auto repeated = world.raycast({5.0F, 0.0F, 2.0F}, {0.0F, 0.0F, -4.0F});
    assert(repeated.hit);
    assert(repeated.triangle_index == hit.triangle_index);
    assert(nearly_equal(repeated.t, hit.t));
}

void bvh_miss_prunes_all_distant_triangles() {
    const auto asset = make_many_floor_asset(32);
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({0.0F, 100.0F, 2.0F}, {0.0F, 0.0F, -4.0F});
    assert(!hit.hit);
    assert(world.stats().last_query_triangle_tests == 0);
}

void degenerate_triangles_do_not_crash_build_or_query() {
    const auto degenerate = triangle({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F},
                                     {0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F});
    const auto asset = make_asset({degenerate, floor_triangle()});
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({0.0F, 0.0F, 2.0F}, {0.0F, 0.0F, -4.0F});
    assert(hit.hit);
    assert(hit.triangle_index == 1);
    assert(world.stats().triangle_count == 2);
}

void bvh_movement_query_prunes_candidates() {
    auto asset = make_many_floor_asset(64);
    asset.meshes[0].triangles.push_back(wall_x_triangle(5.0F));
    stellar::physics::CollisionWorld world(asset);

    const auto moved = world.move_sphere({3.0F, 0.0F, 1.0F}, {4.0F, 0.0F, 0.0F}, 0.5F);
    assert(moved.hit);
    assert(moved.position[0] < 4.501F);
    assert(moved.position[0] > 4.49F);
    assert(world.stats().last_query_triangle_tests > 0);
    assert(world.stats().last_query_triangle_tests < world.stats().triangle_count);
}

void query_triangles_filter_excludes_disabled_mesh() {
    const auto asset = make_two_mesh_asset(floor_triangle(), floor_triangle());
    stellar::physics::CollisionWorld world(asset);
    const std::vector<bool> enabled{true, false};

    const auto candidates = world.query_triangles({.min = {-20.0F, -20.0F, -0.1F},
                                                   .max = {20.0F, 20.0F, 0.1F}},
                                                  {.enabled_meshes = &enabled});

    assert(candidates.size() == 1);
    assert(candidates[0].mesh_index == 0);
    assert(world.stats().last_query_candidate_count == 1);
}

void raycast_ignores_disabled_mesh() {
    const auto asset = make_two_mesh_asset(floor_triangle(0.0F), floor_triangle(1.0F));
    stellar::physics::CollisionWorld world(asset);
    const std::vector<bool> enabled{true, false};

    const auto hit = world.raycast({0.0F, 0.0F, 2.0F}, {0.0F, 0.0F, -4.0F},
                                   {.enabled_meshes = &enabled});

    assert(hit.hit);
    assert(hit.mesh_index == 0);
    assert(nearly_equal(hit.position[2], 0.0F));
}

void probe_ground_ignores_disabled_floor() {
    const auto asset = make_two_mesh_asset(floor_triangle(), floor_triangle());
    stellar::physics::CollisionWorld world(asset);
    const std::vector<bool> enabled{false, false};

    const auto ground = world.probe_ground({0.0F, 0.0F, 2.0F}, 4.0F, 0.5F,
                                           {.enabled_meshes = &enabled});

    assert(!ground.hit);
}

void move_sphere_ignores_disabled_wall() {
    const auto asset = make_two_mesh_asset(wall_x_triangle(2.0F), floor_triangle(-5.0F));
    stellar::physics::CollisionWorld world(asset);
    const std::vector<bool> enabled{false, true};

    const auto moved = world.move_sphere({0.0F, 0.0F, 1.0F}, {5.0F, 0.0F, 0.0F}, 0.5F, 3,
                                         {.enabled_meshes = &enabled});

    assert(!moved.hit);
    assert(moved.position[0] > 4.99F);
}

void filter_preserves_deterministic_order() {
    stellar::assets::LevelCollisionAsset asset;
    stellar::assets::CollisionMesh mesh_a;
    mesh_a.triangles.push_back(floor_triangle());
    stellar::assets::CollisionMesh mesh_b;
    mesh_b.triangles.push_back(floor_triangle());
    stellar::assets::CollisionMesh mesh_c;
    mesh_c.triangles.push_back(floor_triangle());
    asset.meshes = {mesh_a, mesh_b, mesh_c};
    stellar::physics::CollisionWorld world(asset);
    const std::vector<bool> enabled{true, false, true};

    const auto candidates = world.query_triangles({.min = {-20.0F, -20.0F, -0.1F},
                                                   .max = {20.0F, 20.0F, 0.1F}},
                                                  {.enabled_meshes = &enabled});

    assert(candidates.size() == 2);
    assert(candidates[0].mesh_index == 0);
    assert(candidates[1].mesh_index == 2);
}

void filter_out_of_range_policy_is_tested() {
    const auto asset = make_two_mesh_asset(floor_triangle(), floor_triangle());
    stellar::physics::CollisionWorld world(asset);
    const std::vector<bool> enabled{true};

    assert(stellar::physics::collision_mesh_passes_filter({.enabled_meshes = &enabled}, 0));
    assert(!stellar::physics::collision_mesh_passes_filter({.enabled_meshes = &enabled}, 1));
    const auto candidates = world.query_triangles({.min = {-20.0F, -20.0F, -0.1F},
                                                   .max = {20.0F, 20.0F, 0.1F}},
                                                  {.enabled_meshes = &enabled});
    assert(candidates.size() == 1);
    assert(candidates[0].mesh_index == 0);
}

} // namespace

int main() {
    ray_misses_empty_collision_world();
    query_triangles_empty_world_returns_empty();
    query_triangles_returns_only_intersecting_bounds();
    query_triangles_order_is_deterministic();
    query_triangles_handles_degenerate_bounds();
    ray_hits_floor_triangle();
    bvh_raycast_matches_bruteforce_order_for_equal_hits();
    ray_returns_nearest_hit();
    ray_misses_outside_triangle_bounds();
    horizontal_movement_stops_before_wall();
    horizontal_movement_slides_along_axis_aligned_wall();
    downward_ground_probe_detects_floor();
    empty_collision_world_movement_passes_through_unchanged();
    bvh_handles_many_triangles_deterministically_and_prunes();
    bvh_miss_prunes_all_distant_triangles();
    degenerate_triangles_do_not_crash_build_or_query();
    bvh_movement_query_prunes_candidates();
    query_triangles_filter_excludes_disabled_mesh();
    raycast_ignores_disabled_mesh();
    probe_ground_ignores_disabled_floor();
    move_sphere_ignores_disabled_wall();
    filter_preserves_deterministic_order();
    filter_out_of_range_policy_is_tested();
    return 0;
}
