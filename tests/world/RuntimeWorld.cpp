#include "stellar/world/RuntimeWorld.hpp"

#include <cassert>
#include <string>

namespace {

stellar::assets::CollisionTriangle make_triangle() {
    return stellar::assets::CollisionTriangle{.a = {0.0F, 0.0F, 0.0F},
                                              .b = {1.0F, 0.0F, 0.0F},
                                              .c = {0.0F, 1.0F, 0.0F},
                                              .normal = {0.0F, 0.0F, 1.0F}};
}

stellar::assets::WorldMarker make_marker(stellar::assets::WorldMarkerType type,
                                         std::string name,
                                         std::string archetype = {}) {
    stellar::assets::WorldMarker marker;
    marker.type = type;
    marker.name = std::move(name);
    marker.archetype = std::move(archetype);
    return marker;
}

void verify_empty_scene_builds_empty_runtime_world() {
    const stellar::assets::SceneAsset scene;
    const auto world = stellar::world::build_runtime_world(scene);

    assert(world.scene_asset == &scene);
    assert(!world.collision_world.has_value());
    assert(!world.diagnostics.has_collision);
    assert(world.diagnostics.collision_mesh_count == 0);
    assert(world.diagnostics.collision_triangle_count == 0);
    assert(world.diagnostics.marker_count == 0);
    assert(world.diagnostics.sprite_marker_count == 0);
    assert(!world.diagnostics.has_player_spawn);
    assert(stellar::world::find_player_spawn(world) == nullptr);
    assert(!world.diagnostics.warnings.empty());
}

void verify_scene_without_collision_leaves_collision_world_empty() {
    stellar::assets::SceneAsset scene;
    scene.world_metadata.markers.push_back(
        make_marker(stellar::assets::WorldMarkerType::kPlayerSpawn, "Player"));

    const auto world = stellar::world::build_runtime_world(scene);

    assert(!world.collision_world.has_value());
    assert(!world.diagnostics.has_collision);
    assert(world.diagnostics.has_player_spawn);
    assert(world.diagnostics.warnings.empty());
}

void verify_scene_with_collision_creates_collision_world() {
    stellar::assets::SceneAsset scene;
    stellar::assets::CollisionMesh mesh;
    mesh.name = "COL_floor";
    mesh.triangles = {make_triangle(), make_triangle()};
    scene.level_collision = stellar::assets::LevelCollisionAsset{.meshes = {mesh}};
    scene.world_metadata.markers.push_back(
        make_marker(stellar::assets::WorldMarkerType::kPlayerSpawn, "Player"));

    const auto world = stellar::world::build_runtime_world(scene);

    assert(world.collision_world.has_value());
    assert(world.diagnostics.has_collision);
    assert(world.diagnostics.collision_mesh_count == 1);
    assert(world.diagnostics.collision_triangle_count == 2);
    assert(&world.collision_world->asset() == &scene.level_collision.value());
}

void verify_empty_collision_asset_does_not_create_collision_world() {
    stellar::assets::SceneAsset scene;
    scene.level_collision = stellar::assets::LevelCollisionAsset{};
    scene.world_metadata.markers.push_back(
        make_marker(stellar::assets::WorldMarkerType::kPlayerSpawn, "Player"));

    const auto world = stellar::world::build_runtime_world(scene);

    assert(!world.collision_world.has_value());
    assert(!world.diagnostics.has_collision);
    assert(world.diagnostics.collision_mesh_count == 0);
    assert(world.diagnostics.collision_triangle_count == 0);
    assert(world.diagnostics.warnings.size() == 1);
}

void verify_marker_helpers_and_diagnostics() {
    stellar::assets::SceneAsset scene;
    scene.world_metadata.markers = {
        make_marker(stellar::assets::WorldMarkerType::kPlayerSpawn, "Player"),
        make_marker(stellar::assets::WorldMarkerType::kEntitySpawn, "EnemyA", "Imp"),
        make_marker(stellar::assets::WorldMarkerType::kEntitySpawn, "EnemyB", "Ogre"),
        make_marker(stellar::assets::WorldMarkerType::kTrigger, "DoorOpen"),
        make_marker(stellar::assets::WorldMarkerType::kTrigger, "Checkpoint"),
        make_marker(stellar::assets::WorldMarkerType::kSprite, "Torch"),
        make_marker(stellar::assets::WorldMarkerType::kSprite, "Banner"),
    };

    const auto world = stellar::world::build_runtime_world(scene);

    const auto* player = stellar::world::find_player_spawn(world);
    assert(player != nullptr);
    assert(player->name == "Player");

    const auto* imp = stellar::world::find_spawn_by_archetype(world, "Imp");
    assert(imp != nullptr);
    assert(imp->name == "EnemyA");
    assert(stellar::world::find_spawn_by_archetype(world, "Missing") == nullptr);

    const auto triggers = stellar::world::find_trigger_markers(world);
    assert(triggers.size() == 2);
    assert(triggers[0]->name == "DoorOpen");
    assert(triggers[1]->name == "Checkpoint");

    const auto sprites = stellar::world::find_sprite_markers(world);
    assert(sprites.size() == 2);
    assert(sprites[0]->name == "Torch");
    assert(sprites[1]->name == "Banner");

    const auto portals = stellar::world::find_markers_by_type(
        world, stellar::assets::WorldMarkerType::kPortal);
    assert(portals.empty());

    assert(world.diagnostics.marker_count == 7);
    assert(world.diagnostics.sprite_marker_count == 2);
    assert(world.diagnostics.has_player_spawn);
}

void verify_metadata_is_copied_for_stable_lookup() {
    stellar::assets::SceneAsset scene;
    scene.world_metadata.markers.push_back(
        make_marker(stellar::assets::WorldMarkerType::kPlayerSpawn, "OriginalPlayer"));

    const auto world = stellar::world::build_runtime_world(scene);
    scene.world_metadata.markers[0].name = "MutatedPlayer";

    const auto* player = stellar::world::find_player_spawn(world);
    assert(player != nullptr);
    assert(player->name == "OriginalPlayer");
}

} // namespace

int main() {
    verify_empty_scene_builds_empty_runtime_world();
    verify_scene_without_collision_leaves_collision_world_empty();
    verify_scene_with_collision_creates_collision_world();
    verify_empty_collision_asset_does_not_create_collision_world();
    verify_marker_helpers_and_diagnostics();
    verify_metadata_is_copied_for_stable_lookup();
    return 0;
}
