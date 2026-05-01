#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "stellar/assets/CollisionAsset.hpp"
#include "stellar/assets/ImageAsset.hpp"
#include "stellar/assets/AnimationAsset.hpp"
#include "stellar/assets/LevelAsset.hpp"
#include "stellar/assets/MaterialAsset.hpp"
#include "stellar/assets/MeshAsset.hpp"
#include "stellar/assets/SkinAsset.hpp"
#include "stellar/assets/TextureAsset.hpp"
#include "stellar/assets/WorldMetadataAsset.hpp"
#include "stellar/scene/SceneGraph.hpp"

namespace stellar::assets {

/**
 * @brief Legacy imported scene payload retained for importer compatibility during migration.
 */
struct SceneAsset {
    std::string source_uri;
    std::vector<ImageAsset> images;
    std::vector<TextureAsset> textures;
    std::vector<SamplerAsset> samplers;
    std::vector<MeshAsset> meshes;
    std::vector<MaterialAsset> materials;
    std::vector<SkinAsset> skins;
    std::vector<AnimationAsset> animations;
    std::vector<stellar::scene::Node> nodes;
    std::vector<stellar::scene::Scene> scenes;
    std::optional<std::size_t> default_scene_index;
    std::optional<LevelCollisionAsset> level_collision;
    WorldMetadataAsset world_metadata;
};

/**
 * @brief Copy legacy imported scene data into the source-neutral playable level contract.
 */
[[nodiscard]] inline LevelAsset to_level_asset(const SceneAsset& scene) {
    LevelAsset level;
    level.source_uri = scene.source_uri;
    level.geometry.images = scene.images;
    level.geometry.textures = scene.textures;
    level.geometry.samplers = scene.samplers;
    level.geometry.meshes = scene.meshes;
    level.level_collision = scene.level_collision;
    level.world_metadata = scene.world_metadata;
    return level;
}

} // namespace stellar::assets
