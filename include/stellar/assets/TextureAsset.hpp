#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace stellar::assets {

/**
 * @brief Texture minification or magnification filter for imported sampler data.
 */
enum class TextureFilter {
    /** @brief Source did not specify a filter mode. */
    kUnspecified,
    /** @brief Sample from the nearest texel without interpolation. */
    kNearest,
    /** @brief Linearly interpolate between neighboring texels. */
    kLinear,
    /** @brief Nearest texel from the nearest mip level. */
    kNearestMipmapNearest,
    /** @brief Linear texel filtering from the nearest mip level. */
    kLinearMipmapNearest,
    /** @brief Nearest texel filtering blended between mip levels. */
    kNearestMipmapLinear,
    /** @brief Linear texel filtering blended between mip levels. */
    kLinearMipmapLinear,
};

/**
 * @brief Texture coordinate wrapping mode for imported sampler data.
 */
enum class TextureWrapMode {
    /** @brief Clamp coordinates to the texture edge. */
    kClampToEdge,
    /** @brief Repeat coordinates with every other tile mirrored. */
    kMirroredRepeat,
    /** @brief Repeat coordinates normally outside the 0-1 range. */
    kRepeat,
};

/**
 * @brief Backend-neutral texture sampler state.
 */
struct SamplerAsset {
    /** @brief Stable sampler name used for diagnostics and renderer lookup. */
    std::string name;

    /** @brief Magnification filter used when sampling above native resolution. */
    TextureFilter mag_filter = TextureFilter::kUnspecified;

    /** @brief Minification filter used when sampling below native resolution. */
    TextureFilter min_filter = TextureFilter::kUnspecified;

    /** @brief Horizontal texture coordinate wrapping mode. */
    TextureWrapMode wrap_s = TextureWrapMode::kRepeat;

    /** @brief Vertical texture coordinate wrapping mode. */
    TextureWrapMode wrap_t = TextureWrapMode::kRepeat;
};

/**
 * @brief Texture color-space interpretation used during GPU upload and sampling.
 */
enum class TextureColorSpace {
    /** @brief Texture samples are interpreted as linear values. */
    kLinear,
    /** @brief Texture samples are converted from sRGB to linear values when sampled. */
    kSrgb,
};

/**
 * @brief Backend-neutral texture binding to an imported image and optional sampler.
 */
struct TextureAsset {
    /** @brief Stable texture name used by materials and diagnostics. */
    std::string name;

    /** @brief Optional image index into the owning asset image collection. */
    std::optional<std::size_t> image_index;

    /** @brief Optional sampler index into the owning asset sampler collection. */
    std::optional<std::size_t> sampler_index;

    /** @brief Color-space interpretation used by upload and sampling paths. */
    TextureColorSpace color_space = TextureColorSpace::kLinear;
};

/**
 * @brief Texture transform state for a material texture slot.
 */
struct TextureTransform {
    /** @brief UV offset applied after rotation and scale. */
    std::array<float, 2> offset{0.0f, 0.0f};

    /** @brief Counter-clockwise UV rotation in radians. */
    float rotation = 0.0f;

    /** @brief Non-uniform UV scale applied before rotation. */
    std::array<float, 2> scale{1.0f, 1.0f};

    /** @brief Optional texture coordinate set override. */
    std::optional<std::uint32_t> texcoord_set;

    /** @brief True when a texture transform is present on the slot. */
    bool enabled = false;
};

/**
 * @brief Material reference to an engine texture plus the texture coordinate set.
 */
struct MaterialTextureSlot {
    /** @brief Texture index into the owning asset texture collection. */
    std::size_t texture_index = 0;

    /** @brief Texture coordinate set used by this material slot. */
    std::uint32_t texcoord_set = 0;

    /** @brief Scalar slot multiplier interpreted by the material channel. */
    float scale = 1.0f;

    /** @brief Optional UV transform applied before sampling this slot. */
    TextureTransform transform;
};

} // namespace stellar::assets
