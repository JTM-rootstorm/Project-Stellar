#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace stellar::assets {

/**
 * @brief High-level authored world marker category extracted from level metadata.
 */
enum class WorldMarkerType {
    kPlayerSpawn,
    kEntitySpawn,
    kTrigger,
    kSprite,
    kPortal,
};

/**
 * @brief Optional script binding attached to an authored world marker.
 */
struct WorldScriptBinding {
    /** @brief Stable script identifier or asset-relative script path. */
    std::string script_id;

    /** @brief Optional Lua table name containing hook functions. */
    std::string table_name;
};

/**
 * @brief Backend-neutral authored marker from a world or level source asset.
 */
struct WorldMarker {
    WorldMarkerType type = WorldMarkerType::kEntitySpawn;
    std::string name;
    std::string archetype;
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
    std::string extras_json;

    /** @brief Optional script binding copied from authoring metadata. */
    std::optional<WorldScriptBinding> script;
};

/**
 * @brief Backend-neutral collection of authored world metadata markers.
 */
struct WorldMetadataAsset {
    std::vector<WorldMarker> markers;
};

} // namespace stellar::assets
