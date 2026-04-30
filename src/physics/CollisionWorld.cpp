#include "stellar/physics/CollisionWorld.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace stellar::physics {
namespace {

constexpr float kEpsilon = 1.0e-5F;
constexpr float kContactOffset = 1.0e-4F;

using Vec3 = std::array<float, 3>;

Vec3 add(Vec3 a, Vec3 b) noexcept {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}

Vec3 subtract(Vec3 a, Vec3 b) noexcept {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

Vec3 multiply(Vec3 a, float scalar) noexcept {
    return {a[0] * scalar, a[1] * scalar, a[2] * scalar};
}

float dot(Vec3 a, Vec3 b) noexcept {
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {(a[1] * b[2]) - (a[2] * b[1]),
            (a[2] * b[0]) - (a[0] * b[2]),
            (a[0] * b[1]) - (a[1] * b[0])};
}

float length_squared(Vec3 value) noexcept {
    return dot(value, value);
}

Vec3 normalize(Vec3 value) noexcept {
    const float len_sq = length_squared(value);
    if (len_sq <= kEpsilon * kEpsilon) {
        return {0.0F, 1.0F, 0.0F};
    }

    const float inv_len = 1.0F / std::sqrt(len_sq);
    return multiply(value, inv_len);
}

Vec3 triangle_normal(const stellar::assets::CollisionTriangle& triangle) noexcept {
    if (length_squared(triangle.normal) > kEpsilon * kEpsilon) {
        return normalize(triangle.normal);
    }

    return normalize(cross(subtract(triangle.b, triangle.a), subtract(triangle.c, triangle.a)));
}

bool point_in_triangle(Vec3 point, const stellar::assets::CollisionTriangle& triangle) noexcept {
    const Vec3 normal = triangle_normal(triangle);
    const Vec3 edge_ab = subtract(triangle.b, triangle.a);
    const Vec3 edge_bc = subtract(triangle.c, triangle.b);
    const Vec3 edge_ca = subtract(triangle.a, triangle.c);

    const Vec3 ap = subtract(point, triangle.a);
    const Vec3 bp = subtract(point, triangle.b);
    const Vec3 cp = subtract(point, triangle.c);

    const float side_ab = dot(normal, cross(edge_ab, ap));
    const float side_bc = dot(normal, cross(edge_bc, bp));
    const float side_ca = dot(normal, cross(edge_ca, cp));

    return side_ab >= -kEpsilon && side_bc >= -kEpsilon && side_ca >= -kEpsilon;
}

bool segment_triangle(Vec3 origin,
                      Vec3 delta,
                      const stellar::assets::CollisionTriangle& triangle,
                      float& out_t) noexcept {
    const Vec3 edge1 = subtract(triangle.b, triangle.a);
    const Vec3 edge2 = subtract(triangle.c, triangle.a);
    const Vec3 pvec = cross(delta, edge2);
    const float det = dot(edge1, pvec);

    if (std::abs(det) <= kEpsilon) {
        return false;
    }

    const float inv_det = 1.0F / det;
    const Vec3 tvec = subtract(origin, triangle.a);
    const float u = dot(tvec, pvec) * inv_det;
    if (u < -kEpsilon || u > 1.0F + kEpsilon) {
        return false;
    }

    const Vec3 qvec = cross(tvec, edge1);
    const float v = dot(delta, qvec) * inv_det;
    if (v < -kEpsilon || u + v > 1.0F + kEpsilon) {
        return false;
    }

    const float t = dot(edge2, qvec) * inv_det;
    if (t < -kEpsilon || t > 1.0F + kEpsilon) {
        return false;
    }

    out_t = std::clamp(t, 0.0F, 1.0F);
    return true;
}

struct SweepHit {
    bool hit = false;
    float t = 1.0F;
    Vec3 normal{};
};

SweepHit sweep_sphere_against_triangle(Vec3 position,
                                        Vec3 displacement,
                                        float radius,
                                        const stellar::assets::CollisionTriangle& triangle) noexcept {
    const Vec3 normal = triangle_normal(triangle);
    const float signed_start = dot(subtract(position, triangle.a), normal);
    const float denom = dot(displacement, normal);
    Vec3 contact_normal = normal;
    float target_distance = radius;

    if (std::abs(denom) <= kEpsilon) {
        return {};
    }

    if (signed_start > radius && denom < -kEpsilon) {
        target_distance = radius;
        contact_normal = normal;
    } else if (signed_start < -radius && denom > kEpsilon) {
        target_distance = -radius;
        contact_normal = multiply(normal, -1.0F);
    } else {
        return {};
    }

    const float t = (target_distance - signed_start) / denom;
    if (t < -kEpsilon || t > 1.0F + kEpsilon) {
        return {};
    }

    const Vec3 center_at_hit = add(position, multiply(displacement, std::clamp(t, 0.0F, 1.0F)));
    const Vec3 contact_point = subtract(center_at_hit, multiply(contact_normal, radius));
    if (!point_in_triangle(contact_point, triangle)) {
        return {};
    }

    return {.hit = true, .t = std::clamp(t, 0.0F, 1.0F), .normal = contact_normal};
}

} // namespace

CollisionWorld::CollisionWorld(const stellar::assets::LevelCollisionAsset& asset) noexcept
    : asset_(&asset) {}

RaycastHit CollisionWorld::raycast(Vec3 origin, Vec3 delta) const noexcept {
    RaycastHit result{};
    if (asset_ == nullptr || length_squared(delta) <= kEpsilon * kEpsilon) {
        return result;
    }

    float nearest_t = std::numeric_limits<float>::max();
    for (std::size_t mesh_index = 0; mesh_index < asset_->meshes.size(); ++mesh_index) {
        const auto& mesh = asset_->meshes[mesh_index];
        for (std::size_t triangle_index = 0; triangle_index < mesh.triangles.size(); ++triangle_index) {
            const auto& triangle = mesh.triangles[triangle_index];
            float t = 0.0F;
            if (!segment_triangle(origin, delta, triangle, t)) {
                continue;
            }

            if (t < nearest_t) {
                nearest_t = t;
                result.hit = true;
                result.t = t;
                result.position = add(origin, multiply(delta, t));
                result.normal = triangle_normal(triangle);
                result.mesh_index = mesh_index;
                result.triangle_index = triangle_index;
            }
        }
    }

    return result;
}

GroundProbeHit CollisionWorld::probe_ground(Vec3 origin,
                                            float max_distance,
                                            float min_floor_normal_y) const noexcept {
    GroundProbeHit result{};
    if (max_distance <= 0.0F) {
        return result;
    }

    const RaycastHit hit = raycast(origin, {0.0F, -max_distance, 0.0F});
    if (!hit.hit || hit.normal[1] < min_floor_normal_y) {
        return result;
    }

    result.hit = true;
    result.distance = hit.t * max_distance;
    result.raycast = hit;
    return result;
}

MoveResult CollisionWorld::move_sphere(Vec3 position,
                                       Vec3 displacement,
                                       float radius,
                                       int max_iterations) const noexcept {
    MoveResult result{};
    result.position = position;
    result.velocity = displacement;

    if (asset_ == nullptr || asset_->meshes.empty() || length_squared(displacement) <= kEpsilon * kEpsilon) {
        result.position = add(position, displacement);
        result.velocity = {0.0F, 0.0F, 0.0F};
        result.grounded = probe_ground(result.position, std::max(radius + 0.05F, 0.05F)).hit;
        return result;
    }

    const float safe_radius = std::max(radius, 0.0F);
    Vec3 current_position = position;
    Vec3 remaining = displacement;
    const int iterations = std::clamp(max_iterations, 0, 8);

    for (int iteration = 0; iteration < iterations; ++iteration) {
        if (length_squared(remaining) <= kEpsilon * kEpsilon) {
            break;
        }

        SweepHit nearest{};
        for (const auto& mesh : asset_->meshes) {
            for (const auto& triangle : mesh.triangles) {
                const SweepHit candidate =
                    sweep_sphere_against_triangle(current_position, remaining, safe_radius, triangle);
                if (candidate.hit && candidate.t < nearest.t) {
                    nearest = candidate;
                }
            }
        }

        if (!nearest.hit) {
            current_position = add(current_position, remaining);
            remaining = {0.0F, 0.0F, 0.0F};
            break;
        }

        result.hit = true;
        const float travel_t = std::max(0.0F, nearest.t - kContactOffset);
        current_position = add(current_position, multiply(remaining, travel_t));

        const Vec3 unused = multiply(remaining, 1.0F - travel_t);
        const float into_plane = dot(unused, nearest.normal);
        remaining = subtract(unused, multiply(nearest.normal, into_plane));
        result.iterations = iteration + 1;
    }

    result.position = current_position;
    result.velocity = remaining;
    result.grounded = probe_ground(result.position, std::max(safe_radius + 0.05F, 0.05F)).hit;
    return result;
}

} // namespace stellar::physics
