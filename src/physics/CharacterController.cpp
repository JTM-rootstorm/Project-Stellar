#include "stellar/physics/CharacterController.hpp"

#include "stellar/math/Geometry3.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace stellar::physics {
namespace {

using stellar::math::add;
using stellar::math::dot;
using stellar::math::length_squared;
using stellar::math::mul;
using stellar::math::normalize_or;
using stellar::math::sub;
using stellar::math::Vec3;

constexpr float kEpsilon = 1.0e-5F;
constexpr float kSweepContactOffset = 1.0e-3F;
constexpr int kSweepSamples = 32;
constexpr int kRecoveryIterations = 5;

Vec3 min_vec(Vec3 a, Vec3 b) noexcept {
    return {std::min(a[0], b[0]), std::min(a[1], b[1]), std::min(a[2], b[2])};
}

Vec3 max_vec(Vec3 a, Vec3 b) noexcept {
    return {std::max(a[0], b[0]), std::max(a[1], b[1]), std::max(a[2], b[2])};
}

CollisionQueryAabb expand_bounds(Vec3 min_value, Vec3 max_value, float amount) noexcept {
    const float safe_amount = std::max(amount, 0.0F);
    const Vec3 margin{safe_amount, safe_amount, safe_amount};
    return {.min = sub(min_value, margin), .max = add(max_value, margin)};
}

float effective_capsule_half_segment(float radius, float height) noexcept {
    const float safe_radius = std::max(radius, 0.0F);
    const float effective_height = std::max(std::max(height, 0.0F), safe_radius * 2.0F);
    return std::max(0.0F, effective_height * 0.5F - safe_radius);
}

struct CapsuleEndpoints {
    Vec3 lower{};
    Vec3 upper{};
};

CapsuleEndpoints capsule_endpoints(Vec3 center, Vec3 up, float radius, float height) noexcept {
    const float half_segment = effective_capsule_half_segment(radius, height);
    const Vec3 offset = mul(up, half_segment);
    return {.lower = sub(center, offset), .upper = add(center, offset)};
}

CollisionQueryAabb capsule_bounds(Vec3 center, Vec3 up, float radius, float height) noexcept {
    const CapsuleEndpoints endpoints = capsule_endpoints(center, up, radius, height);
    return expand_bounds(min_vec(endpoints.lower, endpoints.upper),
                         max_vec(endpoints.lower, endpoints.upper), radius);
}

CollisionQueryAabb swept_capsule_bounds(Vec3 start,
                                        Vec3 displacement,
                                        Vec3 up,
                                        float radius,
                                        float height) noexcept {
    const CapsuleEndpoints a = capsule_endpoints(start, up, radius, height);
    const CapsuleEndpoints b = capsule_endpoints(add(start, displacement), up, radius, height);
    return expand_bounds(min_vec(min_vec(a.lower, a.upper), min_vec(b.lower, b.upper)),
                         max_vec(max_vec(a.lower, a.upper), max_vec(b.lower, b.upper)), radius);
}

Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {(a[1] * b[2]) - (a[2] * b[1]),
            (a[2] * b[0]) - (a[0] * b[2]),
            (a[0] * b[1]) - (a[1] * b[0])};
}

float length(Vec3 value) noexcept {
    return std::sqrt(length_squared(value));
}

float finite_or(float value, float fallback) noexcept {
    return std::isfinite(value) ? value : fallback;
}

Vec3 reject_from_normal(Vec3 value, Vec3 normal) noexcept {
    const float into_normal = dot(value, normal);
    if (into_normal >= 0.0F) {
        return value;
    }
    return sub(value, mul(normal, into_normal));
}

Vec3 triangle_normal(const stellar::assets::CollisionTriangle& triangle) noexcept {
    if (length_squared(triangle.normal) > kEpsilon * kEpsilon) {
        return normalize_or(triangle.normal, {0.0F, 1.0F, 0.0F});
    }

    return normalize_or(cross(sub(triangle.b, triangle.a), sub(triangle.c, triangle.a)),
                        {0.0F, 1.0F, 0.0F});
}

Vec3 closest_point_on_segment(Vec3 point, Vec3 a, Vec3 b) noexcept {
    const Vec3 ab = sub(b, a);
    const float denom = length_squared(ab);
    if (denom <= kEpsilon * kEpsilon) {
        return a;
    }

    const float t = std::clamp(dot(sub(point, a), ab) / denom, 0.0F, 1.0F);
    return add(a, mul(ab, t));
}

Vec3 closest_point_on_triangle(Vec3 point,
                               const stellar::assets::CollisionTriangle& triangle) noexcept {
    const Vec3 ab = sub(triangle.b, triangle.a);
    const Vec3 ac = sub(triangle.c, triangle.a);
    const Vec3 ap = sub(point, triangle.a);
    const float d1 = dot(ab, ap);
    const float d2 = dot(ac, ap);
    if (d1 <= 0.0F && d2 <= 0.0F) {
        return triangle.a;
    }

    const Vec3 bp = sub(point, triangle.b);
    const float d3 = dot(ab, bp);
    const float d4 = dot(ac, bp);
    if (d3 >= 0.0F && d4 <= d3) {
        return triangle.b;
    }

    const float vc = (d1 * d4) - (d3 * d2);
    if (vc <= 0.0F && d1 >= 0.0F && d3 <= 0.0F) {
        const float v = d1 / (d1 - d3);
        return add(triangle.a, mul(ab, v));
    }

    const Vec3 cp = sub(point, triangle.c);
    const float d5 = dot(ab, cp);
    const float d6 = dot(ac, cp);
    if (d6 >= 0.0F && d5 <= d6) {
        return triangle.c;
    }

    const float vb = (d5 * d2) - (d1 * d6);
    if (vb <= 0.0F && d2 >= 0.0F && d6 <= 0.0F) {
        const float w = d2 / (d2 - d6);
        return add(triangle.a, mul(ac, w));
    }

    const float va = (d3 * d6) - (d5 * d4);
    if (va <= 0.0F && (d4 - d3) >= 0.0F && (d5 - d6) >= 0.0F) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return add(triangle.b, mul(sub(triangle.c, triangle.b), w));
    }

    const float denom = 1.0F / (va + vb + vc);
    const float v = vb * denom;
    const float w = vc * denom;
    return add(triangle.a, add(mul(ab, v), mul(ac, w)));
}

bool segment_intersects_triangle(Vec3 start,
                                 Vec3 end,
                                 const stellar::assets::CollisionTriangle& triangle,
                                 Vec3& intersection) noexcept {
    const Vec3 delta = sub(end, start);
    const Vec3 edge1 = sub(triangle.b, triangle.a);
    const Vec3 edge2 = sub(triangle.c, triangle.a);
    const Vec3 pvec = cross(delta, edge2);
    const float det = dot(edge1, pvec);
    if (std::abs(det) <= kEpsilon) {
        return false;
    }

    const float inv_det = 1.0F / det;
    const Vec3 tvec = sub(start, triangle.a);
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

    intersection = add(start, mul(delta, std::clamp(t, 0.0F, 1.0F)));
    return true;
}

struct ClosestPair {
    Vec3 capsule_point{};
    Vec3 triangle_point{};
    float distance_sq = std::numeric_limits<float>::max();
};

void consider_pair(ClosestPair& best, Vec3 capsule_point, Vec3 triangle_point) noexcept {
    const float dist_sq = length_squared(sub(capsule_point, triangle_point));
    if (dist_sq < best.distance_sq) {
        best.capsule_point = capsule_point;
        best.triangle_point = triangle_point;
        best.distance_sq = dist_sq;
    }
}

void closest_segment_segment(Vec3 p1,
                             Vec3 q1,
                             Vec3 p2,
                             Vec3 q2,
                             Vec3& c1,
                             Vec3& c2) noexcept {
    const Vec3 d1 = sub(q1, p1);
    const Vec3 d2 = sub(q2, p2);
    const Vec3 r = sub(p1, p2);
    const float a = length_squared(d1);
    const float e = length_squared(d2);
    const float f = dot(d2, r);
    float s = 0.0F;
    float t = 0.0F;

    if (a <= kEpsilon * kEpsilon && e <= kEpsilon * kEpsilon) {
        c1 = p1;
        c2 = p2;
        return;
    }
    if (a <= kEpsilon * kEpsilon) {
        t = std::clamp(f / e, 0.0F, 1.0F);
    } else {
        const float c = dot(d1, r);
        if (e <= kEpsilon * kEpsilon) {
            s = std::clamp(-c / a, 0.0F, 1.0F);
        } else {
            const float b = dot(d1, d2);
            const float denom = (a * e) - (b * b);
            if (std::abs(denom) > kEpsilon) {
                s = std::clamp(((b * f) - (c * e)) / denom, 0.0F, 1.0F);
            }
            t = (b * s + f) / e;
            if (t < 0.0F) {
                t = 0.0F;
                s = std::clamp(-c / a, 0.0F, 1.0F);
            } else if (t > 1.0F) {
                t = 1.0F;
                s = std::clamp((b - c) / a, 0.0F, 1.0F);
            }
        }
    }

    c1 = add(p1, mul(d1, s));
    c2 = add(p2, mul(d2, t));
}

ClosestPair closest_capsule_segment_triangle(CapsuleEndpoints capsule,
                                             const stellar::assets::CollisionTriangle& triangle) noexcept {
    ClosestPair best{};
    Vec3 intersection{};
    if (segment_intersects_triangle(capsule.lower, capsule.upper, triangle, intersection)) {
        best.capsule_point = intersection;
        best.triangle_point = intersection;
        best.distance_sq = 0.0F;
        return best;
    }

    consider_pair(best, capsule.lower, closest_point_on_triangle(capsule.lower, triangle));
    consider_pair(best, capsule.upper, closest_point_on_triangle(capsule.upper, triangle));

    const Vec3 tri_points[3] = {triangle.a, triangle.b, triangle.c};
    for (Vec3 point : tri_points) {
        consider_pair(best, closest_point_on_segment(point, capsule.lower, capsule.upper), point);
    }

    const Vec3 edges[3][2] = {{triangle.a, triangle.b}, {triangle.b, triangle.c},
                              {triangle.c, triangle.a}};
    for (const auto& edge : edges) {
        Vec3 capsule_point{};
        Vec3 triangle_point{};
        closest_segment_segment(capsule.lower, capsule.upper, edge[0], edge[1], capsule_point,
                                triangle_point);
        consider_pair(best, capsule_point, triangle_point);
    }

    return best;
}

struct Contact {
    bool hit = false;
    float t = 1.0F;
    Vec3 normal{0.0F, 1.0F, 0.0F};
};

bool capsule_overlaps_triangle(Vec3 center,
                               Vec3 up,
                               float radius,
                               float height,
                               const stellar::assets::CollisionTriangle& triangle,
                               Vec3 fallback_normal,
                               Contact& contact) noexcept {
    const CapsuleEndpoints capsule = capsule_endpoints(center, up, radius, height);
    const ClosestPair closest = closest_capsule_segment_triangle(capsule, triangle);
    const float radius_sq = radius * radius;
    if (closest.distance_sq > radius_sq) {
        return false;
    }

    Vec3 normal = normalize_or(sub(closest.capsule_point, closest.triangle_point),
                               triangle_normal(triangle));
    if (dot(normal, fallback_normal) < 0.0F) {
        normal = mul(normal, -1.0F);
    }

    contact.hit = true;
    contact.normal = normal;
    return true;
}

Contact sweep_capsule_triangle(Vec3 start,
                               Vec3 displacement,
                               Vec3 up,
                               float radius,
                               float height,
                               const stellar::assets::CollisionTriangle& triangle) noexcept {
    Contact result{};
    if (length_squared(displacement) <= kEpsilon * kEpsilon) {
        return result;
    }

    Contact overlap{};
    bool previous_overlap = capsule_overlaps_triangle(start, up, radius, height, triangle,
                                                      mul(displacement, -1.0F), overlap);

    for (int sample = 1; sample <= kSweepSamples; ++sample) {
        const float t = static_cast<float>(sample) / static_cast<float>(kSweepSamples);
        const Vec3 current = add(start, mul(displacement, t));
        Contact sample_contact{};
        const bool overlapping = capsule_overlaps_triangle(current, up, radius, height, triangle,
                                                           mul(displacement, -1.0F),
                                                           sample_contact);
        if (!previous_overlap && overlapping) {
            float lo = static_cast<float>(sample - 1) / static_cast<float>(kSweepSamples);
            float hi = t;
            for (int refine = 0; refine < 8; ++refine) {
                const float mid = (lo + hi) * 0.5F;
                Contact mid_contact{};
                if (capsule_overlaps_triangle(add(start, mul(displacement, mid)), up, radius,
                                              height, triangle, mul(displacement, -1.0F),
                                              mid_contact)) {
                    hi = mid;
                    sample_contact = mid_contact;
                } else {
                    lo = mid;
                }
            }
            result.hit = true;
            result.t = hi;
            result.normal = sample_contact.normal;
            return result;
        }

        previous_overlap = overlapping;
    }

    return result;
}

float min_walkable_dot(float max_slope_degrees) noexcept {
    constexpr float kPi = 3.14159265358979323846F;
    const float clamped_degrees = std::clamp(max_slope_degrees, 0.0F, 89.0F);
    return std::cos(clamped_degrees * kPi / 180.0F);
}

bool is_walkable(Vec3 normal, Vec3 up, float min_dot) noexcept {
    return dot(normalize_or(normal, up), up) >= min_dot;
}

struct RecoveryResult {
    Vec3 position{};
    bool overlapped = false;
};

RecoveryResult recover_overlaps(const CollisionWorld& world,
                                 Vec3 position,
                                 float radius,
                                 float height,
                                 Vec3 up,
                                 CollisionQueryFilter filter) noexcept {
    RecoveryResult result{.position = position};
    const auto& asset = world.asset();

    for (int iteration = 0; iteration < kRecoveryIterations; ++iteration) {
        float deepest = 0.0F;
        Vec3 push_normal = up;
        bool found = false;

        const auto candidates = world.query_triangles(capsule_bounds(result.position, up, radius, height),
                                                      filter);
        for (const auto& candidate : candidates) {
            const auto& triangle =
                asset.meshes[candidate.mesh_index].triangles[candidate.triangle_index];
            const ClosestPair closest = closest_capsule_segment_triangle(
                capsule_endpoints(result.position, up, radius, height), triangle);
            const float dist = std::sqrt(std::max(closest.distance_sq, 0.0F));
            const float penetration = radius - dist;
            if (penetration <= 0.0F) {
                continue;
            }

            Vec3 normal = normalize_or(sub(closest.capsule_point, closest.triangle_point),
                                       triangle_normal(triangle));
            if (closest.distance_sq <= kEpsilon * kEpsilon && dot(normal, up) < 0.0F) {
                normal = mul(normal, -1.0F);
            }

            if (penetration > deepest) {
                deepest = penetration;
                push_normal = normal;
                found = true;
            }
        }

        if (!found) {
            break;
        }

        result.overlapped = true;
        result.position = add(result.position, mul(push_normal, deepest + kSweepContactOffset));
    }

    return result;
}

struct SlideResult {
    Vec3 position{};
    Vec3 remaining{};
    Vec3 last_normal{0.0F, 1.0F, 0.0F};
    bool hit = false;
    int iterations = 0;
};

SlideResult slide_move(const CollisionWorld& world,
                       Vec3 start,
                       Vec3 displacement,
                       float radius,
                       float height,
                       Vec3 up,
                       int max_iterations,
                       CollisionQueryFilter filter) noexcept {
    SlideResult result{.position = start, .remaining = displacement};
    const auto& asset = world.asset();
    const int iterations = std::clamp(max_iterations, 0, 8);

    for (int iteration = 0; iteration < iterations; ++iteration) {
        if (length_squared(result.remaining) <= kEpsilon * kEpsilon) {
            result.remaining = {0.0F, 0.0F, 0.0F};
            break;
        }

        Contact nearest{};
        const auto candidates = world.query_triangles(
            swept_capsule_bounds(result.position, result.remaining, up, radius, height), filter);
        for (const auto& candidate : candidates) {
            const auto& triangle =
                asset.meshes[candidate.mesh_index].triangles[candidate.triangle_index];
            const Contact contact = sweep_capsule_triangle(result.position, result.remaining,
                                                           up, radius, height, triangle);
            if (contact.hit && contact.t < nearest.t) {
                nearest = contact;
            }
        }

        if (!nearest.hit) {
            result.position = add(result.position, result.remaining);
            result.remaining = {0.0F, 0.0F, 0.0F};
            break;
        }

        result.hit = true;
        result.iterations = iteration + 1;
        result.last_normal = nearest.normal;

        const float travel_t = std::max(0.0F, nearest.t - kSweepContactOffset);
        result.position = add(result.position, mul(result.remaining, travel_t));
        const Vec3 leftover = mul(result.remaining, 1.0F - travel_t);
        result.remaining = reject_from_normal(leftover, nearest.normal);
    }

    return result;
}

bool snap_to_ground(const CollisionWorld& world,
                    Vec3& position,
                    Vec3 up,
                    float radius,
                    float height,
                    float snap_distance,
                    float min_ground_dot,
                    Vec3& ground_normal,
                    CollisionQueryFilter filter) noexcept {
    const float cast_distance = std::max(0.0F, radius + snap_distance + kSweepContactOffset);
    if (cast_distance <= 0.0F) {
        return false;
    }

    const float half_segment = effective_capsule_half_segment(radius, height);
    const Vec3 lower_endpoint = sub(position, mul(up, half_segment));
    const RaycastHit hit = world.raycast(lower_endpoint, mul(up, -cast_distance), filter);
    if (!hit.hit || !is_walkable(hit.normal, up, min_ground_dot)) {
        return false;
    }

    const float distance = hit.t * cast_distance;
    if (distance > radius + snap_distance + kSweepContactOffset) {
        return false;
    }

    const Vec3 snapped_lower = add(hit.position, mul(up, radius + kSweepContactOffset));
    position = add(snapped_lower, mul(up, half_segment));
    ground_normal = normalize_or(hit.normal, up);
    return true;
}

float progress_along(Vec3 start, Vec3 end, Vec3 direction) noexcept {
    const float dir_len_sq = length_squared(direction);
    if (dir_len_sq <= kEpsilon * kEpsilon) {
        return 0.0F;
    }
    return dot(sub(end, start), direction) / std::sqrt(dir_len_sq);
}

void publish_character_query_diagnostics(const CollisionWorld& world,
                                          Vec3 start,
                                          Vec3 end,
                                          Vec3 up,
                                          float radius,
                                          float height,
                                          CollisionQueryFilter filter) {
    const Vec3 displacement = sub(end, start);
    (void)world.query_triangles(swept_capsule_bounds(start, displacement, up, radius, height), filter);
}

} // namespace

CharacterController::CharacterController(const CollisionWorld& world) noexcept : world_(&world) {}

CharacterMoveResult CharacterController::move(const CharacterMoveInput& input,
                                               const CharacterControllerConfig& config) const noexcept {
    return move(input, config, {});
}

CharacterMoveResult CharacterController::move(const CharacterMoveInput& input,
                                              const CharacterControllerConfig& config,
                                              CollisionQueryFilter filter) const noexcept {
    CharacterMoveResult result{};
    const Vec3 up = normalize_or(input.up, {0.0F, 1.0F, 0.0F});
    result.position = input.position;
    result.remaining_displacement = input.displacement;
    result.ground_normal = up;

    if (world_ == nullptr) {
        result.position = add(input.position, input.displacement);
        result.remaining_displacement = {0.0F, 0.0F, 0.0F};
        return result;
    }

    const float skin_width = std::max(0.0F, finite_or(config.skin_width, 0.0F));
    const float radius = std::max(0.0F, finite_or(config.radius, 0.0F)) + skin_width;
    const float requested_height = std::max(0.0F, finite_or(config.height, radius * 2.0F));
    const float height = std::max(requested_height + (skin_width * 2.0F), radius * 2.0F);
    const float min_ground_dot = min_walkable_dot(config.max_slope_degrees);

    const RecoveryResult recovered = recover_overlaps(*world_, input.position, radius, height, up,
                                                      filter);
    Vec3 start = recovered.position;
    result.started_overlapping = recovered.overlapped;
    result.hit = recovered.overlapped;

    SlideResult normal_move = slide_move(*world_, start, input.displacement, radius, height, up,
                                          config.max_slide_iterations, filter);

    Vec3 normal_ground = up;
    const bool normal_grounded = snap_to_ground(*world_, normal_move.position, up, radius, height,
                                                config.ground_snap_distance,
                                                min_ground_dot, normal_ground, filter);

    SlideResult chosen = normal_move;
    Vec3 chosen_ground = normal_ground;
    bool chosen_grounded = normal_grounded;
    bool stepped = false;

    const Vec3 vertical = mul(up, dot(input.displacement, up));
    const Vec3 horizontal = sub(input.displacement, vertical);
    if (normal_move.hit && config.step_height > 0.0F && length_squared(horizontal) > kEpsilon) {
        SlideResult lift_move = slide_move(*world_, start, mul(up, config.step_height), radius,
                                           height, up, 1, filter);
        const bool lift_clear = !lift_move.hit &&
                                length_squared(lift_move.remaining) <= kEpsilon * kEpsilon;
        const Vec3 raised_start = lift_move.position;
        SlideResult step_move = slide_move(*world_, raised_start, horizontal, radius, height, up,
                                           config.max_slide_iterations, filter);
        Vec3 step_ground = up;
        const bool step_grounded = snap_to_ground(*world_, step_move.position, up, radius, height,
                                                   config.step_height + config.ground_snap_distance,
                                                   min_ground_dot, step_ground, filter);
        const RecoveryResult step_recovery = recover_overlaps(*world_, step_move.position, radius,
                                                              height, up, filter);
        const float normal_progress = progress_along(start, normal_move.position, horizontal);
        const float step_progress = progress_along(start, step_move.position, horizontal);
        const float lifted = dot(sub(step_move.position, start), up);
        const float max_lift = config.step_height + kSweepContactOffset;
        if (lift_clear && !step_recovery.overlapped && step_grounded &&
            step_progress > normal_progress + 0.01F && lifted <= max_lift) {
            chosen = step_move;
            chosen_ground = step_ground;
            chosen_grounded = true;
            stepped = true;
        }
    }

    result.position = chosen.position;
    result.remaining_displacement = chosen.remaining;
    result.hit = result.hit || chosen.hit;
    result.grounded = chosen_grounded;
    result.stepped = stepped;
    result.ground_normal = chosen_ground;
    result.iterations = chosen.iterations;

    if (!std::isfinite(result.position[0]) || !std::isfinite(result.position[1]) ||
        !std::isfinite(result.position[2])) {
        result.position = input.position;
        result.remaining_displacement = {0.0F, 0.0F, 0.0F};
        result.grounded = false;
        result.stepped = false;
    }

    publish_character_query_diagnostics(*world_,
                                         input.position,
                                         add(input.position, input.displacement),
                                          up,
                                          radius + kSweepContactOffset,
                                          height,
                                          filter);

    return result;
}

} // namespace stellar::physics
