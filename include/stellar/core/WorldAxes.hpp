#pragma once

#include <array>

#include "stellar/core/WorldUnits.hpp"

namespace stellar::core {

/** @brief World-space right direction in the Z-up axis contract: positive X. */
inline constexpr std::array<float, 3> kWorldRight{1.0F, 0.0F, 0.0F};

/** @brief World-space forward direction in the Z-up axis contract: positive Y. */
inline constexpr std::array<float, 3> kWorldForward{0.0F, 1.0F, 0.0F};

/** @brief World-space up direction in the Z-up axis contract: positive Z. */
inline constexpr std::array<float, 3> kWorldUp{0.0F, 0.0F, 1.0F};

/** @brief World-space down/gravity direction in the Z-up axis contract: negative Z. */
inline constexpr std::array<float, 3> kWorldDown{0.0F, 0.0F, -1.0F};

/** @brief Zero-based coordinate index for the world right axis: X. */
inline constexpr int kWorldRightIndex = 0;

/** @brief Zero-based coordinate index for the world forward axis: Y. */
inline constexpr int kWorldForwardIndex = 1;

/** @brief Zero-based coordinate index for the world up axis: Z. */
inline constexpr int kWorldUpIndex = 2;

/** @brief Zero-based coordinate index for the horizontal world X axis. */
inline constexpr int kHorizontalPlaneXIndex = kWorldRightIndex;

/** @brief Zero-based coordinate index for the horizontal world Y axis. */
inline constexpr int kHorizontalPlaneYIndex = kWorldForwardIndex;

/** @brief Default player center height above the floor in inch-scale world units. */
inline constexpr float kDefaultPlayerSpawnHeightInches = kPlayerHeightInches * 0.5F;

} // namespace stellar::core
