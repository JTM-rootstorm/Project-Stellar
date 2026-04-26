#pragma once

#include <cstdint>
#include <optional>

#include "stellar/assets/MaterialAsset.hpp"
#include "stellar/assets/TextureAsset.hpp"
#include "stellar/graphics/GraphicsHandles.hpp"

namespace stellar::graphics {

/**
 * @brief Resolved texture and sampler state used by a GPU material upload.
 */
struct MaterialTextureBinding {
    TextureHandle texture;
    stellar::assets::SamplerAsset sampler;
    std::uint32_t texcoord_set = 0;
};

/**
 * @brief Backend-neutral material upload payload with resolved GPU handles.
 */
struct MaterialUpload {
    stellar::assets::MaterialAsset material;
    std::optional<MaterialTextureBinding> base_color_texture;
};

} // namespace stellar::graphics
