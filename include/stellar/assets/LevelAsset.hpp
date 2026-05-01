#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
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

} // namespace stellar::assets
