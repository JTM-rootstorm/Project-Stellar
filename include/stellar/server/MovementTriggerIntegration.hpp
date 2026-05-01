#pragma once

#include <array>
#include <vector>
#include <string>

#include "stellar/server/MovementSimulation.hpp"
#include "stellar/world/RuntimeWorld.hpp"
#include "stellar/world/TriggerSystem.hpp"

namespace stellar::server {

/**
 * @brief Authoritative trigger event associated with a movement tick.
 */
struct MovementTriggerEvent {
    /** @brief Stable authored trigger name. */
    std::string trigger_name;

    /** @brief True only on the first update where authoritative movement overlaps this trigger. */
    bool entered = false;

    /** @brief True on updates after enter while authoritative movement remains overlapping. */
    bool stayed = false;

    /** @brief True only on the first update where authoritative movement leaves this trigger. */
    bool exited = false;
};

/**
 * @brief Result of one authoritative movement tick followed by trigger evaluation.
 */
struct MovementTriggerTickResult {
    /** @brief Authoritative movement result for the tick. */
    MovementTickResult movement{};

    /** @brief Trigger transitions generated from the authoritative final movement position. */
    std::vector<MovementTriggerEvent> trigger_events;
};

/**
 * @brief Stateful trigger tracker for one authoritative moving character.
 *
 * Trigger overlap uses the authoritative character capsule centered at the movement position. The
 * tracker owns overlap state only; the caller supplies already-authoritative shape configuration.
 */
class MovementTriggerTracker {
public:
    /**
     * @brief Build trigger volumes from the runtime world and reset overlap state.
     */
    void reset_from_world(const stellar::world::RuntimeWorld& world);

    /**
     * @brief Update trigger state from an authoritative character center position and capsule shape.
     */
    [[nodiscard]] std::vector<MovementTriggerEvent> update(
        std::array<float, 3> position,
        const stellar::physics::CharacterControllerConfig& character) noexcept;

private:
    stellar::world::TriggerSystem trigger_system_;
};

/**
 * @brief Advance authoritative movement and update trigger state from its final position.
 */
[[nodiscard]] MovementTriggerTickResult simulate_movement_tick_and_update_triggers(
    const stellar::world::RuntimeWorld& world,
    const MovementState& previous,
    const MovementCommand& command,
    const MovementSimulationConfig& config,
    MovementTriggerTracker& tracker) noexcept;

/** @brief Advance authoritative movement with optional collision state, then update triggers. */
[[nodiscard]] MovementTriggerTickResult simulate_movement_tick_and_update_triggers(
    const stellar::world::RuntimeWorld& world,
    const MovementState& previous,
    const MovementCommand& command,
    const MovementSimulationConfig& config,
    const stellar::world::RuntimeCollisionState* collision_state,
    MovementTriggerTracker& tracker) noexcept;

} // namespace stellar::server
