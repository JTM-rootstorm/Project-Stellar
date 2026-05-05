#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "stellar/server/GameplayWorld.hpp"

namespace stellar::server {

/** @brief Deterministic cadence settings for authoritative footstep events. */
struct FootstepTrackerConfig {
    /** @brief Minimum horizontal speed in world units per second required for footstep cadence. */
    float min_horizontal_speed = 24.0F;

    /** @brief Grounded travel distance required between walking footstep events. */
    float walk_step_distance = 56.0F;

    /** @brief Grounded travel distance required between faster movement footstep events. */
    float run_step_distance = 44.0F;

    /** @brief True to emit one footstep when becoming grounded after airborne movement. */
    bool emit_on_landing = false;
};

/** @brief Authoritative surface identity under a grounded player. */
struct FootstepSurfaceHit {
    /** @brief True when the surface data came from a valid authoritative collision hit. */
    bool valid = false;

    /** @brief Server-safe surface id such as generic, wood, or metal. */
    std::string surface_id = "generic";

    /** @brief Original source texture/material name, when collision metadata provides one. */
    std::string source_material_name;
};

/** @brief Server-owned footstep event before conversion to the transport protocol. */
struct FootstepEvent {
    /** @brief Authoritative tick associated with this footstep. */
    std::uint64_t tick = 0;

    /** @brief Player slot that produced the footstep. */
    PlayerId player_id = 0;

    /** @brief Gameplay entity associated with the player, when known. */
    EntityId entity_id = 0;

    /** @brief Server-safe surface id such as generic, wood, or metal. */
    std::string surface_id = "generic";

    /** @brief Original source texture/material name, when known. */
    std::string source_material_name;
};

/** @brief Inputs for one deterministic footstep cadence update. */
struct FootstepTrackerInput {
    /** @brief Player slot that owns the movement state. */
    PlayerId player_id = 0;

    /** @brief Gameplay entity associated with the player, when known. */
    EntityId entity_id = 0;

    /** @brief Authoritative tick associated with this update. */
    std::uint64_t tick = 0;

    /** @brief Authoritative position before movement. */
    std::array<float, 3> previous_position{};

    /** @brief Authoritative position after movement. */
    std::array<float, 3> current_position{};

    /** @brief Authoritative velocity after movement. */
    std::array<float, 3> current_velocity{};

    /** @brief True when authoritative movement is grounded on walkable collision. */
    bool grounded = false;

    /** @brief Authoritative surface under the player for this update. */
    FootstepSurfaceHit surface{};
};

/** @brief Deterministic distance-based footstep cadence tracker for one authoritative player. */
class FootstepTracker {
public:
    /** @brief Construct a tracker with deterministic cadence settings. */
    explicit FootstepTracker(FootstepTrackerConfig config = {});

    /** @brief Reset accumulated cadence state at a spawn, teleport, or session reset. */
    void reset(std::array<float, 3> position = {}) noexcept;

    /** @brief Advance cadence and optionally emit one authoritative footstep event. */
    [[nodiscard]] std::optional<FootstepEvent> update(const FootstepTrackerInput& input);

private:
    FootstepTrackerConfig config_{};
    std::array<float, 3> last_position_{};
    float accumulated_distance_ = 0.0F;
    bool was_grounded_ = false;
};

} // namespace stellar::server
