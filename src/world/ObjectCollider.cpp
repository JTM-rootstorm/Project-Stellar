#include "stellar/world/ObjectCollider.hpp"

#include "stellar/core/WorldAxes.hpp"
#include "stellar/math/Geometry3.hpp"
#include "stellar/world/RuntimeWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>

namespace stellar::world {
namespace {

using stellar::math::Aabb3;
using stellar::math::Segment3;
using stellar::math::add;
using stellar::math::dot;
using stellar::math::is_finite;
using stellar::math::length_squared;
using stellar::math::mul;
using stellar::math::point_aabb_distance_squared;
using stellar::math::point_segment_distance_squared;
using stellar::math::sanitized_capsule_height;
using stellar::math::sanitized_half_extent;
using stellar::math::sanitized_radius;
using stellar::math::sub;

std::array<float, 3> normalized_or_world_up(std::array<float, 3> value) noexcept {
    if (!is_finite(value)) {
        return stellar::core::kWorldUp;
    }
    const float len_sq = length_squared(value);
    if (len_sq <= 0.0F) {
        return stellar::core::kWorldUp;
    }
    return mul(value, 1.0F / std::sqrt(len_sq));
}

struct Segment {
    Segment3 segment{};
    float radius = 0.0F;
    bool valid = false;
};

Segment capsule_segment(const TriggerCapsule& capsule) noexcept {
    if (!is_finite(capsule.center)) {
        return {};
    }
    const float radius = sanitized_radius(capsule.radius);
    const float height = sanitized_capsule_height(capsule.height, radius);
    const auto up = normalized_or_world_up(capsule.up);
    const float half_segment_length = std::max(0.0F, (height - 2.0F * radius) * 0.5F);
    const auto start = sub(capsule.center, mul(up, half_segment_length));
    const auto end = add(capsule.center, mul(up, half_segment_length));
    return {.segment = {.start = start, .end = end},
            .radius = radius,
            .valid = is_finite(start) && is_finite(end)};
}

Segment vertical_shape_capsule_segment(const ObjectColliderShape& shape) noexcept {
    if (!is_finite(shape.center)) {
        return {};
    }
    const float radius = sanitized_radius(shape.radius);
    const float height = sanitized_capsule_height(shape.height, radius);
    const float half_segment_length = std::max(0.0F, (height - 2.0F * radius) * 0.5F);
    const std::array<float, 3> offset{stellar::core::kWorldUp[0] * half_segment_length,
                                      stellar::core::kWorldUp[1] * half_segment_length,
                                      stellar::core::kWorldUp[2] * half_segment_length};
    const auto start = sub(shape.center, offset);
    const auto end = add(shape.center, offset);
    return {.segment = {.start = start, .end = end},
            .radius = radius,
            .valid = is_finite(start) && is_finite(end)};
}

float segment_segment_distance_squared(const Segment& lhs, const Segment& rhs) noexcept {
    const auto d1 = sub(lhs.segment.end, lhs.segment.start);
    const auto d2 = sub(rhs.segment.end, rhs.segment.start);
    const auto r = sub(lhs.segment.start, rhs.segment.start);
    const float a = dot(d1, d1);
    const float e = dot(d2, d2);
    const float f = dot(d2, r);

    float s = 0.0F;
    float t = 0.0F;
    if (a <= 0.0F && e <= 0.0F) {
        return length_squared(sub(lhs.segment.start, rhs.segment.start));
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
    return length_squared(
        sub(add(lhs.segment.start, mul(d1, s)), add(rhs.segment.start, mul(d2, t))));
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

ObjectColliderOverlapEvent make_exit_event(std::uint32_t collider_id, const std::string& name) {
    return {.collider_id = collider_id,
            .name = name,
            .entered = false,
            .stayed = false,
            .exited = true};
}

ObjectColliderOverlapEvent to_object_collider_event(
    const SensorOverlapTransition& transition) {
    return {.collider_id = transition.id,
            .name = transition.name,
            .entered = transition.entered,
            .stayed = transition.stayed,
            .exited = transition.exited};
}

std::vector<ObjectColliderOverlapEvent> to_object_collider_events(
    std::vector<SensorOverlapTransition> transitions) {
    std::vector<ObjectColliderOverlapEvent> events;
    events.reserve(transitions.size());
    for (const SensorOverlapTransition& transition : transitions) {
        events.push_back(to_object_collider_event(transition));
    }
    return events;
}

std::size_t collider_id_count(std::span<const ObjectCollider> colliders,
                              std::uint32_t collider_id) noexcept {
    std::size_t count = 0;
    for (const ObjectCollider& collider : colliders) {
        if (collider.id == collider_id) {
            ++count;
        }
    }
    return count;
}

std::size_t first_collider_index(std::span<const ObjectCollider> colliders,
                                 std::uint32_t collider_id) noexcept {
    for (std::size_t i = 0; i < colliders.size(); ++i) {
        if (colliders[i].id == collider_id) {
            return i;
        }
    }
    return colliders.size();
}

bool id_seen_before(std::span<const std::uint32_t> ids, std::uint32_t id) noexcept {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

std::vector<SensorOverlapSample> build_collider_samples(
    std::span<const ObjectCollider> colliders,
    const auto& is_currently_overlapping) {
    std::vector<SensorOverlapSample> samples;
    samples.reserve(colliders.size());
    std::vector<std::uint32_t> seen_ids;
    seen_ids.reserve(colliders.size());

    for (const ObjectCollider& collider : colliders) {
        if (id_seen_before(seen_ids, collider.id)) {
            continue;
        }
        seen_ids.push_back(collider.id);
        samples.push_back({.id = collider.id,
                           .name = collider.name,
                           .currently_overlapping = is_currently_overlapping(collider)});
    }

    return samples;
}

} // namespace

bool capsule_overlaps_object_sphere(const TriggerCapsule& capsule,
                                    const ObjectColliderShape& sphere) noexcept {
    const auto segment = capsule_segment(capsule);
    if (!segment.valid || !is_finite(sphere.center)) {
        return false;
    }
    const float radius_sum = segment.radius + sanitized_radius(sphere.radius);
    return point_segment_distance_squared(sphere.center, segment.segment) <= radius_sum * radius_sum;
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

    const auto direction = sub(segment.segment.end, segment.segment.start);
    std::array<float, 8> candidates{};
    std::size_t count = 0;
    add_candidate(0.0F, candidates, count);
    add_candidate(1.0F, candidates, count);
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (direction[axis] != 0.0F) {
            add_candidate((min_values[axis] - segment.segment.start[axis]) / direction[axis],
                          candidates, count);
            add_candidate((max_values[axis] - segment.segment.start[axis]) / direction[axis],
                          candidates, count);
        }
    }

    std::sort(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(count));
    float best_distance_squared = std::numeric_limits<float>::infinity();
    auto evaluate = [&](float t) noexcept {
        best_distance_squared = std::min(
            best_distance_squared,
            point_aabb_distance_squared(add(segment.segment.start, mul(direction, t)),
                                        Aabb3{.min = min_values, .max = max_values}));
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
        const auto mid_point = add(segment.segment.start, mul(direction, mid));
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
                numerator += direction[axis] * (segment.segment.start[axis] - bound);
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

std::vector<ObjectCollider> build_object_colliders(
    const stellar::assets::WorldMetadataAsset& metadata) {
    std::vector<ObjectCollider> colliders;
    colliders.reserve(metadata.markers.size());
    for (std::size_t marker_index = 0; marker_index < metadata.markers.size(); ++marker_index) {
        const auto& marker = metadata.markers[marker_index];
        if (marker.type != stellar::assets::WorldMarkerType::kObjectCollider) {
            continue;
        }

        ObjectCollider collider;
        collider.id = static_cast<std::uint32_t>(marker_index + 1U);
        collider.name = marker.name;
        collider.archetype = marker.archetype;
        collider.shape.type = ObjectColliderShapeType::kAabb;
        collider.shape.center = marker.position;
        collider.shape.half_extents = {std::abs(marker.scale[0]), std::abs(marker.scale[1]),
                                       std::abs(marker.scale[2])};
        colliders.push_back(std::move(collider));
    }
    return colliders;
}

std::vector<ObjectCollider> build_object_colliders(const RuntimeWorld& world) {
    return build_object_colliders(world.world_metadata);
}

void ObjectColliderSystem::set_colliders(std::span<const ObjectCollider> colliders) {
    colliders_.assign(colliders.begin(), colliders.end());
    overlap_tracker_.reset(build_collider_samples(colliders_, [](const ObjectCollider&) {
        return false;
    }));
}

std::vector<ObjectColliderOverlapEvent> ObjectColliderSystem::replace_colliders_preserving_overlaps(
    std::span<const ObjectCollider> colliders) noexcept {
    std::vector<ObjectColliderOverlapEvent> exits;
    std::vector<std::uint32_t> seen_old_ids;
    seen_old_ids.reserve(colliders_.size());
    for (const ObjectCollider& old_collider : colliders_) {
        if (id_seen_before(seen_old_ids, old_collider.id)) {
            continue;
        }
        seen_old_ids.push_back(old_collider.id);
        if (!overlap_tracker_.is_overlapping(old_collider.id)) {
            continue;
        }
        const std::size_t new_index = first_collider_index(colliders, old_collider.id);
        if (new_index == colliders.size() || !colliders[new_index].enabled) {
            exits.push_back(make_exit_event(old_collider.id,
                                            overlap_tracker_.name_or(old_collider.id,
                                                                     old_collider.name)));
        }
    }

    colliders_.assign(colliders.begin(), colliders.end());
    overlap_tracker_.reset(build_collider_samples(colliders_, [this](const ObjectCollider& collider) {
        return collider.enabled && overlap_tracker_.is_overlapping(collider.id);
    }));
    return exits;
}

ObjectColliderMutationResult ObjectColliderSystem::set_collider_enabled(
    std::uint32_t collider_id, bool enabled) noexcept {
    const std::size_t count = collider_id_count(colliders_, collider_id);
    if (count == 0) {
        return {
            .applied = false, .code = "not_found", .message = "Object collider id was not found."};
    }
    if (count > 1) {
        return {.applied = false,
                .code = "duplicate_collider_id",
                .message = "Object collider id is not unique; mutation was rejected."};
    }

    const std::size_t index = first_collider_index(colliders_, collider_id);
    ObjectCollider& collider = colliders_[index];
    ObjectColliderMutationResult result{.applied = true,
                                        .code = "applied",
                                        .message = "Object collider enabled state was applied."};
    if (collider.enabled == enabled) {
        return result;
    }

    collider.enabled = enabled;
    if (!enabled) {
        result.exit_events = to_object_collider_events(
            overlap_tracker_.remove_or_disable(collider_id, collider.name));
    }
    overlap_tracker_.reset(build_collider_samples(colliders_, [this](const ObjectCollider& sample) {
        return sample.enabled && overlap_tracker_.is_overlapping(sample.id);
    }));
    return result;
}

ObjectColliderMutationResult ObjectColliderSystem::upsert_collider(
    const ObjectCollider& collider) noexcept {
    const std::size_t count = collider_id_count(colliders_, collider.id);
    if (count > 1) {
        return {.applied = false,
                .code = "duplicate_collider_id",
                .message = "Object collider id is not unique; mutation was rejected."};
    }

    ObjectColliderMutationResult result{.applied = true,
                                        .code = "applied",
                                        .message = "Object collider was upserted."};
    if (count == 0) {
        colliders_.push_back(collider);
        overlap_tracker_.reset(build_collider_samples(colliders_, [this](const ObjectCollider& sample) {
            return sample.enabled && overlap_tracker_.is_overlapping(sample.id);
        }));
        return result;
    }

    const std::size_t index = first_collider_index(colliders_, collider.id);
    const std::string previous_name = overlap_tracker_.name_or(collider.id, colliders_[index].name);
    const bool was_overlapping = overlap_tracker_.is_overlapping(collider.id);
    colliders_[index] = collider;
    if (!collider.enabled && was_overlapping) {
        result.exit_events.push_back(make_exit_event(collider.id, previous_name));
    }
    overlap_tracker_.reset(build_collider_samples(colliders_, [this](const ObjectCollider& sample) {
        return sample.enabled && overlap_tracker_.is_overlapping(sample.id);
    }));
    return result;
}

ObjectColliderMutationResult ObjectColliderSystem::remove_collider(
    std::uint32_t collider_id) noexcept {
    const std::size_t count = collider_id_count(colliders_, collider_id);
    if (count == 0) {
        return {
            .applied = false, .code = "not_found", .message = "Object collider id was not found."};
    }
    if (count > 1) {
        return {.applied = false,
                .code = "duplicate_collider_id",
                .message = "Object collider id is not unique; mutation was rejected."};
    }

    ObjectColliderMutationResult result{.applied = true,
                                        .code = "applied",
                                        .message = "Object collider was removed."};
    const std::size_t index = first_collider_index(colliders_, collider_id);
    result.exit_events = to_object_collider_events(
        overlap_tracker_.remove_or_disable(collider_id, colliders_[index].name));
    colliders_.erase(colliders_.begin() + static_cast<std::ptrdiff_t>(index));
    overlap_tracker_.reset(build_collider_samples(colliders_, [this](const ObjectCollider& sample) {
        return sample.enabled && overlap_tracker_.is_overlapping(sample.id);
    }));
    return result;
}

std::vector<ObjectColliderOverlapEvent> ObjectColliderSystem::update_player_capsule(
    const TriggerCapsule& player_capsule) noexcept {
    const auto samples = build_collider_samples(
        colliders_, [&player_capsule](const ObjectCollider& collider) {
            return current_overlap(player_capsule, collider);
        });
    return to_object_collider_events(overlap_tracker_.update(samples));
}

const std::vector<ObjectCollider>& ObjectColliderSystem::colliders() const noexcept {
    return colliders_;
}

std::vector<ObjectColliderDiagnostic> ObjectColliderSystem::diagnostics() const {
    std::vector<ObjectColliderDiagnostic> diagnostics;
    std::vector<std::uint32_t> seen_ids;
    seen_ids.reserve(colliders_.size());

    for (std::size_t i = 0; i < colliders_.size(); ++i) {
        const ObjectCollider& collider = colliders_[i];
        if (id_seen_before(seen_ids, collider.id)) {
            diagnostics.push_back({.severity = ObjectColliderDiagnosticSeverity::kError,
                                   .code = "duplicate_collider_id",
                                   .message = "Object collider id duplicates an earlier collider.",
                                   .collider_id = collider.id,
                                   .collider_index = i});
        } else {
            seen_ids.push_back(collider.id);
        }

        if (!is_finite(collider.shape.center)) {
            diagnostics.push_back({.severity = ObjectColliderDiagnosticSeverity::kError,
                                   .code = "non_finite_center",
                                   .message = "Object collider shape center contains non-finite "
                                              "data.",
                                   .collider_id = collider.id,
                                   .collider_index = i});
        }
        if (!std::isfinite(collider.shape.radius) || collider.shape.radius < 0.0F) {
            diagnostics.push_back({.severity = ObjectColliderDiagnosticSeverity::kWarning,
                                   .code = "invalid_radius",
                                   .message = "Object collider radius will sanitize to zero.",
                                   .collider_id = collider.id,
                                   .collider_index = i});
        }
        if (collider.shape.type == ObjectColliderShapeType::kAabb &&
            !is_finite(collider.shape.half_extents)) {
            diagnostics.push_back({.severity = ObjectColliderDiagnosticSeverity::kWarning,
                                   .code = "non_finite_half_extents",
                                   .message = "Object collider AABB half extents contain "
                                              "non-finite data.",
                                   .collider_id = collider.id,
                                   .collider_index = i});
        }
        if (collider.shape.type == ObjectColliderShapeType::kCapsule &&
            (!std::isfinite(collider.shape.height) || collider.shape.height < 0.0F)) {
            diagnostics.push_back({.severity = ObjectColliderDiagnosticSeverity::kWarning,
                                   .code = "invalid_height",
                                   .message = "Object collider capsule height will sanitize.",
                                   .collider_id = collider.id,
                                   .collider_index = i});
        }
    }

    return diagnostics;
}

} // namespace stellar::world
