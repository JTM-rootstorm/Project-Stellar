#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "stellar/assets/CollisionAsset.hpp"

namespace stellar::world {

/** @brief Sentinel index used when a validation finding is not tied to one mesh or triangle. */
inline constexpr std::size_t kCollisionValidationInvalidIndex =
    std::numeric_limits<std::size_t>::max();

/** @brief Bounds with any extent larger than this world-unit threshold are authoring warnings. */
inline constexpr float kCollisionValidationLargeBoundsThreshold = 100000.0F;

/** @brief Severity for authored collision validation messages. */
enum class CollisionValidationSeverity {
    kWarning,
    kError,
};

/** @brief One deterministic collision validation finding. */
struct CollisionValidationFinding {
    /** @brief Warning or error severity for the finding. */
    CollisionValidationSeverity severity = CollisionValidationSeverity::kWarning;

    /** @brief Stable machine-readable finding code. */
    std::string code;

    /** @brief Human-readable diagnostic text for tools and logs. */
    std::string message;

    /** @brief Mesh index related to the finding, or kCollisionValidationInvalidIndex. */
    std::size_t mesh_index = kCollisionValidationInvalidIndex;

    /** @brief Triangle index related to the finding, or kCollisionValidationInvalidIndex. */
    std::size_t triangle_index = kCollisionValidationInvalidIndex;
};

/** @brief Summary of static collision validation results. */
struct CollisionValidationReport {
    /** @brief Deterministically ordered validation findings. */
    std::vector<CollisionValidationFinding> findings;

    /** @brief True when at least one finding has error severity. */
    bool has_errors = false;
};

/** @brief Validate backend-neutral static collision data for authoring and runtime use. */
[[nodiscard]] CollisionValidationReport validate_level_collision(
    const stellar::assets::LevelCollisionAsset& collision);

} // namespace stellar::world
