#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "stellar/assets/CollisionAsset.hpp"
#include "stellar/assets/ImageAsset.hpp"
#include "stellar/assets/MeshAsset.hpp"
#include "stellar/assets/TextureAsset.hpp"
#include "stellar/assets/WorldMetadataAsset.hpp"

namespace stellar::assets {

/**
 * @brief Source-neutral material metadata for a static level surface.
 */
struct LevelSurfaceMaterial {
    /** @brief Runtime/display name for the surface material. */
    std::string name;

    /** @brief Optional texture index into LevelGeometryAsset::textures. */
    std::optional<std::size_t> texture_index;

    /** @brief Optional lightmap texture index when imported visibility/light data provides one. */
    std::optional<std::size_t> lightmap_index;

    /** @brief Original source material name preserved for tooling and diagnostics. */
    std::string source_name;
};

/**
 * @brief Source-neutral imported lightmap metadata for static level surfaces.
 */
struct LevelLightmap {
    /** @brief Index into LevelGeometryAsset lightmap image/storage collections. */
    std::size_t image_index = 0;

    /** @brief Lightmap dimensions in texels when known. */
    std::array<std::uint32_t, 2> size{};

    /** @brief Classic BSP light style index. */
    std::uint8_t style = 0;

    /** @brief Source lightmap or lump name preserved for diagnostics. */
    std::string source_name;
};

/**
 * @brief Source-neutral static level surface mapped to imported mesh geometry.
 */
struct LevelSurface {
    /** @brief Stable surface name from the level source when available. */
    std::string name;

    /** @brief Mesh index containing this surface geometry. */
    std::size_t mesh_index = 0;

    /** @brief Primitive index within the referenced mesh. */
    std::size_t primitive_index = 0;

    /** @brief Optional material index into LevelGeometryAsset::materials. */
    std::optional<std::size_t> material_index;

    /** @brief World-space axis-aligned bounds minimum. */
    std::array<float, 3> bounds_min{};

    /** @brief World-space axis-aligned bounds maximum. */
    std::array<float, 3> bounds_max{};

    /** @brief Source format surface flags preserved for importer-specific tooling. */
    std::uint32_t source_flags = 0;

    /** @brief Source-neutral brush/model index that owns this surface; worldspawn is model 0. */
    std::uint32_t brush_model_index = 0;
};

/**
 * @brief Source-neutral imported brush model entity with independent runtime ownership.
 */
struct LevelBrushEntity {
    /** @brief Stable brush entity name, usually targetname or generated model name. */
    std::string name;

    /** @brief Authored entity classname such as func_door, func_button, or func_wall. */
    std::string classname;

    /** @brief Authored targetname used by server-authoritative target routing. */
    std::string targetname;

    /** @brief Authored target fired by triggers/buttons where applicable. */
    std::string target;

    /** @brief Original source model token such as *1 when available. */
    std::string model;

    /** @brief Source-neutral brush model index; worldspawn is model 0 and brush entities are > 0. */
    std::uint32_t bsp_model_index = 0;

    /** @brief Mesh index in LevelGeometryAsset::meshes containing renderable faces for this brush. */
    std::size_t mesh_index = 0;

    /** @brief Collision mesh name that runtime collision overlays can target. */
    std::string collision_mesh_name;

    /** @brief Initial server-owned translation for this brush entity. */
    std::array<float, 3> origin{0.0F, 0.0F, 0.0F};

    /** @brief Imported local/world bounds minimum before runtime movement. */
    std::array<float, 3> bounds_min{};

    /** @brief Imported local/world bounds maximum before runtime movement. */
    std::array<float, 3> bounds_max{};

    /** @brief Ordered inert source properties preserved for runtime binding and diagnostics. */
    std::vector<WorldEntityProperty> properties;
};

/**
 * @brief Source-neutral static geometry payload for a playable level.
 */
struct LevelGeometryAsset {
    /** @brief Mesh data suitable for backend-neutral render upload. */
    std::vector<MeshAsset> meshes;

    /** @brief Static level surfaces mapped onto mesh primitives. */
    std::vector<LevelSurface> surfaces;

    /** @brief Source-neutral surface material records. */
    std::vector<LevelSurfaceMaterial> materials;

    /** @brief CPU-side image payloads referenced by textures. */
    std::vector<ImageAsset> images;

    /** @brief Texture descriptors referenced by level surface materials. */
    std::vector<TextureAsset> textures;

    /** @brief Texture sampler descriptors used by level textures. */
    std::vector<SamplerAsset> samplers;

    /** @brief Imported lightmap records referenced by LevelSurfaceMaterial::lightmap_index. */
    std::vector<LevelLightmap> lightmaps;

    /** @brief Raw source lighting bytes retained for later lightmap import phases. */
    std::vector<std::byte> raw_lighting;

    /** @brief Imported brush entities with independently transformable render/collision ownership. */
    std::vector<LevelBrushEntity> brush_entities;
};

/**
 * @brief Source-neutral classic BSP leaf metadata for visibility consumers.
 */
struct LevelLeaf {
    /** @brief Source contents value for this leaf. */
    std::int32_t contents = 0;

    /** @brief World-space leaf bounds minimum. */
    std::array<float, 3> bounds_min{};

    /** @brief World-space leaf bounds maximum. */
    std::array<float, 3> bounds_max{};

    /** @brief Surface indices referenced by this leaf. */
    std::vector<std::size_t> surface_indices;

    /** @brief Optional offset into LevelVisibilityAsset::compressed_pvs. */
    std::optional<std::size_t> compressed_pvs_offset;
};

/**
 * @brief Optional visibility metadata placeholder for static level imports.
 */
struct LevelVisibilityAsset {
    /** @brief True when source visibility data was imported. */
    bool available = false;

    /** @brief Classic BSP leaf records with surface references. */
    std::vector<LevelLeaf> leaves;

    /** @brief Raw classic BSP compressed PVS bytes. */
    std::vector<std::byte> compressed_pvs;

    /** @brief Number of visibility clusters represented by the imported data. */
    std::size_t cluster_count = 0;
};

/**
 * @brief Canonical source-neutral asset contract for playable level runtime assembly.
 */
struct LevelAsset {
    /** @brief Source URI or logical asset identifier used for diagnostics. */
    std::string source_uri;

    /** @brief Static level geometry and material payload. */
    LevelGeometryAsset geometry;

    /** @brief Optional static collision/query data for authoritative runtime use. */
    std::optional<LevelCollisionAsset> level_collision;

    /** @brief Authored world markers, script bindings, and preserved source metadata. */
    WorldMetadataAsset world_metadata;

    /** @brief Optional imported visibility/lightmap placeholder data. */
    LevelVisibilityAsset visibility;
};

/**
 * @brief Find the first visibility leaf whose bounds contain a world-space point.
 *
 * This helper is source-neutral and intended for renderer-side presentation
 * culling only. If visibility data is unavailable, the point contains
 * non-finite values, or no leaf bounds contain the point, no leaf is returned.
 */
[[nodiscard]] inline std::optional<std::size_t>
find_level_leaf_for_point(const LevelVisibilityAsset &visibility,
                          std::array<float, 3> position) noexcept {
    if (!visibility.available) {
        return std::nullopt;
    }

    for (const float coordinate : position) {
        if (!(coordinate <= std::numeric_limits<float>::max() &&
              coordinate >= -std::numeric_limits<float>::max())) {
            return std::nullopt;
        }
    }

    for (std::size_t leaf_index = 0; leaf_index < visibility.leaves.size();
         ++leaf_index) {
        const LevelLeaf &leaf = visibility.leaves[leaf_index];
        bool contains = true;
        for (std::size_t axis = 0; axis < position.size(); ++axis) {
            if (position[axis] < leaf.bounds_min[axis] ||
                position[axis] > leaf.bounds_max[axis]) {
                contains = false;
                break;
            }
        }
        if (contains) {
            return leaf_index;
        }
    }

    return std::nullopt;
}

namespace detail {

[[nodiscard]] inline std::optional<std::vector<bool>> decompress_level_pvs_bits(
    const LevelVisibilityAsset &visibility, std::size_t offset,
    std::size_t bit_count) noexcept {
    if (bit_count == 0 || offset >= visibility.compressed_pvs.size()) {
        return std::nullopt;
    }

    std::vector<bool> visible_leaves(bit_count, false);
    std::size_t pvs_offset = offset;
    std::size_t leaf_bit = 0;
    while (leaf_bit < bit_count) {
        if (pvs_offset >= visibility.compressed_pvs.size()) {
            return std::nullopt;
        }

        const auto value =
            std::to_integer<std::uint8_t>(visibility.compressed_pvs[pvs_offset]);
        ++pvs_offset;
        if (value == 0) {
            if (pvs_offset >= visibility.compressed_pvs.size()) {
                return std::nullopt;
            }
            const auto zero_run = std::to_integer<std::uint8_t>(
                visibility.compressed_pvs[pvs_offset]);
            ++pvs_offset;
            if (zero_run == 0) {
                return std::nullopt;
            }
            leaf_bit = std::min(
                bit_count, leaf_bit + static_cast<std::size_t>(zero_run) * 8U);
            continue;
        }

        for (std::size_t bit = 0; bit < 8 && leaf_bit < bit_count;
             ++bit, ++leaf_bit) {
            visible_leaves[leaf_bit] = ((value >> bit) & 0x1U) != 0;
        }
    }

    return visible_leaves;
}

} // namespace detail

/**
 * @brief Build a per-surface visibility mask from a classic BSP-style leaf PVS row.
 *
 * Returns an all-visible mask when visibility data is unavailable, the leaf
 * index is invalid, or the compressed PVS stream is malformed. PVS is
 * presentation-only and never gameplay authority.
 */
[[nodiscard]] inline std::vector<bool>
visible_level_surfaces_from_leaf(const LevelAsset &level,
                                 std::size_t leaf_index) {
    const LevelVisibilityAsset &visibility = level.visibility;
    std::vector<bool> visible_surfaces(level.geometry.surfaces.size(), true);
    if (!visibility.available || leaf_index >= visibility.leaves.size()) {
        return visible_surfaces;
    }

    const LevelLeaf &camera_leaf = visibility.leaves[leaf_index];
    if (!camera_leaf.compressed_pvs_offset.has_value() ||
        *camera_leaf.compressed_pvs_offset >= visibility.compressed_pvs.size()) {
        return visible_surfaces;
    }

    const std::size_t leaf_count =
        visibility.cluster_count == 0 ? visibility.leaves.size()
                                      : visibility.cluster_count;
    if (leaf_count == 0 || leaf_count > visibility.leaves.size()) {
        return visible_surfaces;
    }

    auto visible_leaves = detail::decompress_level_pvs_bits(
        visibility, *camera_leaf.compressed_pvs_offset, leaf_count);
    if (!visible_leaves.has_value()) {
        return visible_surfaces;
    }

    std::fill(visible_surfaces.begin(), visible_surfaces.end(), false);
    for (std::size_t visible_leaf_index = 0; visible_leaf_index < leaf_count;
         ++visible_leaf_index) {
        if (!(*visible_leaves)[visible_leaf_index]) {
            continue;
        }
        for (const std::size_t surface_index :
             visibility.leaves[visible_leaf_index].surface_indices) {
            if (surface_index < visible_surfaces.size()) {
                visible_surfaces[surface_index] = true;
            }
        }
    }

    return visible_surfaces;
}

} // namespace stellar::assets
