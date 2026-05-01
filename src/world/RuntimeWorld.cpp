#include "stellar/world/RuntimeWorld.hpp"

#include <utility>

namespace stellar::world {
namespace {

std::size_t count_collision_triangles(const stellar::assets::LevelCollisionAsset& collision) {
    std::size_t total = 0;
    for (const auto& mesh : collision.meshes) {
        total += mesh.triangles.size();
    }
    return total;
}

bool has_marker_type(const stellar::assets::WorldMetadataAsset& metadata,
                     stellar::assets::WorldMarkerType type) noexcept {
    for (const auto& marker : metadata.markers) {
        if (marker.type == type) {
            return true;
        }
    }
    return false;
}

std::size_t count_marker_type(const stellar::assets::WorldMetadataAsset& metadata,
                              stellar::assets::WorldMarkerType type) noexcept {
    std::size_t total = 0;
    for (const auto& marker : metadata.markers) {
        if (marker.type == type) {
            ++total;
        }
    }
    return total;
}

} // namespace

RuntimeWorld build_runtime_world(const stellar::assets::SceneAsset& scene) {
    RuntimeWorld world;
    world.scene_asset = &scene;
    world.world_metadata = scene.world_metadata;

    if (scene.level_collision.has_value()) {
        world.diagnostics.collision_mesh_count = scene.level_collision->meshes.size();
        world.diagnostics.collision_triangle_count = count_collision_triangles(*scene.level_collision);

        if (!scene.level_collision->meshes.empty()) {
            world.collision_world.emplace(*scene.level_collision);
            world.diagnostics.has_collision = true;
        } else {
            world.diagnostics.warnings.push_back(
                "Scene has a level collision asset with no collision meshes");
        }
    }

    world.diagnostics.marker_count = world.world_metadata.markers.size();
    world.diagnostics.sprite_marker_count =
        count_marker_type(world.world_metadata, stellar::assets::WorldMarkerType::kSprite);
    world.diagnostics.has_player_spawn =
        has_marker_type(world.world_metadata, stellar::assets::WorldMarkerType::kPlayerSpawn);

    if (!world.diagnostics.has_player_spawn) {
        world.diagnostics.warnings.push_back("Scene has no player spawn marker");
    }

    return world;
}

const stellar::assets::WorldMarker* find_player_spawn(const RuntimeWorld& world) noexcept {
    for (const auto& marker : world.world_metadata.markers) {
        if (marker.type == stellar::assets::WorldMarkerType::kPlayerSpawn) {
            return &marker;
        }
    }
    return nullptr;
}

std::vector<const stellar::assets::WorldMarker*> find_markers_by_type(
    const RuntimeWorld& world,
    stellar::assets::WorldMarkerType type) {
    std::vector<const stellar::assets::WorldMarker*> markers;
    for (const auto& marker : world.world_metadata.markers) {
        if (marker.type == type) {
            markers.push_back(&marker);
        }
    }
    return markers;
}

const stellar::assets::WorldMarker* find_spawn_by_archetype(const RuntimeWorld& world,
                                                            std::string_view archetype) noexcept {
    for (const auto& marker : world.world_metadata.markers) {
        if (marker.type == stellar::assets::WorldMarkerType::kEntitySpawn &&
            marker.archetype == archetype) {
            return &marker;
        }
    }
    return nullptr;
}

std::vector<const stellar::assets::WorldMarker*> find_sprite_markers(const RuntimeWorld& world) {
    return find_markers_by_type(world, stellar::assets::WorldMarkerType::kSprite);
}

std::vector<const stellar::assets::WorldMarker*> find_trigger_markers(const RuntimeWorld& world) {
    return find_markers_by_type(world, stellar::assets::WorldMarkerType::kTrigger);
}

} // namespace stellar::world
