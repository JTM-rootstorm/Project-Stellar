#include "stellar/server/MovementTriggerIntegration.hpp"

#include <utility>

namespace stellar::server {
namespace {

[[nodiscard]] MovementTriggerEvent to_movement_trigger_event(
    stellar::world::TriggerOverlap overlap) {
    return {.trigger_name = std::move(overlap.name),
            .entered = overlap.entered,
            .stayed = overlap.stayed,
            .exited = overlap.exited};
}

} // namespace

void MovementTriggerTracker::reset_from_world(const stellar::world::RuntimeWorld& world) {
    const auto triggers = stellar::world::build_trigger_volumes(world);
    trigger_system_.set_triggers(triggers);
}

std::vector<MovementTriggerEvent> MovementTriggerTracker::update(std::array<float, 3> position,
                                                                 float radius) noexcept {
    const auto overlaps = trigger_system_.update_sphere(position, radius);
    std::vector<MovementTriggerEvent> events;
    events.reserve(overlaps.size());
    for (auto overlap : overlaps) {
        events.push_back(to_movement_trigger_event(std::move(overlap)));
    }
    return events;
}

MovementTriggerTickResult simulate_movement_tick_and_update_triggers(
    const stellar::world::RuntimeWorld& world,
    const MovementState& previous,
    const MovementCommand& command,
    const MovementSimulationConfig& config,
    MovementTriggerTracker& tracker) noexcept {
    MovementTriggerTickResult result;
    result.movement = simulate_movement_tick(world, previous, command, config);
    result.trigger_events = tracker.update(result.movement.state.position, config.character.radius);
    return result;
}

} // namespace stellar::server
