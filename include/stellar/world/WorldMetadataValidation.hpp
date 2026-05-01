#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "stellar/assets/WorldMetadataAsset.hpp"

namespace stellar::world {

struct RuntimeWorld;

/** @brief Sentinel index used when a metadata validation finding is not tied to a marker. */
inline constexpr std::size_t kWorldMetadataValidationInvalidIndex =
    std::numeric_limits<std::size_t>::max();

/** @brief Trigger extents larger than this world-unit threshold are authoring warnings. */
inline constexpr float kWorldMetadataValidationLargeTriggerExtentThreshold = 100000.0F;

/** @brief Object-collider extents larger than this world-unit threshold are authoring warnings. */
inline constexpr float kWorldMetadataValidationLargeObjectColliderExtentThreshold = 100000.0F;

/** @brief Severity for authored world metadata validation messages. */
enum class WorldMetadataValidationSeverity {
    kWarning,
    kError,
};

/** @brief One deterministic world metadata validation finding. */
struct WorldMetadataValidationFinding {
    /** @brief Warning or error severity for the finding. */
    WorldMetadataValidationSeverity severity = WorldMetadataValidationSeverity::kWarning;

    /** @brief Stable machine-readable finding code. */
    std::string code;

    /** @brief Human-readable diagnostic text for tools and logs. */
    std::string message;

    /** @brief Marker index related to the finding, or kWorldMetadataValidationInvalidIndex. */
    std::size_t marker_index = kWorldMetadataValidationInvalidIndex;
};

/** @brief Summary of authored world metadata validation results. */
struct WorldMetadataValidationReport {
    /** @brief Deterministically ordered validation findings. */
    std::vector<WorldMetadataValidationFinding> findings;

    /** @brief True when at least one finding has error severity. */
    bool has_errors = false;
};

/** @brief Configurable policy for metadata authoring validation. */
struct WorldMetadataValidationConfig {
    /** @brief Report an error when no player spawn marker is present. */
    bool require_player_spawn = true;

    /** @brief Report a warning when more than one player spawn marker is present. */
    bool warn_multiple_player_spawns = true;

    /** @brief Report a warning when a trigger marker has zero or empty extents. */
    bool warn_empty_trigger_extents = true;

    /** @brief Report a warning when an object collider marker has zero or empty extents. */
    bool warn_empty_object_collider_extents = true;

    /** @brief Report a warning when a sprite marker name is empty. */
    bool warn_empty_sprite_names = true;

    /** @brief Report an error when an entity spawn marker has no archetype. */
    bool warn_empty_entity_archetypes = true;

    /** @brief Report a warning when a script binding is attached to a non-trigger marker. */
    bool warn_script_binding_unsupported_marker_types = true;
};

/** @brief Validate backend-neutral world metadata markers for runtime use. */
[[nodiscard]] WorldMetadataValidationReport validate_world_metadata(
    const stellar::assets::WorldMetadataAsset& metadata,
    const WorldMetadataValidationConfig& config = {});

/** @brief Validate world metadata copied into a RuntimeWorld. */
[[nodiscard]] WorldMetadataValidationReport validate_world_metadata(
    const RuntimeWorld& world,
    const WorldMetadataValidationConfig& config = {});

} // namespace stellar::world
