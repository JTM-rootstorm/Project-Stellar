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
    kOpaque,
    kMask,
    kBlend,
};

/**
 * @brief Backend-neutral material description.
 */
struct MaterialAsset {
    std::string name;
    std::array<float, 4> base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
    std::optional<MaterialTextureSlot> base_color_texture;
    std::optional<MaterialTextureSlot> normal_texture;
    /** @brief Optional grayscale or RGB texture controlling lightweight specular strength. */
    std::optional<MaterialTextureSlot> specular_texture;
    std::optional<MaterialTextureSlot> metallic_roughness_texture;
    std::optional<MaterialTextureSlot> occlusion_texture;
    std::optional<MaterialTextureSlot> emissive_texture;
    std::array<float, 3> emissive_factor{0.0f, 0.0f, 0.0f};
    /** @brief Scalar applied to normal-map XY perturbation before reconstruction. */
    float normal_scale = 1.0f;
    /** @brief Presentation-only detail lighting amount for lightmapped normal maps. */
    float normal_light_strength = 0.0f;
    /** @brief Lightweight specular highlight multiplier; zero preserves legacy behavior. */
    float specular_factor = 0.0f;
    /** @brief Lightweight specular exponent used by presentation backends. */
    float specular_power = 32.0f;
    float occlusion_strength = 1.0f;
    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
    AlphaMode alpha_mode = AlphaMode::kOpaque;
    float alpha_cutoff = 0.5f;
    bool double_sided = false;
    /** @brief True when lighting is disabled for this material. */
    bool unlit = false;
};

} // namespace stellar::assets
