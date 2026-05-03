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
    stellar::assets::TextureTransform transform;
};

/**
 * @brief Backend-neutral material upload payload with resolved GPU handles.
 */
struct MaterialUpload {
    stellar::assets::MaterialAsset material;
    std::optional<MaterialTextureBinding> base_color_texture;
    /** @brief Optional static level lightmap sampled from secondary UVs. */
    std::optional<MaterialTextureBinding> lightmap_texture;
    /** @brief Static BSP lightstyle multiplier applied to lightmap samples. */
    float lightmap_multiplier = 1.0F;
    std::optional<MaterialTextureBinding> normal_texture;
    std::optional<MaterialTextureBinding> metallic_roughness_texture;
    std::optional<MaterialTextureBinding> occlusion_texture;
    std::optional<MaterialTextureBinding> emissive_texture;
};

} // namespace stellar::graphics
