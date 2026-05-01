#pragma once

namespace stellar::core {

/** @brief Number of Stellar gameplay world units in one authored inch. */
inline constexpr float kUnitsPerInch = 1.0F;

/** @brief Number of inches in one authored foot. */
inline constexpr float kInchesPerFoot = 12.0F;

/** @brief Default authoritative player capsule height in inch-scale world units. */
inline constexpr float kPlayerHeightInches = 72.0F;

/** @brief Default authoritative player capsule radius in inch-scale world units. */
inline constexpr float kPlayerRadiusInches = 16.0F;

/** @brief Default player eye height in inch-scale world units. */
inline constexpr float kPlayerEyeHeightInches = 64.0F;

/** @brief Default authoritative player step height in inch-scale world units. */
inline constexpr float kPlayerStepHeightInches = 18.0F;

/** @brief Default character-controller skin width in inch-scale world units. */
inline constexpr float kPlayerSkinWidthInches = 0.5F;

/** @brief Default post-move ground snap distance in inch-scale world units. */
inline constexpr float kGroundSnapDistanceInches = 4.0F;

/** @brief Default authoritative walk speed in inch-scale world units per second. */
inline constexpr float kWalkSpeedInchesPerSecond = 160.0F;

/** @brief Default authoritative walk acceleration in inch-scale world units per second squared. */
inline constexpr float kAccelerationInchesPerSecondSquared = 1200.0F;

/** @brief Default authoritative gravity in inch-scale world units per second squared. */
inline constexpr float kGravityInchesPerSecondSquared = 800.0F;

/** @brief Default authoritative terminal fall speed in inch-scale world units per second. */
inline constexpr float kTerminalFallSpeedInchesPerSecond = 2400.0F;

/** @brief Converts authored inches to Stellar gameplay world units. */
[[nodiscard]] constexpr float inches_to_units(float inches) noexcept {
    return inches * kUnitsPerInch;
}

/** @brief Converts authored feet to Stellar gameplay world units. */
[[nodiscard]] constexpr float feet_to_units(float feet) noexcept {
    return inches_to_units(feet * kInchesPerFoot);
}

} // namespace stellar::core
