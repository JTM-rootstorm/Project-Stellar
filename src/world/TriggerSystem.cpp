#include "stellar/world/TriggerSystem.hpp"

#include "stellar/world/RuntimeWorld.hpp"

#include <algorithm>
#include <cmath>
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
    std::vector<TriggerOverlap> overlaps;
    if (previous_overlaps_.size() != triggers_.size()) {
        previous_overlaps_.assign(triggers_.size(), false);
    }

    for (std::size_t i = 0; i < triggers_.size(); ++i) {
        const bool current = sphere_overlaps_aabb_inclusive(triggers_[i], center, radius);
        const bool previous = previous_overlaps_[i];

        TriggerOverlap event;
        event.name = triggers_[i].name;
        event.entered = current && !previous;
        event.stayed = current && previous;
        event.exited = !current && previous;

        if (event.entered || event.stayed || event.exited) {
            overlaps.push_back(std::move(event));
        }
        previous_overlaps_[i] = current;
    }

    return overlaps;
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
