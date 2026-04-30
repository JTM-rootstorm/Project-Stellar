#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "stellar/assets/TextureAsset.hpp"
#include "stellar/graphics/GraphicsHandles.hpp"

namespace stellar::graphics {

/**
 * @brief Backend-neutral 2D sprite description in 3D world space.
 */
struct BillboardSprite {
    /** @brief Sprite center position in world space. */
    std::array<float, 3> position{0.0F, 0.0F, 0.0F};

    /** @brief Sprite world-space width and height. */
    std::array<float, 2> size{1.0F, 1.0F};

    /** @brief Per-sprite color multiplied with the sampled texture. */
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};

    /** @brief Optional GPU texture handle sampled as the base color texture. */
    TextureHandle texture{};

    /** @brief Sampler state used when texture is valid. */
    stellar::assets::SamplerAsset sampler{};

    /** @brief True when the sprite should be alpha blended and sorted back-to-front. */
    bool alpha_blend = true;

    /** @brief True when the sprite should use alpha cutoff instead of blend transparency. */
    bool alpha_mask = false;

    /** @brief Alpha cutoff threshold used when alpha_mask is true. */
    float alpha_cutoff = 0.5F;
};

/**
 * @brief Camera basis and transform used to expand billboard sprites into quads.
 */
struct BillboardView {
    /** @brief Column-major view-projection matrix used for final sprite transforms. */
    std::array<float, 16> view_projection{};

    /** @brief Column-major view matrix used for alpha-blend depth sorting. */
    std::array<float, 16> view{};

    /** @brief World-space camera right vector. */
    std::array<float, 3> camera_right{1.0F, 0.0F, 0.0F};

    /** @brief World-space camera up vector. */
    std::array<float, 3> camera_up{0.0F, 1.0F, 0.0F};
};

/**
 * @brief Expanded billboard quad draw data used by renderer tests and backend submission.
 */
struct BillboardQuad {
    /** @brief Four world-space quad corners in index order 0, 1, 2, 3. */
    std::array<std::array<float, 3>, 4> positions{};

    /** @brief Four texture coordinates matching positions. */
    std::array<std::array<float, 2>, 4> texcoords{};

    /** @brief Original sprite center position in world space. */
    std::array<float, 3> center{};

    /** @brief Original sprite world-space width and height. */
    std::array<float, 2> size{};

    /** @brief Preserved per-sprite color. */
    std::array<float, 4> color{};

    /** @brief Preserved texture handle. */
    TextureHandle texture{};

    /** @brief Preserved sampler state. */
    stellar::assets::SamplerAsset sampler{};

    /** @brief True when this quad belongs to the alpha-blend group. */
    bool alpha_blend = false;

    /** @brief True when this quad belongs to the alpha-mask group. */
    bool alpha_mask = false;

    /** @brief Preserved alpha cutoff. */
    float alpha_cutoff = 0.5F;

    /** @brief View-space Z depth used for deterministic sorting. */
    float depth = 0.0F;

    /** @brief Original submission index used as a stable tie-breaker. */
    std::size_t submission_index = 0;
};

/**
 * @brief Expand and classify sprites into deterministic billboard draw order.
 */
[[nodiscard]] std::vector<BillboardQuad> build_billboard_quads(
    std::span<const BillboardSprite> sprites,
    const BillboardView& view) noexcept;

} // namespace stellar::graphics
