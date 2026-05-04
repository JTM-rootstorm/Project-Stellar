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
    /** @brief Player start location marker. */
    kPlayerSpawn,
    /** @brief Runtime entity spawn marker. */
    kEntitySpawn,
    /** @brief Trigger volume or trigger point marker. */
    kTrigger,
    /** @brief Presentation sprite marker. */
    kSprite,
    /** @brief Portal or transition marker. */
    kPortal,
    /** @brief Authored object collision volume marker. */
    kObjectCollider,
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
 * @brief Source key/value metadata preserved on an authored world marker.
 */
struct WorldEntityProperty {
    /** @brief Source metadata key. */
    std::string key;

    /** @brief Source metadata value. */
    std::string value;
};

/**
 * @brief Backend-neutral authored marker from a world or level source asset.
 */
struct WorldMarker {
    /** @brief Marker category used by runtime binding and tooling. */
    WorldMarkerType type = WorldMarkerType::kEntitySpawn;

    /** @brief Stable marker name from source metadata when available. */
    std::string name;

    /** @brief Runtime archetype or classname associated with the marker. */
    std::string archetype;

    /** @brief World-space marker position. */
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};

    /** @brief Marker orientation as an XYZW quaternion. */
    std::array<float, 4> rotation{0.0f, 0.0f, 0.0f, 1.0f};

    /** @brief Non-uniform marker scale. */
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};

    /** @brief Optional serialized source-specific marker metadata. */
    std::string extras_json;

    /** @brief Optional script binding copied from authoring metadata. */
    std::optional<WorldScriptBinding> script;

    /** @brief Ordered source key/value metadata preserved for tooling and gameplay binding. */
    std::vector<WorldEntityProperty> properties;
};

/**
 * @brief Backend-neutral collection of authored world metadata markers.
 */
struct WorldMetadataAsset {
    /** @brief Authored markers extracted from the source world or level. */
    std::vector<WorldMarker> markers;
};

} // namespace stellar::assets
