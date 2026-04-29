#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace stellar::assets {

/**
 * @brief Texture minification or magnification filter imported from a glTF sampler.
 */
enum class TextureFilter {
    kUnspecified,
    kNearest,
    kLinear,
    kNearestMipmapNearest,
    kLinearMipmapNearest,
    kNearestMipmapLinear,
    kLinearMipmapLinear,
};

/**
 * @brief Texture coordinate wrapping mode imported from a glTF sampler.
 */
enum class TextureWrapMode {
    kClampToEdge,
    kMirroredRepeat,
    kRepeat,
};

/**
 * @brief Backend-neutral texture sampler state.
 */
struct SamplerAsset {
    std::string name;
    TextureFilter mag_filter = TextureFilter::kUnspecified;
    TextureFilter min_filter = TextureFilter::kUnspecified;
    TextureWrapMode wrap_s = TextureWrapMode::kRepeat;
    TextureWrapMode wrap_t = TextureWrapMode::kRepeat;
};

/**
 * @brief Texture color-space interpretation used during GPU upload and sampling.
 */
enum class TextureColorSpace {
    kLinear,
    kSrgb,
};

/**
 * @brief Backend-neutral texture binding to an imported image and optional sampler.
 */
struct TextureAsset {
    std::string name;
    std::optional<std::size_t> image_index;
    std::optional<std::size_t> sampler_index;
    TextureColorSpace color_space = TextureColorSpace::kLinear;
};

/**
 * @brief KHR_texture_transform state for a material texture slot.
 */
struct TextureTransform {
    /** @brief UV offset applied after rotation and scale. */
    std::array<float, 2> offset{0.0f, 0.0f};

    /** @brief Counter-clockwise UV rotation in radians. */
    float rotation = 0.0f;

    /** @brief Non-uniform UV scale applied before rotation. */
    std::array<float, 2> scale{1.0f, 1.0f};

    /** @brief Optional texture coordinate set override from the extension. */
    std::optional<std::uint32_t> texcoord_set;

    /** @brief True when KHR_texture_transform was present on the slot. */
    bool enabled = false;
};

/**
 * @brief Material reference to an engine texture plus the glTF texture coordinate set.
 */
struct MaterialTextureSlot {
    std::size_t texture_index = 0;
    std::uint32_t texcoord_set = 0;
    float scale = 1.0f;
    TextureTransform transform;
};

} // namespace stellar::assets
