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
};

/**
 * @brief Optional visibility/lightmap metadata placeholder for static level imports.
 */
struct LevelVisibilityAsset {
    /** @brief True when source visibility data was imported. */
    bool available = false;
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
