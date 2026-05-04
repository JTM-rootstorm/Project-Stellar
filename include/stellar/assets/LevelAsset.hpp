#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "stellar/assets/CollisionAsset.hpp"
#include "stellar/assets/ImageAsset.hpp"
#include "stellar/assets/MaterialAsset.hpp"
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

    /** @brief Optional import-resolved material overriding default BSP surface presentation. */
    std::optional<MaterialAsset> resolved_material;
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
 * @brief Level-wide lighting policy used by importers and renderers.
 */
enum class LevelLightingMode {
    /** @brief Missing baked lightmaps should render as visible fullbright geometry. */
    kFullbright,

    /** @brief Missing baked lightmaps should render as black/unlit geometry. */
    kBakedRequired,
};

/**
 * @brief Explicit authored level-wide ambient/global lighting contribution.
 */
struct LevelGlobalLight {
    /** @brief Linear RGB color channels normalized to the 0-1 range. */
    std::array<float, 3> color{0.0F, 0.0F, 0.0F};

    /** @brief Scalar ambient intensity multiplier. */
    float intensity = 0.0F;

    /** @brief True when the authored global light should contribute to the level. */
    bool enabled = false;
};

/**
 * @brief Source-neutral lighting settings for a playable level.
 */
struct LevelLightingAsset {
    /** @brief Imported or inferred baked-lighting policy. */
    LevelLightingMode mode = LevelLightingMode::kFullbright;

    /** @brief Explicit authored ambient/global fill; absence remains disabled/black. */
    LevelGlobalLight global_ambient{};

    /** @brief Renderer multiplier applied to baked lightmap texels. */
    float lightmap_intensity = 1.0F;

    /** @brief Renderer minimum baked-lightmap floor; zero preserves authored darkness. */
    float lightmap_min = 0.0F;
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

    /** @brief Level-wide lighting policy and authored ambient/global lighting. */
    LevelLightingAsset lighting;
};

} // namespace stellar::assets
