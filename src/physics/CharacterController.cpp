#include "stellar/physics/CharacterController.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace stellar::physics {
namespace {

using Vec3 = std::array<float, 3>;

constexpr float kEpsilon = 1.0e-5F;
constexpr float kSweepContactOffset = 1.0e-3F;
constexpr int kSweepSamples = 32;
constexpr int kRecoveryIterations = 5;

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

float length(Vec3 value) noexcept {
    return std::sqrt(length_squared(value));
}

Vec3 normalize_or(Vec3 value, Vec3 fallback) noexcept {
    const float len_sq = length_squared(value);
    if (len_sq <= kEpsilon * kEpsilon) {
        return fallback;
    }

    return multiply(value, 1.0F / std::sqrt(len_sq));
}

Vec3 reject_from_normal(Vec3 value, Vec3 normal) noexcept {
    const float into_normal = dot(value, normal);
    if (into_normal >= 0.0F) {
        return value;
    }
    return subtract(value, multiply(normal, into_normal));
}

Vec3 triangle_normal(const stellar::assets::CollisionTriangle& triangle) noexcept {
    if (length_squared(triangle.normal) > kEpsilon * kEpsilon) {
        return normalize_or(triangle.normal, {0.0F, 1.0F, 0.0F});
    }

    return normalize_or(cross(subtract(triangle.b, triangle.a), subtract(triangle.c, triangle.a)),
                        {0.0F, 1.0F, 0.0F});
}

Vec3 closest_point_on_segment(Vec3 point, Vec3 a, Vec3 b) noexcept {
    const Vec3 ab = subtract(b, a);
    const float denom = length_squared(ab);
    if (denom <= kEpsilon * kEpsilon) {
        return a;
    }

    const float t = std::clamp(dot(subtract(point, a), ab) / denom, 0.0F, 1.0F);
    return add(a, multiply(ab, t));
}

Vec3 closest_point_on_triangle(Vec3 point,
                               const stellar::assets::CollisionTriangle& triangle) noexcept {
    const Vec3 ab = subtract(triangle.b, triangle.a);
    const Vec3 ac = subtract(triangle.c, triangle.a);
    const Vec3 ap = subtract(point, triangle.a);
    const float d1 = dot(ab, ap);
    const float d2 = dot(ac, ap);
    if (d1 <= 0.0F && d2 <= 0.0F) {
        return triangle.a;
    }

    const Vec3 bp = subtract(point, triangle.b);
    const float d3 = dot(ab, bp);
    const float d4 = dot(ac, bp);
    if (d3 >= 0.0F && d4 <= d3) {
        return triangle.b;
    }

    const float vc = (d1 * d4) - (d3 * d2);
    if (vc <= 0.0F && d1 >= 0.0F && d3 <= 0.0F) {
        const float v = d1 / (d1 - d3);
        return add(triangle.a, multiply(ab, v));
    }

    const Vec3 cp = subtract(point, triangle.c);
    const float d5 = dot(ab, cp);
    const float d6 = dot(ac, cp);
    if (d6 >= 0.0F && d5 <= d6) {
        return triangle.c;
    }

    const float vb = (d5 * d2) - (d1 * d6);
    if (vb <= 0.0F && d2 >= 0.0F && d6 <= 0.0F) {
        const float w = d2 / (d2 - d6);
        return add(triangle.a, multiply(ac, w));
    }

    const float va = (d3 * d6) - (d5 * d4);
    if (va <= 0.0F && (d4 - d3) >= 0.0F && (d5 - d6) >= 0.0F) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return add(triangle.b, multiply(subtract(triangle.c, triangle.b), w));
    }

    const float denom = 1.0F / (va + vb + vc);
    const float v = vb * denom;
    const float w = vc * denom;
    return add(triangle.a, add(multiply(ab, v), multiply(ac, w)));
}

struct Contact {
    bool hit = false;
    float t = 1.0F;
    Vec3 normal{0.0F, 1.0F, 0.0F};
};

bool sphere_overlaps_triangle(Vec3 center,
                              float radius,
                              const stellar::assets::CollisionTriangle& triangle,
                              Vec3 fallback_normal,
                              Contact& contact) noexcept {
    const Vec3 closest = closest_point_on_triangle(center, triangle);
    const Vec3 from_surface = subtract(center, closest);
    const float dist_sq = length_squared(from_surface);
    const float radius_sq = radius * radius;
    if (dist_sq > radius_sq) {
        return false;
    }

    Vec3 normal = normalize_or(from_surface, triangle_normal(triangle));
    if (dot(normal, fallback_normal) < 0.0F) {
        normal = multiply(normal, -1.0F);
    }

    contact.hit = true;
    contact.normal = normal;
    return true;
}

Contact sweep_sphere_triangle(Vec3 start,
                              Vec3 displacement,
                              float radius,
                              const stellar::assets::CollisionTriangle& triangle) noexcept {
    Contact result{};
    if (length_squared(displacement) <= kEpsilon * kEpsilon) {
        return result;
    }

    Vec3 previous = start;
    bool previous_overlap = false;
    Contact overlap{};
    sphere_overlaps_triangle(previous, radius, triangle, multiply(displacement, -1.0F), overlap);
    previous_overlap = overlap.hit;

    for (int sample = 1; sample <= kSweepSamples; ++sample) {
        const float t = static_cast<float>(sample) / static_cast<float>(kSweepSamples);
        const Vec3 current = add(start, multiply(displacement, t));
        Contact sample_contact{};
        const bool overlapping = sphere_overlaps_triangle(current, radius, triangle,
                                                          multiply(displacement, -1.0F),
                                                          sample_contact);
        if (!previous_overlap && overlapping) {
            float lo = static_cast<float>(sample - 1) / static_cast<float>(kSweepSamples);
            float hi = t;
            for (int refine = 0; refine < 8; ++refine) {
                const float mid = (lo + hi) * 0.5F;
                Contact mid_contact{};
                if (sphere_overlaps_triangle(add(start, multiply(displacement, mid)), radius,
                                             triangle, multiply(displacement, -1.0F),
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

        previous = current;
        previous_overlap = overlapping;
    }

    (void)previous;
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
                                Vec3 up) noexcept {
    RecoveryResult result{.position = position};
    const auto& asset = world.asset();

    for (int iteration = 0; iteration < kRecoveryIterations; ++iteration) {
        float deepest = 0.0F;
        Vec3 push_normal = up;
        bool found = false;

        for (const auto& mesh : asset.meshes) {
            for (const auto& triangle : mesh.triangles) {
                const Vec3 closest = closest_point_on_triangle(result.position, triangle);
                const Vec3 delta = subtract(result.position, closest);
                const float dist = length(delta);
                const float penetration = radius - dist;
                if (penetration <= 0.0F) {
                    continue;
                }

                Vec3 normal = normalize_or(delta, triangle_normal(triangle));
                if (length_squared(delta) <= kEpsilon * kEpsilon && dot(normal, up) < 0.0F) {
                    normal = multiply(normal, -1.0F);
                }

                if (penetration > deepest) {
                    deepest = penetration;
                    push_normal = normal;
                    found = true;
                }
            }
        }

        if (!found) {
            break;
        }

        result.overlapped = true;
        result.position = add(result.position, multiply(push_normal, deepest + kSweepContactOffset));
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
                       int max_iterations) noexcept {
    SlideResult result{.position = start, .remaining = displacement};
    const auto& asset = world.asset();
    const int iterations = std::clamp(max_iterations, 0, 8);

    for (int iteration = 0; iteration < iterations; ++iteration) {
        if (length_squared(result.remaining) <= kEpsilon * kEpsilon) {
            result.remaining = {0.0F, 0.0F, 0.0F};
            break;
        }

        Contact nearest{};
        for (const auto& mesh : asset.meshes) {
            for (const auto& triangle : mesh.triangles) {
                const Contact candidate = sweep_sphere_triangle(result.position, result.remaining,
                                                                radius, triangle);
                if (candidate.hit && candidate.t < nearest.t) {
                    nearest = candidate;
                }
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
        result.position = add(result.position, multiply(result.remaining, travel_t));
        const Vec3 leftover = multiply(result.remaining, 1.0F - travel_t);
        result.remaining = reject_from_normal(leftover, nearest.normal);
    }

    return result;
}

bool snap_to_ground(const CollisionWorld& world,
                    Vec3& position,
                    Vec3 up,
                    float radius,
                    float snap_distance,
                    float min_ground_dot,
                    Vec3& ground_normal) noexcept {
    const float cast_distance = std::max(0.0F, radius + snap_distance + kSweepContactOffset);
    if (cast_distance <= 0.0F) {
        return false;
    }

    const RaycastHit hit = world.raycast(position, multiply(up, -cast_distance));
    if (!hit.hit || !is_walkable(hit.normal, up, min_ground_dot)) {
        return false;
    }

    const float distance = hit.t * cast_distance;
    if (distance > radius + snap_distance + kSweepContactOffset) {
        return false;
    }

    position = add(hit.position, multiply(up, radius + kSweepContactOffset));
    ground_normal = normalize_or(hit.normal, up);
    return true;
}

float progress_along(Vec3 start, Vec3 end, Vec3 direction) noexcept {
    const float dir_len_sq = length_squared(direction);
    if (dir_len_sq <= kEpsilon * kEpsilon) {
        return 0.0F;
    }
    return dot(subtract(end, start), direction) / std::sqrt(dir_len_sq);
}

} // namespace

CharacterController::CharacterController(const CollisionWorld& world) noexcept : world_(&world) {}

CharacterMoveResult CharacterController::move(const CharacterMoveInput& input,
                                              const CharacterControllerConfig& config) const noexcept {
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

    const float radius = std::max(0.0F, config.radius) + std::max(0.0F, config.skin_width);
    const float min_ground_dot = min_walkable_dot(config.max_slope_degrees);

    const RecoveryResult recovered = recover_overlaps(*world_, input.position, radius, up);
    Vec3 start = recovered.position;
    result.started_overlapping = recovered.overlapped;
    result.hit = recovered.overlapped;

    SlideResult normal_move = slide_move(*world_, start, input.displacement, radius,
                                         config.max_slide_iterations);

    Vec3 normal_ground = up;
    const bool normal_grounded = snap_to_ground(*world_, normal_move.position, up, radius,
                                                config.ground_snap_distance,
                                                min_ground_dot, normal_ground);

    SlideResult chosen = normal_move;
    Vec3 chosen_ground = normal_ground;
    bool chosen_grounded = normal_grounded;
    bool stepped = false;

    const Vec3 vertical = multiply(up, dot(input.displacement, up));
    const Vec3 horizontal = subtract(input.displacement, vertical);
    if (normal_move.hit && config.step_height > 0.0F && length_squared(horizontal) > kEpsilon) {
        const Vec3 raised_start = add(start, multiply(up, config.step_height));
        SlideResult step_move = slide_move(*world_, raised_start, horizontal, radius,
                                           config.max_slide_iterations);
        Vec3 step_ground = up;
        const bool step_grounded = snap_to_ground(*world_, step_move.position, up, radius,
                                                  config.step_height + config.ground_snap_distance,
                                                  min_ground_dot, step_ground);
        const float normal_progress = progress_along(start, normal_move.position, horizontal);
        const float step_progress = progress_along(start, step_move.position, horizontal);
        const float lifted = dot(subtract(step_move.position, start), up);
        const float max_lift = config.step_height + kSweepContactOffset;
        if (step_grounded && step_progress > normal_progress + 0.01F && lifted <= max_lift) {
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

    return result;
}

} // namespace stellar::physics
