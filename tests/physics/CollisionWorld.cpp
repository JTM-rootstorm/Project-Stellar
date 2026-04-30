#include "stellar/physics/CollisionWorld.hpp"

#include <array>
#include <cassert>
#include <cmath>

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

stellar::assets::CollisionTriangle floor_triangle(float y = 0.0F) {
    return triangle({-10.0F, y, -10.0F}, {0.0F, y, 10.0F}, {10.0F, y, -10.0F},
                    {0.0F, 1.0F, 0.0F});
}

stellar::assets::CollisionTriangle wall_x_triangle(float x = 2.0F) {
    return triangle({x, 0.0F, -2.0F}, {x, 0.0F, 2.0F}, {x, 2.0F, 0.0F},
                    {-1.0F, 0.0F, 0.0F});
}

void ray_misses_empty_collision_world() {
    stellar::assets::LevelCollisionAsset asset;
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({0.0F, 1.0F, 0.0F}, {0.0F, -2.0F, 0.0F});
    assert(!hit.hit);
}

void ray_hits_floor_triangle() {
    const auto asset = make_asset({floor_triangle()});
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({0.0F, 2.0F, 0.0F}, {0.0F, -4.0F, 0.0F});
    assert(hit.hit);
    assert(nearly_equal(hit.t, 0.5F));
    assert(nearly_equal(hit.position[1], 0.0F));
    assert(nearly_equal(hit.normal[1], 1.0F));
}

void ray_returns_nearest_hit() {
    const auto near_floor = floor_triangle(0.0F);
    const auto far_floor = floor_triangle(-2.0F);
    const auto asset = make_asset({far_floor, near_floor});
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({0.0F, 3.0F, 0.0F}, {0.0F, -6.0F, 0.0F});
    assert(hit.hit);
    assert(nearly_equal(hit.position[1], 0.0F));
    assert(hit.triangle_index == 1);
}

void ray_misses_outside_triangle_bounds() {
    const auto asset = make_asset({floor_triangle()});
    stellar::physics::CollisionWorld world(asset);

    const auto hit = world.raycast({20.0F, 2.0F, 20.0F}, {0.0F, -4.0F, 0.0F});
    assert(!hit.hit);
}

void horizontal_movement_stops_before_wall() {
    const auto asset = make_asset({wall_x_triangle()});
    stellar::physics::CollisionWorld world(asset);

    const auto moved = world.move_sphere({0.0F, 1.0F, 0.0F}, {5.0F, 0.0F, 0.0F}, 0.5F);
    assert(moved.hit);
    assert(moved.position[0] < 1.501F);
    assert(moved.position[0] > 1.49F);
    assert(nearly_equal(moved.position[2], 0.0F));
}

void horizontal_movement_slides_along_axis_aligned_wall() {
    const auto asset = make_asset({wall_x_triangle()});
    stellar::physics::CollisionWorld world(asset);

    const auto moved = world.move_sphere({0.0F, 1.0F, 0.0F}, {5.0F, 0.0F, 1.0F}, 0.5F);
    assert(moved.hit);
    assert(moved.position[0] < 1.501F);
    assert(moved.position[2] > 0.9F);
}

void downward_ground_probe_detects_floor() {
    const auto asset = make_asset({floor_triangle()});
    stellar::physics::CollisionWorld world(asset);

    const auto ground = world.probe_ground({0.0F, 3.0F, 0.0F}, 5.0F);
    assert(ground.hit);
    assert(nearly_equal(ground.distance, 3.0F));
    assert(nearly_equal(ground.raycast.position[1], 0.0F));
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

} // namespace

int main() {
    ray_misses_empty_collision_world();
    ray_hits_floor_triangle();
    ray_returns_nearest_hit();
    ray_misses_outside_triangle_bounds();
    horizontal_movement_stops_before_wall();
    horizontal_movement_slides_along_axis_aligned_wall();
    downward_ground_probe_detects_floor();
    empty_collision_world_movement_passes_through_unchanged();
    return 0;
}
