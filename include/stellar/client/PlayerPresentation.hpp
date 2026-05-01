#pragma once

#include <array>
#include <optional>

#include "stellar/core/WorldUnits.hpp"
#include "stellar/server/WorldSession.hpp"

namespace stellar::client {

/** @brief Camera follow settings derived from authoritative player snapshots. */
struct PlayerCameraConfig {
    /** @brief World-space offset from the player position to the camera eye. */
    std::array<float, 3> follow_offset{0.0F, stellar::core::kPlayerHeightInches,
                                       stellar::core::feet_to_units(12.0F)};

    /** @brief World-space offset from the player position to the camera look-at target. */
    std::array<float, 3> look_at_offset{0.0F, 60.0F, 0.0F};

    /** @brief Requested near clip plane; finite positive values are preserved above 0.01. */
    float near_plane = 0.1F;

    /** @brief Requested far clip plane; finite values greater than near are preserved. */
    float far_plane = 4096.0F;
};

/** @brief Presentation state derived from one authoritative player snapshot. */
struct PlayerPresentationState {
    /** @brief Authoritative world-space player position copied for presentation. */
    std::array<float, 3> position{};

    /** @brief Authoritative x, y, z, w player rotation copied for presentation. */
    std::array<float, 4> rotation{0.0F, 0.0F, 0.0F, 1.0F};

    /** @brief True when the authoritative snapshot says the player is grounded. */
    bool grounded = false;
};

/** @brief Camera parameters for rendering the current authoritative player snapshot. */
struct PlayerCameraFrame {
    /** @brief Sanitized world-space camera eye position. */
    std::array<float, 3> eye{};

    /** @brief Sanitized world-space camera look-at target. */
    std::array<float, 3> target{};

    /** @brief Sanitized near clip plane, never less than 0.01. */
    float near_plane = 0.1F;

    /** @brief Sanitized far clip plane, always greater than near_plane. */
    float far_plane = 4096.0F;
};

/** @brief Extract player presentation state from the latest authoritative world snapshot. */
[[nodiscard]] std::optional<PlayerPresentationState> make_player_presentation_state(
    const stellar::server::WorldSnapshot& snapshot,
    stellar::server::PlayerId player_id);

/**
 * @brief Compute a deterministic camera frame from player presentation state.
 *
 * Non-finite position and offset components are treated as zero. The near plane preserves finite
 * positive input clamped to at least 0.01. The far plane preserves finite input only when it is
 * greater than the sanitized near plane; otherwise it is raised to near + 0.01.
 */
[[nodiscard]] PlayerCameraFrame make_player_camera_frame(
    const PlayerPresentationState& player,
    const PlayerCameraConfig& config = {}) noexcept;

} // namespace stellar::client
