#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include "stellar/assets/TextureAsset.hpp"

namespace stellar::assets {

/**
 * @brief Alpha blending mode used by backend-neutral materials.
 */
enum class AlphaMode {
    /** @brief Fully opaque material with no alpha blending or cutoff. */
    kOpaque,
    /** @brief Alpha-tested material using MaterialAsset::alpha_cutoff. */
    kMask,
    /** @brief Alpha-blended material for transparent presentation surfaces. */
    kBlend,
};

/**
 * @brief Backend-neutral material description.
 */
struct MaterialAsset {
    /** @brief Stable material name used for diagnostics and renderer lookup. */
    std::string name;

    /** @brief Linear RGBA multiplier applied to the base color texture or fallback color. */
    std::array<float, 4> base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};

    /** @brief Optional base color texture slot. */
    std::optional<MaterialTextureSlot> base_color_texture;

    /** @brief Optional tangent-space normal map texture slot. */
    std::optional<MaterialTextureSlot> normal_texture;

    /** @brief Optional grayscale or RGB texture controlling lightweight specular strength. */
    std::optional<MaterialTextureSlot> specular_texture;

    /** @brief Optional metallic-roughness texture slot for glTF-style material data. */
    std::optional<MaterialTextureSlot> metallic_roughness_texture;

    /** @brief Optional ambient occlusion texture slot. */
    std::optional<MaterialTextureSlot> occlusion_texture;

    /** @brief Optional emissive texture slot. */
    std::optional<MaterialTextureSlot> emissive_texture;

    /** @brief Linear RGB emissive multiplier. */
    std::array<float, 3> emissive_factor{0.0f, 0.0f, 0.0f};

    /** @brief Scalar applied to normal-map XY perturbation before reconstruction. */
    float normal_scale = 1.0f;

    /** @brief Presentation-only detail lighting amount for lightmapped normal maps. */
    float normal_light_strength = 0.0f;

    /** @brief Lightweight specular highlight multiplier; zero preserves legacy behavior. */
    float specular_factor = 0.0f;

    /** @brief Lightweight specular exponent used by presentation backends. */
    float specular_power = 32.0f;

    /** @brief Scalar strength for the optional occlusion texture. */
    float occlusion_strength = 1.0f;

    /** @brief Scalar metallic factor for physically inspired material imports. */
    float metallic_factor = 1.0f;

    /** @brief Scalar roughness factor for physically inspired material imports. */
    float roughness_factor = 1.0f;

    /** @brief Alpha handling mode used by presentation backends. */
    AlphaMode alpha_mode = AlphaMode::kOpaque;

    /** @brief Alpha-test threshold used when alpha_mode is AlphaMode::kMask. */
    float alpha_cutoff = 0.5f;

    /** @brief True when both sides of triangles should be rendered. */
    bool double_sided = false;

    /** @brief True when lighting is disabled for this material. */
    bool unlit = false;
};

} // namespace stellar::assets
