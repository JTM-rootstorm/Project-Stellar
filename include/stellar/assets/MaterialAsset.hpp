#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>

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
    std::optional<std::size_t> base_color_texture_index;
    std::optional<std::size_t> normal_texture_index;
    std::optional<std::size_t> metallic_roughness_texture_index;
    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
    AlphaMode alpha_mode = AlphaMode::kOpaque;
    float alpha_cutoff = 0.5f;
    bool double_sided = false;
};

} // namespace stellar::assets
