#include "stellar/world/TriggerSystem.hpp"

#include "stellar/world/RuntimeWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace stellar::world {
namespace {

float sanitized_half_extent(float value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0F;
    }
    return std::abs(value);
}

float sanitized_radius(float radius) noexcept {
    if (!std::isfinite(radius) || radius < 0.0F) {
        return 0.0F;
    }
    return radius;
}

bool is_finite(float value) noexcept {
    return std::isfinite(value);
}

bool is_finite(std::array<float, 3> value) noexcept {
    return is_finite(value[0]) && is_finite(value[1]) && is_finite(value[2]);
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

std::array<float, 3> normalized_or_world_y(std::array<float, 3> value) noexcept {
    if (!is_finite(value)) {
        return {0.0F, 1.0F, 0.0F};
    }
    const float len_sq = length_squared(value);
    if (len_sq <= 0.0F) {
        return {0.0F, 1.0F, 0.0F};
    }
    const float inv_len = 1.0F / std::sqrt(len_sq);
    return mul(value, inv_len);
}

float sanitized_height(float height, float radius) noexcept {
    if (!std::isfinite(height) || height < 0.0F) {
        return 2.0F * radius;
    }
    return std::max(height, 2.0F * radius);
}

bool trigger_bounds(const TriggerVolume& trigger,
                    std::array<float, 3>& min_values,
                    std::array<float, 3>& max_values) noexcept {
    if (!is_finite(trigger.center)) {
        return false;
    }
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const float extent = sanitized_half_extent(trigger.half_extents[axis]);
        min_values[axis] = trigger.center[axis] - extent;
        max_values[axis] = trigger.center[axis] + extent;
    }
    return is_finite(min_values) && is_finite(max_values);
}

bool sphere_overlaps_aabb_inclusive(const TriggerVolume& trigger,
                                    std::array<float, 3> center,
                                    float radius) noexcept {
    const float query_radius = sanitized_radius(radius);
    float distance_squared = 0.0F;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const float extent = sanitized_half_extent(trigger.half_extents[axis]);
        const float min_value = trigger.center[axis] - extent;
        const float max_value = trigger.center[axis] + extent;
        float delta = 0.0F;
        if (center[axis] < min_value) {
            delta = min_value - center[axis];
        } else if (center[axis] > max_value) {
            delta = center[axis] - max_value;
        }
        distance_squared += delta * delta;
    }
    return distance_squared <= query_radius * query_radius;
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

bool capsule_overlaps_aabb_inclusive(const TriggerVolume& trigger,
                                     const TriggerCapsule& capsule) noexcept {
    if (!is_finite(capsule.center)) {
        return false;
    }

    const float radius = sanitized_radius(capsule.radius);
    const float height = sanitized_height(capsule.height, radius);
    const auto up = normalized_or_world_y(capsule.up);
    const float half_segment_length = std::max(0.0F, (height - 2.0F * radius) * 0.5F);
    const auto start = sub(capsule.center, mul(up, half_segment_length));
    const auto end = add(capsule.center, mul(up, half_segment_length));
    const auto direction = sub(end, start);

    std::array<float, 3> min_values{};
    std::array<float, 3> max_values{};
    if (!trigger_bounds(trigger, min_values, max_values) || !is_finite(start) || !is_finite(end)) {
        return false;
    }

    std::array<float, 8> candidates{};
    std::size_t count = 0;
    add_candidate(0.0F, candidates, count);
    add_candidate(1.0F, candidates, count);
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (direction[axis] != 0.0F) {
            add_candidate((min_values[axis] - start[axis]) / direction[axis], candidates, count);
            add_candidate((max_values[axis] - start[axis]) / direction[axis], candidates, count);
        }
    }

    std::sort(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(count));
    float best_distance_squared = std::numeric_limits<float>::infinity();
    auto evaluate = [&](float t) noexcept {
        const auto point = add(start, mul(direction, t));
        best_distance_squared = std::min(
            best_distance_squared, point_aabb_distance_squared(point, min_values, max_values));
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
        const auto mid_point = add(start, mul(direction, mid));
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
                numerator += direction[axis] * (start[axis] - bound);
                denominator += direction[axis] * direction[axis];
            }
        }
        if (denominator > 0.0F) {
            evaluate(std::clamp(-numerator / denominator, lo, hi));
        }
    }

    return best_distance_squared <= radius * radius;
}

std::vector<TriggerOverlap> update_overlaps(
    const std::vector<TriggerVolume>& triggers,
    std::vector<bool>& previous_overlaps,
    const auto& overlaps_trigger) noexcept {
    std::vector<TriggerOverlap> overlaps;
    if (previous_overlaps.size() != triggers.size()) {
        previous_overlaps.assign(triggers.size(), false);
    }

    for (std::size_t i = 0; i < triggers.size(); ++i) {
        const bool current = overlaps_trigger(triggers[i]);
        const bool previous = previous_overlaps[i];

        TriggerOverlap event;
        event.name = triggers[i].name;
        event.entered = current && !previous;
        event.stayed = current && previous;
        event.exited = !current && previous;

        if (event.entered || event.stayed || event.exited) {
            overlaps.push_back(std::move(event));
        }
        previous_overlaps[i] = current;
    }

    return overlaps;
}

} // namespace

void TriggerSystem::set_triggers(std::span<const TriggerVolume> triggers) {
    triggers_.assign(triggers.begin(), triggers.end());
    std::stable_sort(triggers_.begin(), triggers_.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.name < rhs.name;
    });
    previous_overlaps_.assign(triggers_.size(), false);
}

std::vector<TriggerOverlap> TriggerSystem::update_sphere(std::array<float, 3> center,
                                                          float radius) noexcept {
    return update_overlaps(
        triggers_, previous_overlaps_, [center, radius](const TriggerVolume& trigger) {
            return sphere_overlaps_aabb_inclusive(trigger, center, radius);
        });
}

std::vector<TriggerOverlap> TriggerSystem::update_capsule(
    const TriggerCapsule& capsule) noexcept {
    return update_overlaps(triggers_, previous_overlaps_, [&capsule](const TriggerVolume& trigger) {
        return capsule_overlaps_aabb_inclusive(trigger, capsule);
    });
}

const std::vector<TriggerVolume>& TriggerSystem::triggers() const noexcept {
    return triggers_;
}

std::vector<TriggerVolume> build_trigger_volumes(
    const stellar::assets::WorldMetadataAsset& metadata) {
    std::vector<TriggerVolume> triggers;
    for (const auto& marker : metadata.markers) {
        if (marker.type != stellar::assets::WorldMarkerType::kTrigger) {
            continue;
        }

        TriggerVolume trigger;
        trigger.name = marker.name;
        trigger.center = marker.position;
        trigger.half_extents = {sanitized_half_extent(marker.scale[0]),
                                sanitized_half_extent(marker.scale[1]),
                                sanitized_half_extent(marker.scale[2])};
        triggers.push_back(std::move(trigger));
    }

    std::stable_sort(triggers.begin(), triggers.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.name < rhs.name;
    });
    return triggers;
}

std::vector<TriggerVolume> build_trigger_volumes(const RuntimeWorld& world) {
    return build_trigger_volumes(world.world_metadata);
}

} // namespace stellar::world
