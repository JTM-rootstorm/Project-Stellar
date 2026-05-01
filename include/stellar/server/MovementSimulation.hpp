#pragma once

#include <array>

#include "stellar/physics/CharacterController.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::server {

/**
 * @brief Tunable deterministic movement settings owned by authoritative simulation.
 */
struct MovementSimulationConfig {
    /** @brief Maximum horizontal movement speed in world units per second. */
    float max_speed = 6.0F;

    /** @brief Horizontal acceleration toward the requested movement direction. */
    float acceleration = 40.0F;

    /** @brief Downward acceleration applied when not grounded. */
    float gravity = 24.0F;

    /** @brief Maximum downward velocity magnitude. */
    float terminal_fall_speed = 50.0F;

    /** @brief Fixed simulation tick duration in seconds. */
    float fixed_dt = 1.0F / 60.0F;

    /** @brief Static character collision shape and sweep/slide tuning. */
    stellar::physics::CharacterControllerConfig character{};
};

/**
 * @brief Client-requested movement intent after server-side validation.
 */
struct MovementCommand {
    /** @brief Requested movement direction; sanitized and clamped by the server. */
    std::array<float, 3> wish_direction{};

    /** @brief Requested jump intent; currently accepted but not applied by this deterministic seam. */
    bool jump = false;
};

/**
 * @brief Authoritative kinematic character state owned by the server simulation.
 */
struct MovementState {
    /** @brief Authoritative world-space character center position. */
    std::array<float, 3> position{};

    /** @brief Authoritative world-space character velocity. */
    std::array<float, 3> velocity{};

    /** @brief True when the authoritative character is supported by walkable collision. */
    bool grounded = false;
};

/**
 * @brief Result of one authoritative movement tick.
 */
struct MovementTickResult {
    /** @brief Updated authoritative movement state. */
    MovementState state{};

    /** @brief Collision query result produced by the character controller, if collision existed. */
    stellar::physics::CharacterMoveResult collision{};

    /** @brief True when client intent or unsafe state/config values were clamped or replaced. */
    bool command_was_sanitized = false;
};

/**
 * @brief Initialize movement state from the first player spawn, or origin if absent.
 */
[[nodiscard]] MovementState make_spawn_movement_state(const stellar::world::RuntimeWorld& world);

/**
 * @brief Advance authoritative movement by one fixed tick using server-owned state and world collision.
 */
[[nodiscard]] MovementTickResult simulate_movement_tick(
    const stellar::world::RuntimeWorld& world,
    const MovementState& previous,
    const MovementCommand& command,
    const MovementSimulationConfig& config) noexcept;

} // namespace stellar::server
