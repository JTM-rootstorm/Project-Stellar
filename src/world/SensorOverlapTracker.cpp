#include "stellar/world/SensorOverlapTracker.hpp"

#include <algorithm>
#include <utility>

namespace stellar::world {
namespace {

bool is_transition_event(const SensorOverlapTransition& transition) noexcept {
    return transition.entered || transition.stayed || transition.exited;
}

} // namespace

void SensorOverlapTracker::reset(std::span<const SensorOverlapSample> samples) {
    entries_.clear();
    entries_.reserve(samples.size());
    for (const SensorOverlapSample& sample : samples) {
        entries_.push_back({.id = sample.id,
                            .name = sample.name,
                            .overlapping = sample.currently_overlapping});
    }
}

std::vector<SensorOverlapTransition> SensorOverlapTracker::update(
    std::span<const SensorOverlapSample> samples) {
    std::vector<SensorOverlapTransition> transitions;
    std::vector<Entry> next_entries;
    transitions.reserve(samples.size());
    next_entries.reserve(samples.size());

    for (const SensorOverlapSample& sample : samples) {
        bool previous = false;
        for (const Entry& entry : entries_) {
            if (entry.id == sample.id) {
                previous = entry.overlapping;
                break;
            }
        }

        SensorOverlapTransition transition;
        transition.id = sample.id;
        transition.name = sample.name;
        transition.entered = sample.currently_overlapping && !previous;
        transition.stayed = sample.currently_overlapping && previous;
        transition.exited = !sample.currently_overlapping && previous;
        if (is_transition_event(transition)) {
            transitions.push_back(std::move(transition));
        }

        next_entries.push_back({.id = sample.id,
                                .name = sample.name,
                                .overlapping = sample.currently_overlapping});
    }

    entries_ = std::move(next_entries);
    return transitions;
}

std::vector<SensorOverlapTransition> SensorOverlapTracker::remove_or_disable(
    std::uint32_t id, std::string_view fallback_name) noexcept {
    for (Entry& entry : entries_) {
        if (entry.id != id) {
            continue;
        }

        std::vector<SensorOverlapTransition> transitions;
        if (entry.overlapping) {
            transitions.push_back({.id = id,
                                   .name = entry.name,
                                   .entered = false,
                                   .stayed = false,
                                   .exited = true});
        }
        entry.overlapping = false;
        if (entry.name.empty()) {
            entry.name = std::string{fallback_name};
        }
        return transitions;
    }

    return {};
}

bool SensorOverlapTracker::is_overlapping(std::uint32_t id) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.id == id) {
            return entry.overlapping;
        }
    }
    return false;
}

std::string SensorOverlapTracker::name_or(std::uint32_t id, std::string_view fallback_name) const {
    for (const Entry& entry : entries_) {
        if (entry.id == id) {
            return entry.name;
        }
    }
    return std::string{fallback_name};
}

} // namespace stellar::world
