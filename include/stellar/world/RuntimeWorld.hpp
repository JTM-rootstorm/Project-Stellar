#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "stellar/assets/LevelAsset.hpp"
#include "stellar/assets/WorldMetadataAsset.hpp"
#include "stellar/physics/CollisionWorld.hpp"

namespace stellar::world {

/**
 * @brief Display-free summary of runtime world assembly results.
 */
struct RuntimeWorldDiagnostics {
    /** @brief True when a non-empty static collision world was created. */
    bool has_collision = false;

    /** @brief Number of source collision meshes available to the runtime world. */
    std::size_t collision_mesh_count = 0;

    /** @brief Number of source collision triangles available to the runtime world. */
    std::size_t collision_triangle_count = 0;

    /** @brief Total number of imported world metadata markers. */
    std::size_t marker_count = 0;

    /** @brief Number of imported sprite markers exposed for later render binding. */
    std::size_t sprite_marker_count = 0;

    /** @brief Number of imported object collider markers exposed for gameplay sensors. */
    std::size_t object_collider_marker_count = 0;

    /** @brief True when a player spawn marker is discoverable. */
    bool has_player_spawn = false;

    /** @brief Non-fatal assembly warnings for validation and tooling output. */
    std::vector<std::string> warnings;
};

/**
 * @brief Backend-neutral runtime world assembled from a source-neutral level asset.
 *
 * RuntimeWorld does not own the source LevelAsset. The caller must keep the LevelAsset alive for at
 * least as long as this object because CollisionWorld references LevelAsset::level_collision data.
 */
struct RuntimeWorld {
    /** @brief Source level asset backing geometry metadata and collision lifetime. */
    const stellar::assets::LevelAsset* level_asset = nullptr;

    /** @brief Optional static collision query world, created only for non-empty level collision. */
    std::optional<stellar::physics::CollisionWorld> collision_world;

    /** @brief Copy of imported metadata markers for pure runtime lookups. */
    stellar::assets::WorldMetadataAsset world_metadata;

    /** @brief Assembly diagnostics and non-fatal warnings. */
    RuntimeWorldDiagnostics diagnostics;
};

/**
 * @brief Assemble a backend-neutral runtime world from a source-neutral level asset.
 */
[[nodiscard]] RuntimeWorld build_runtime_world(const stellar::assets::LevelAsset& level);

/**
 * @brief Find the first player spawn marker in runtime world metadata.
 */
[[nodiscard]] const stellar::assets::WorldMarker* find_player_spawn(
    const RuntimeWorld& world) noexcept;

/**
 * @brief Find all markers of a metadata type in runtime world metadata.
 */
[[nodiscard]] std::vector<const stellar::assets::WorldMarker*> find_markers_by_type(
    const RuntimeWorld& world,
    stellar::assets::WorldMarkerType type);

/**
 * @brief Find the first entity spawn marker whose archetype matches the supplied name.
 */
[[nodiscard]] const stellar::assets::WorldMarker* find_spawn_by_archetype(
    const RuntimeWorld& world,
    std::string_view archetype) noexcept;

/**
 * @brief Find all sprite markers exposed for later texture/material binding.
 */
[[nodiscard]] std::vector<const stellar::assets::WorldMarker*> find_sprite_markers(
    const RuntimeWorld& world);

/**
 * @brief Find all trigger markers exposed for later gameplay hook binding.
 */
[[nodiscard]] std::vector<const stellar::assets::WorldMarker*> find_trigger_markers(
    const RuntimeWorld& world);

} // namespace stellar::world
