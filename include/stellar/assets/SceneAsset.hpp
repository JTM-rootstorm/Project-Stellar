#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "stellar/assets/ImageAsset.hpp"
#include "stellar/assets/MaterialAsset.hpp"
#include "stellar/assets/MeshAsset.hpp"
#include "stellar/assets/TextureAsset.hpp"
#include "stellar/scene/SceneGraph.hpp"

namespace stellar::assets {

/**
 * @brief Complete imported asset payload for a glTF-like scene.
 */
struct SceneAsset {
    std::string source_uri;
    std::vector<ImageAsset> images;
    std::vector<TextureAsset> textures;
    std::vector<SamplerAsset> samplers;
    std::vector<MeshAsset> meshes;
    std::vector<MaterialAsset> materials;
    std::vector<stellar::scene::Node> nodes;
    std::vector<stellar::scene::Scene> scenes;
    std::optional<std::size_t> default_scene_index;
};

} // namespace stellar::assets
