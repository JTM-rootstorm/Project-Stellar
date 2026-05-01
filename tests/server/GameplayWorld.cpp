#include "stellar/server/GameplayWorld.hpp"
#include "stellar/server/WorldSession.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <utility>

namespace {

using Vec3 = std::array<float, 3>;

stellar::assets::WorldMarker marker(stellar::assets::WorldMarkerType type,
                                    std::string name,
                                    std::string archetype,
                                    Vec3 position) {
    stellar::assets::WorldMarker result{};
    result.type = type;
    result.name = std::move(name);
    result.archetype = std::move(archetype);
    result.position = position;
    result.scale = {2.0F, 3.0F, 4.0F};
    return result;
}

stellar::assets::LevelAsset scene_with_markers(
    std::initializer_list<stellar::assets::WorldMarker> markers) {
    stellar::assets::LevelAsset scene;
    scene.world_metadata.markers.assign(markers.begin(), markers.end());
    return scene;
}

stellar::assets::CollisionMesh door_mesh(std::string name, Vec3 min, Vec3 max) {
    stellar::assets::CollisionMesh mesh{};
    mesh.name = std::move(name);
    mesh.bounds_min = min;
    mesh.bounds_max = max;
    return mesh;
}

const stellar::server::GameplayEntity& entity_at(
    const stellar::server::GameplayWorldSnapshot& snapshot,
    std::size_t index) {
    assert(index < snapshot.entities.size());
    return snapshot.entities[index];
}

void spawns_player_sprite_pickup_and_door_entities_deterministically() {
    auto sprite = marker(stellar::assets::WorldMarkerType::kSprite, "TorchSprite", "torch",
                         {10.0F, 20.0F, 30.0F});
    sprite.properties.push_back({.key = "sprite_id", .value = "sprites/torch"});
    sprite.properties.push_back({.key = "alpha", .value = "0.5"});
    auto pickup = marker(stellar::assets::WorldMarkerType::kObjectCollider, "AmmoBox", "pickup_ammo",
                         {4.0F, 5.0F, 6.0F});
    auto door = marker(stellar::assets::WorldMarkerType::kTrigger, "GateTrigger", "gate",
                       {7.0F, 8.0F, 9.0F});
    const auto scene = scene_with_markers(
        {marker(stellar::assets::WorldMarkerType::kPlayerSpawn, "PlayerStart", "player",
                {1.0F, 2.0F, 3.0F}),
         sprite, pickup, door});
    const auto world = stellar::world::build_runtime_world(scene);

    const auto gameplay_world = stellar::server::build_gameplay_world(world, 42);
    const auto snapshot = gameplay_world.snapshot();

    assert(snapshot.entities.size() == 4);
    assert(snapshot.player_bindings.size() == 1);
    assert(snapshot.player_bindings[0].player_id == 42);
    assert(snapshot.player_bindings[0].entity_id == 1);
    assert(gameplay_world.entity_for_player(42).value() == 1);

    assert(entity_at(snapshot, 0).id == 1);
    assert(entity_at(snapshot, 0).kind == stellar::server::EntityKind::kPlayer);
    assert(entity_at(snapshot, 0).metadata.name == "PlayerStart");
    assert(entity_at(snapshot, 0).transform.position == (Vec3{1.0F, 2.0F, 3.0F}));

    assert(entity_at(snapshot, 1).id == 2);
    assert(entity_at(snapshot, 1).kind == stellar::server::EntityKind::kSprite);
    assert(entity_at(snapshot, 1).metadata.sprite_id == "sprites/torch");
    assert(entity_at(snapshot, 1).metadata.alpha == 0.5F);
    assert(entity_at(snapshot, 1).metadata.size == (Vec3{2.0F, 3.0F, 4.0F}));

    assert(entity_at(snapshot, 2).id == 3);
    assert(entity_at(snapshot, 2).kind == stellar::server::EntityKind::kPickup);
    assert(entity_at(snapshot, 2).metadata.object_collider_id == 3);
    assert(entity_at(snapshot, 2).active);

    assert(entity_at(snapshot, 3).id == 4);
    assert(entity_at(snapshot, 3).kind == stellar::server::EntityKind::kDoor);
    assert(entity_at(snapshot, 3).metadata.source_type == "trigger");
    assert(!entity_at(snapshot, 3).open);
}

void spawns_pickup_from_info_spawn_and_doors_from_collision_meshes() {
    auto spawn = marker(stellar::assets::WorldMarkerType::kEntitySpawn, "info_stellar_spawn",
                        "item_health", {3.0F, 4.0F, 5.0F});
    stellar::assets::LevelAsset scene = scene_with_markers({spawn});
    scene.level_collision = stellar::assets::LevelCollisionAsset{
        .meshes = {door_mesh("func_door_secret", {-1.0F, 0.0F, -2.0F}, {3.0F, 8.0F, 2.0F}),
                   door_mesh("solid_wall", {0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F})}};
    const auto world = stellar::world::build_runtime_world(scene);

    const auto snapshot = stellar::server::build_gameplay_world(world, 7).snapshot();

    assert(snapshot.player_bindings.empty());
    assert(snapshot.entities.size() == 2);
    assert(entity_at(snapshot, 0).id == 1);
    assert(entity_at(snapshot, 0).kind == stellar::server::EntityKind::kPickup);
    assert(entity_at(snapshot, 0).metadata.name == "info_stellar_spawn");
    assert(entity_at(snapshot, 1).id == 2);
    assert(entity_at(snapshot, 1).kind == stellar::server::EntityKind::kDoor);
    assert(entity_at(snapshot, 1).metadata.source_type == "collision_mesh");
    assert(entity_at(snapshot, 1).metadata.collision_mesh_index == 0);
    assert(entity_at(snapshot, 1).transform.position == (Vec3{1.0F, 4.0F, 0.0F}));
    assert(entity_at(snapshot, 1).metadata.size == (Vec3{4.0F, 8.0F, 4.0F}));
}

void world_session_exposes_gameplay_snapshot_without_changing_player_snapshot() {
    const auto scene = scene_with_markers(
        {marker(stellar::assets::WorldMarkerType::kPlayerSpawn, "PlayerStart", "player",
                {11.0F, 12.0F, 13.0F}),
         marker(stellar::assets::WorldMarkerType::kSprite, "Lamp", "lamp", {1.0F, 2.0F, 3.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSessionConfig config{};
    config.local_player_id = 99;
    const stellar::server::WorldSession session(world, config);

    const auto snapshot = session.snapshot();

    assert(snapshot.players.size() == 1);
    assert(snapshot.players[0].player_id == 99);
    assert(snapshot.players[0].position == (Vec3{11.0F, 12.0F, 13.0F}));
    assert(snapshot.gameplay_world.entities.size() == 2);
    assert(snapshot.gameplay_world.player_bindings[0].entity_id == 1);
    assert(session.entity_for_player(99).value() == 1);
    assert(session.gameplay_snapshot().entities[1].kind == stellar::server::EntityKind::kSprite);
}

} // namespace

int main() {
    spawns_player_sprite_pickup_and_door_entities_deterministically();
    spawns_pickup_from_info_spawn_and_doors_from_collision_meshes();
    world_session_exposes_gameplay_snapshot_without_changing_player_snapshot();
    return 0;
}
