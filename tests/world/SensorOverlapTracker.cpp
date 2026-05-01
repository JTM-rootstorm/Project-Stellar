#include "stellar/world/SensorOverlapTracker.hpp"

#include <cassert>
#include <vector>

namespace {

stellar::world::SensorOverlapSample sample(std::uint32_t id,
                                           const char* name,
                                           bool overlapping) {
    return {.id = id, .name = name, .currently_overlapping = overlapping};
}

void update_emits_enter_stay_exit_in_sample_order() {
    stellar::world::SensorOverlapTracker tracker;

    std::vector samples{
        sample(2, "Beta", true),
        sample(1, "Alpha", true),
    };
    auto transitions = tracker.update(samples);
    assert(transitions.size() == 2);
    assert(transitions[0].id == 2);
    assert(transitions[0].entered);
    assert(transitions[1].id == 1);
    assert(transitions[1].entered);

    transitions = tracker.update(samples);
    assert(transitions.size() == 2);
    assert(transitions[0].id == 2);
    assert(transitions[0].stayed);
    assert(transitions[1].id == 1);
    assert(transitions[1].stayed);

    samples = {
        sample(2, "Beta", false),
        sample(1, "Alpha", true),
    };
    transitions = tracker.update(samples);
    assert(transitions.size() == 2);
    assert(transitions[0].id == 2);
    assert(transitions[0].exited);
    assert(transitions[1].id == 1);
    assert(transitions[1].stayed);
}

void reset_replaces_state_without_emitting_transitions() {
    stellar::world::SensorOverlapTracker tracker;
    const std::vector initial{sample(7, "Door", true)};
    tracker.reset(initial);

    const auto transitions = tracker.update(initial);
    assert(transitions.size() == 1);
    assert(transitions[0].id == 7);
    assert(transitions[0].stayed);
}

void missing_samples_are_forgotten_without_exit() {
    stellar::world::SensorOverlapTracker tracker;
    tracker.reset(std::vector{sample(9, "Pickup", true)});

    const auto missing = tracker.update(std::vector<stellar::world::SensorOverlapSample>{});
    assert(missing.empty());

    const auto reintroduced = tracker.update(std::vector{sample(9, "Pickup", true)});
    assert(reintroduced.size() == 1);
    assert(reintroduced[0].entered);
}

void remove_or_disable_emits_synchronous_exit_once() {
    stellar::world::SensorOverlapTracker tracker;
    tracker.reset(std::vector{sample(11, "Switch", true)});

    auto exits = tracker.remove_or_disable(11, "Fallback");
    assert(exits.size() == 1);
    assert(exits[0].id == 11);
    assert(exits[0].name == "Switch");
    assert(exits[0].exited);
    assert(!tracker.is_overlapping(11));

    exits = tracker.remove_or_disable(11, "Fallback");
    assert(exits.empty());
}

void unknown_remove_and_name_fallback_are_deterministic() {
    stellar::world::SensorOverlapTracker tracker;
    assert(tracker.remove_or_disable(99, "Missing").empty());
    assert(!tracker.is_overlapping(99));
    assert(tracker.name_or(99, "Missing") == "Missing");
}

} // namespace

int main() {
    update_emits_enter_stay_exit_in_sample_order();
    reset_replaces_state_without_emitting_transitions();
    missing_samples_are_forgotten_without_exit();
    remove_or_disable_emits_synchronous_exit_once();
    unknown_remove_and_name_fallback_are_deterministic();
    return 0;
}
