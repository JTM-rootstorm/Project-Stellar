#pragma once

#include <array>
#include <vector>

#include "stellar/graphics/BillboardSprite.hpp"
#include "stellar/server/WorldSession.hpp"

namespace stellar::client {

/** @brief Presentation-only configuration for authoritative gameplay snapshot billboards. */
struct GameplayPresentationConfig {
    /** @brief Enables deterministic debug markers for door/gate state when true. */
    bool show_debug_interaction_markers = false;

    /** @brief Fallback sprite width and height in world units for missing or invalid metadata. */
    std::array<float, 2> default_sprite_size{24.0F, 24.0F};

    /** @brief Fallback sprite tint used for authored sprite markers. */
    std::array<float, 4> default_sprite_color{1.0F, 1.0F, 1.0F, 1.0F};

    /** @brief Presentation tint used for active pickup entities. */
    std::array<float, 4> pickup_color{1.0F, 1.0F, 0.25F, 1.0F};

    /** @brief Debug marker tint for closed door/gate entities. */
    std::array<float, 4> door_closed_color{1.0F, 0.25F, 0.25F, 1.0F};

    /** @brief Debug marker tint for open door/gate entities. */
    std::array<float, 4> door_open_color{0.25F, 1.0F, 0.25F, 0.5F};
};

/** @brief Presentation-only billboard frame derived from an authoritative snapshot. */
struct GameplayPresentationFrame {
    /** @brief Backend-neutral billboards ready for graphics submission. */
    std::vector<stellar::graphics::BillboardSprite> sprites;
};

/**
 * @brief Convert server-owned gameplay snapshot state into backend-neutral billboard draw data.
 *
 * This function does not mutate snapshots, resolve GPU resources, or own gameplay truth.
 */
[[nodiscard]] GameplayPresentationFrame make_gameplay_presentation_frame(
    const stellar::server::WorldSnapshot& snapshot,
    const GameplayPresentationConfig& config = {}) noexcept;

} // namespace stellar::client
