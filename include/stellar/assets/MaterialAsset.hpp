#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include "stellar/assets/TextureAsset.hpp"

namespace stellar::assets {

/**
 * @brief Alpha blending mode used by glTF-compatible materials.
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
    std::optional<MaterialTextureSlot> metallic_roughness_texture;
    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
    AlphaMode alpha_mode = AlphaMode::kOpaque;
    float alpha_cutoff = 0.5f;
    bool double_sided = false;
};

} // namespace stellar::assets
