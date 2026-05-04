#pragma once

#include <array>
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
    /** @brief Backend-neutral texture handle resolved before material upload. */
    TextureHandle texture;

    /** @brief Sampler state paired with the resolved texture handle. */
    stellar::assets::SamplerAsset sampler;

    /** @brief Texture coordinate set used by this material binding. */
    std::uint32_t texcoord_set = 0;

    /** @brief Texture transform applied before sampling this binding. */
    stellar::assets::TextureTransform transform;
};

/**
 * @brief Backend-neutral material upload payload with resolved GPU handles.
 */
struct MaterialUpload {
    /** @brief Source material factors and shading metadata shared by graphics backends. */
    stellar::assets::MaterialAsset material;

    /** @brief Optional base color texture binding for albedo or sprite color sampling. */
    std::optional<MaterialTextureBinding> base_color_texture;

    /** @brief Optional static level lightmap sampled from secondary UVs. */
    std::optional<MaterialTextureBinding> lightmap_texture;

    /** @brief Static BSP lightstyle multiplier applied to lightmap samples. */
    float lightmap_multiplier = 1.0F;

    /** @brief Minimum lighting floor after lightmap and global light contributions. */
    float lightmap_min = 0.0F;

    /** @brief Linear RGB global level light color added to baked lightmap lighting. */
    std::array<float, 3> global_light_color{0.0F, 0.0F, 0.0F};

    /** @brief Scalar global level light intensity added to baked lightmap lighting. */
    float global_light_intensity = 0.0F;

    /** @brief Optional tangent-space normal texture for presentation-only surface shading. */
    std::optional<MaterialTextureBinding> normal_texture;

    /** @brief Optional lightweight specular strength texture binding. */
    std::optional<MaterialTextureBinding> specular_texture;

    /** @brief Optional metallic/roughness texture preserved for backend-neutral material data. */
    std::optional<MaterialTextureBinding> metallic_roughness_texture;

    /** @brief Optional occlusion texture preserved for backend-neutral material data. */
    std::optional<MaterialTextureBinding> occlusion_texture;

    /** @brief Optional emissive texture preserved for backend-neutral material data. */
    std::optional<MaterialTextureBinding> emissive_texture;
};

} // namespace stellar::graphics
