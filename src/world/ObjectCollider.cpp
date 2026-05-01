#include "stellar/world/ObjectCollider.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace stellar::world {
namespace {

bool is_finite(float value) noexcept {
    return std::isfinite(value);
}

bool is_finite(std::array<float, 3> value) noexcept {
    return is_finite(value[0]) && is_finite(value[1]) && is_finite(value[2]);
}

float sanitized_radius(float radius) noexcept {
    if (!std::isfinite(radius) || radius < 0.0F) {
        return 0.0F;
    }
    return radius;
}

float sanitized_half_extent(float value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0F;
    }
    return std::abs(value);
}

float sanitized_height(float height, float radius) noexcept {
    if (!std::isfinite(height) || height < 0.0F) {
        return 2.0F * radius;
    }
    return std::max(height, 2.0F * radius);
}

float length_squared(std::array<float, 3> value) noexcept {
    return value[0] * value[0] + value[1] * value[1] + value[2] * value[2];
}

std::array<float, 3> add(std::array<float, 3> lhs, std::array<float, 3> rhs) noexcept {
    return {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
}

std::array<float, 3> sub(std::array<float, 3> lhs, std::array<float, 3> rhs) noexcept {
    return {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2]};
}

std::array<float, 3> mul(std::array<float, 3> value, float scale) noexcept {
    return {value[0] * scale, value[1] * scale, value[2] * scale};
}

float dot(std::array<float, 3> lhs, std::array<float, 3> rhs) noexcept {
    return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
}

std::array<float, 3> normalized_or_world_y(std::array<float, 3> value) noexcept {
    if (!is_finite(value)) {
        return {0.0F, 1.0F, 0.0F};
    }
    const float len_sq = length_squared(value);
    if (len_sq <= 0.0F) {
        return {0.0F, 1.0F, 0.0F};
    }
    return mul(value, 1.0F / std::sqrt(len_sq));
}

struct Segment {
    std::array<float, 3> start{};
    std::array<float, 3> end{};
    float radius = 0.0F;
    bool valid = false;
};

Segment capsule_segment(const TriggerCapsule& capsule) noexcept {
    if (!is_finite(capsule.center)) {
        return {};
    }
    const float radius = sanitized_radius(capsule.radius);
    const float height = sanitized_height(capsule.height, radius);
    const auto up = normalized_or_world_y(capsule.up);
    const float half_segment_length = std::max(0.0F, (height - 2.0F * radius) * 0.5F);
    const auto start = sub(capsule.center, mul(up, half_segment_length));
    const auto end = add(capsule.center, mul(up, half_segment_length));
    return {.start = start, .end = end, .radius = radius, .valid = is_finite(start) && is_finite(end)};
}

Segment vertical_shape_capsule_segment(const ObjectColliderShape& shape) noexcept {
    if (!is_finite(shape.center)) {
        return {};
    }
    const float radius = sanitized_radius(shape.radius);
    const float height = sanitized_height(shape.height, radius);
    const float half_segment_length = std::max(0.0F, (height - 2.0F * radius) * 0.5F);
    const std::array<float, 3> offset{0.0F, half_segment_length, 0.0F};
    const auto start = sub(shape.center, offset);
    const auto end = add(shape.center, offset);
    return {.start = start, .end = end, .radius = radius, .valid = is_finite(start) && is_finite(end)};
}

float point_segment_distance_squared(std::array<float, 3> point, const Segment& segment) noexcept {
    const auto ab = sub(segment.end, segment.start);
    const float ab_len_sq = length_squared(ab);
    if (ab_len_sq <= 0.0F) {
        return length_squared(sub(point, segment.start));
    }
    const float t = std::clamp(dot(sub(point, segment.start), ab) / ab_len_sq, 0.0F, 1.0F);
    return length_squared(sub(point, add(segment.start, mul(ab, t))));
}

float segment_segment_distance_squared(const Segment& lhs, const Segment& rhs) noexcept {
    const auto d1 = sub(lhs.end, lhs.start);
    const auto d2 = sub(rhs.end, rhs.start);
    const auto r = sub(lhs.start, rhs.start);
    const float a = dot(d1, d1);
    const float e = dot(d2, d2);
    const float f = dot(d2, r);

    float s = 0.0F;
    float t = 0.0F;
    if (a <= 0.0F && e <= 0.0F) {
        return length_squared(sub(lhs.start, rhs.start));
    }
    if (a <= 0.0F) {
        t = std::clamp(f / e, 0.0F, 1.0F);
    } else {
        const float c = dot(d1, r);
        if (e <= 0.0F) {
            s = std::clamp(-c / a, 0.0F, 1.0F);
        } else {
            const float b = dot(d1, d2);
            const float denom = a * e - b * b;
            if (denom != 0.0F) {
                s = std::clamp((b * f - c * e) / denom, 0.0F, 1.0F);
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
    return length_squared(sub(add(lhs.start, mul(d1, s)), add(rhs.start, mul(d2, t))));
}

float point_aabb_distance_squared(std::array<float, 3> point,
                                  std::array<float, 3> min_values,
                                  std::array<float, 3> max_values) noexcept {
    float distance_squared = 0.0F;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        float delta = 0.0F;
        if (point[axis] < min_values[axis]) {
            delta = min_values[axis] - point[axis];
        } else if (point[axis] > max_values[axis]) {
            delta = point[axis] - max_values[axis];
        }
        distance_squared += delta * delta;
    }
    return distance_squared;
}

bool add_candidate(float value, std::array<float, 8>& candidates, std::size_t& count) noexcept {
    if (!std::isfinite(value) || value < 0.0F || value > 1.0F) {
        return false;
    }
    candidates[count++] = value;
    return true;
}

bool current_overlap(const TriggerCapsule& player_capsule, const ObjectCollider& collider) noexcept {
    if (!collider.enabled) {
        return false;
    }
    switch (collider.shape.type) {
    case ObjectColliderShapeType::kSphere:
        return capsule_overlaps_object_sphere(player_capsule, collider.shape);
    case ObjectColliderShapeType::kAabb:
        return capsule_overlaps_object_aabb(player_capsule, collider.shape);
    case ObjectColliderShapeType::kCapsule:
        return capsule_overlaps_object_capsule(player_capsule, collider.shape);
    }
    return false;
}

} // namespace

bool capsule_overlaps_object_sphere(const TriggerCapsule& capsule,
                                    const ObjectColliderShape& sphere) noexcept {
    const auto segment = capsule_segment(capsule);
    if (!segment.valid || !is_finite(sphere.center)) {
        return false;
    }
    const float radius_sum = segment.radius + sanitized_radius(sphere.radius);
    return point_segment_distance_squared(sphere.center, segment) <= radius_sum * radius_sum;
}

bool capsule_overlaps_object_aabb(const TriggerCapsule& capsule,
                                  const ObjectColliderShape& aabb) noexcept {
    const auto segment = capsule_segment(capsule);
    if (!segment.valid || !is_finite(aabb.center)) {
        return false;
    }

    std::array<float, 3> min_values{};
    std::array<float, 3> max_values{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const float extent = sanitized_half_extent(aabb.half_extents[axis]);
        min_values[axis] = aabb.center[axis] - extent;
        max_values[axis] = aabb.center[axis] + extent;
    }
    if (!is_finite(min_values) || !is_finite(max_values)) {
        return false;
    }

    const auto direction = sub(segment.end, segment.start);
    std::array<float, 8> candidates{};
    std::size_t count = 0;
    add_candidate(0.0F, candidates, count);
    add_candidate(1.0F, candidates, count);
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (direction[axis] != 0.0F) {
            add_candidate((min_values[axis] - segment.start[axis]) / direction[axis], candidates,
                          count);
            add_candidate((max_values[axis] - segment.start[axis]) / direction[axis], candidates,
                          count);
        }
    }

    std::sort(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(count));
    float best_distance_squared = std::numeric_limits<float>::infinity();
    auto evaluate = [&](float t) noexcept {
        best_distance_squared = std::min(
            best_distance_squared,
            point_aabb_distance_squared(add(segment.start, mul(direction, t)), min_values,
                                        max_values));
    };

    for (std::size_t i = 0; i < count; ++i) {
        evaluate(candidates[i]);
    }
    for (std::size_t i = 1; i < count; ++i) {
        const float lo = candidates[i - 1];
        const float hi = candidates[i];
        if (hi <= lo) {
            continue;
        }
        const float mid = (lo + hi) * 0.5F;
        const auto mid_point = add(segment.start, mul(direction, mid));
        float numerator = 0.0F;
        float denominator = 0.0F;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            float bound = 0.0F;
            bool active = false;
            if (mid_point[axis] < min_values[axis]) {
                bound = min_values[axis];
                active = true;
            } else if (mid_point[axis] > max_values[axis]) {
                bound = max_values[axis];
                active = true;
            }
            if (active) {
                numerator += direction[axis] * (segment.start[axis] - bound);
                denominator += direction[axis] * direction[axis];
            }
        }
        if (denominator > 0.0F) {
            evaluate(std::clamp(-numerator / denominator, lo, hi));
        }
    }

    return best_distance_squared <= segment.radius * segment.radius;
}

bool capsule_overlaps_object_capsule(const TriggerCapsule& capsule,
                                     const ObjectColliderShape& other) noexcept {
    const auto lhs = capsule_segment(capsule);
    const auto rhs = vertical_shape_capsule_segment(other);
    if (!lhs.valid || !rhs.valid) {
        return false;
    }
    const float radius_sum = lhs.radius + rhs.radius;
    return segment_segment_distance_squared(lhs, rhs) <= radius_sum * radius_sum;
}

void ObjectColliderSystem::set_colliders(std::span<const ObjectCollider> colliders) {
    colliders_.assign(colliders.begin(), colliders.end());
    previous_overlaps_.assign(colliders_.size(), false);
}

std::vector<ObjectColliderOverlapEvent> ObjectColliderSystem::update_player_capsule(
    const TriggerCapsule& player_capsule) noexcept {
    if (previous_overlaps_.size() != colliders_.size()) {
        previous_overlaps_.assign(colliders_.size(), false);
    }

    std::vector<ObjectColliderOverlapEvent> events;
    for (std::size_t i = 0; i < colliders_.size(); ++i) {
        const bool current = current_overlap(player_capsule, colliders_[i]);
        const bool previous = previous_overlaps_[i];

        ObjectColliderOverlapEvent event;
        event.collider_id = colliders_[i].id;
        event.name = colliders_[i].name;
        event.entered = current && !previous;
        event.stayed = current && previous;
        event.exited = !current && previous;
        if (event.entered || event.stayed || event.exited) {
            events.push_back(std::move(event));
        }
        previous_overlaps_[i] = current;
    }
    return events;
}

const std::vector<ObjectCollider>& ObjectColliderSystem::colliders() const noexcept {
    return colliders_;
}

} // namespace stellar::world
