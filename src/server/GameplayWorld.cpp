#include "stellar/server/GameplayWorld.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <initializer_list>
#include <string>
#include <system_error>

namespace stellar::server {
namespace {

using stellar::assets::CollisionMesh;
using stellar::assets::WorldMarker;
using stellar::assets::WorldMarkerType;

[[nodiscard]] std::string lowercase(std::string_view value) {
    std::string result(value);
    std::ranges::transform(result, result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

[[nodiscard]] bool contains_token(std::string_view value, std::string_view token) {
    return lowercase(value).find(token) != std::string::npos;
}

[[nodiscard]] bool is_pickup_archetype(std::string_view archetype) {
    const std::string lowered = lowercase(archetype);
    return lowered == "pickup" || lowered == "item" || lowered.starts_with("pickup_") ||
           lowered.starts_with("item_") || lowered.find("pickup") != std::string::npos ||
           lowered.find("item") != std::string::npos;
}

[[nodiscard]] bool is_door_or_gate(std::string_view value) {
    const std::string lowered = lowercase(value);
    return lowered == "door" || lowered == "gate" || lowered == "func_door" ||
           lowered == "func_wall" || lowered.find("door") != std::string::npos ||
           lowered.find("gate") != std::string::npos;
}

[[nodiscard]] const std::string* property_value(const WorldMarker& marker,
                                                std::string_view key) noexcept {
    for (const auto& property : marker.properties) {
        if (property.key == key) {
            return &property.value;
        }
    }
    return nullptr;
}

[[nodiscard]] std::string first_property_value(const WorldMarker& marker,
                                               std::initializer_list<std::string_view> keys) {
    for (std::string_view key : keys) {
        if (const std::string* value = property_value(marker, key); value != nullptr) {
            return *value;
        }
    }
    return {};
}

[[nodiscard]] float parse_float_or(std::string_view value, float fallback) noexcept {
    float parsed = fallback;
    const auto* begin = value.data();
    const auto* end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end) {
        return fallback;
    }
    return parsed;
}

[[nodiscard]] float marker_alpha(const WorldMarker& marker) noexcept {
    if (const std::string* value = property_value(marker, "alpha"); value != nullptr) {
        return parse_float_or(*value, 1.0F);
    }
    if (const std::string* value = property_value(marker, "renderamt"); value != nullptr) {
        return parse_float_or(*value, 255.0F) / 255.0F;
    }
    return 1.0F;
}

[[nodiscard]] TransformComponent marker_transform(const WorldMarker& marker) {
    return {.position = marker.position, .rotation = marker.rotation, .scale = marker.scale};
}

[[nodiscard]] std::array<float, 3> mesh_center(const CollisionMesh& mesh) noexcept {
    return {(mesh.bounds_min[0] + mesh.bounds_max[0]) * 0.5F,
            (mesh.bounds_min[1] + mesh.bounds_max[1]) * 0.5F,
            (mesh.bounds_min[2] + mesh.bounds_max[2]) * 0.5F};
}

[[nodiscard]] std::array<float, 3> mesh_size(const CollisionMesh& mesh) noexcept {
    return {mesh.bounds_max[0] - mesh.bounds_min[0], mesh.bounds_max[1] - mesh.bounds_min[1],
            mesh.bounds_max[2] - mesh.bounds_min[2]};
}

[[nodiscard]] std::string marker_source_type(WorldMarkerType type) {
    switch (type) {
    case WorldMarkerType::kPlayerSpawn:
        return "player_spawn";
    case WorldMarkerType::kEntitySpawn:
        return "entity_spawn";
    case WorldMarkerType::kTrigger:
        return "trigger";
    case WorldMarkerType::kSprite:
        return "sprite";
    case WorldMarkerType::kPortal:
        return "portal";
    case WorldMarkerType::kObjectCollider:
        return "object_collider";
    }
    return "unknown";
}

[[nodiscard]] GameplayEntity marker_entity(EntityId id, EntityKind kind, const WorldMarker& marker,
                                           std::uint32_t object_collider_id = 0) {
    const std::string sprite_id = first_property_value(
        marker, {"sprite_id", "sprite", "texture", "texture_id", "material", "material_id"});
    return {.id = id,
            .kind = kind,
            .transform = marker_transform(marker),
            .metadata = {.name = marker.name,
                         .archetype = marker.archetype,
                         .sprite_id = sprite_id,
                         .source_type = marker_source_type(marker.type),
                         .extras_json = marker.extras_json,
                         .size = marker.scale,
                         .alpha = marker_alpha(marker),
                         .object_collider_id = object_collider_id}};
}

} // namespace

void GameplayWorld::reset_from_world(const stellar::world::RuntimeWorld& world,
                                     PlayerId local_player_id) {
    entities_.clear();
    player_bindings_.clear();
    next_entity_id_ = 1;

    if (const WorldMarker* spawn = stellar::world::find_player_spawn(world); spawn != nullptr) {
        const EntityId id = allocate_entity_id();
        entities_.push_back(marker_entity(id, EntityKind::kPlayer, *spawn));
        player_bindings_.push_back({.player_id = local_player_id, .entity_id = id});
    }

    for (std::size_t marker_index = 0; marker_index < world.world_metadata.markers.size();
         ++marker_index) {
        const WorldMarker& marker = world.world_metadata.markers[marker_index];
        if (marker.type == WorldMarkerType::kObjectCollider) {
            const EntityKind kind = is_pickup_archetype(marker.archetype) ? EntityKind::kPickup
                                                                          : EntityKind::kObjectCollider;
            entities_.push_back(marker_entity(allocate_entity_id(), kind, marker,
                                              static_cast<std::uint32_t>(marker_index + 1U)));
            continue;
        }

        if (marker.type == WorldMarkerType::kSprite) {
            entities_.push_back(marker_entity(allocate_entity_id(), EntityKind::kSprite, marker));
            continue;
        }

        if (marker.type == WorldMarkerType::kTrigger) {
            const EntityKind kind = (is_door_or_gate(marker.name) || is_door_or_gate(marker.archetype))
                                        ? EntityKind::kDoor
                                        : EntityKind::kTrigger;
            entities_.push_back(marker_entity(allocate_entity_id(), kind, marker));
            continue;
        }

        if (marker.type == WorldMarkerType::kEntitySpawn) {
            if (is_pickup_archetype(marker.archetype)) {
                entities_.push_back(marker_entity(allocate_entity_id(), EntityKind::kPickup, marker));
            } else if (is_door_or_gate(marker.name) || is_door_or_gate(marker.archetype)) {
                entities_.push_back(marker_entity(allocate_entity_id(), EntityKind::kDoor, marker));
            }
        }
    }

    if (!world.level_asset || !world.level_asset->level_collision.has_value()) {
        return;
    }

    const auto& meshes = world.level_asset->level_collision->meshes;
    for (std::size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
        const CollisionMesh& mesh = meshes[mesh_index];
        if (!(is_door_or_gate(mesh.name) || contains_token(mesh.name, "func_wall"))) {
            continue;
        }
        const EntityId id = allocate_entity_id();
        const auto size = mesh_size(mesh);
        entities_.push_back({.id = id,
                             .kind = EntityKind::kDoor,
                             .transform = {.position = mesh_center(mesh), .scale = size},
                             .metadata = {.name = mesh.name,
                                          .archetype = contains_token(mesh.name, "func_wall")
                                                           ? "func_wall"
                                                           : "door",
                                          .source_type = "collision_mesh",
                                          .size = size,
                                          .collision_mesh_index =
                                              static_cast<std::uint32_t>(mesh_index)}});
    }
}

std::span<const GameplayEntity> GameplayWorld::entities() const noexcept {
    return entities_;
}

std::span<const PlayerEntityBinding> GameplayWorld::player_bindings() const noexcept {
    return player_bindings_;
}

std::optional<EntityId> GameplayWorld::entity_for_player(PlayerId player_id) const noexcept {
    for (const PlayerEntityBinding& binding : player_bindings_) {
        if (binding.player_id == player_id) {
            return binding.entity_id;
        }
    }
    return std::nullopt;
}

const GameplayEntity* GameplayWorld::entity_for_object_collider(
    std::uint32_t collider_id) const noexcept {
    for (const GameplayEntity& entity : entities_) {
        if (entity.metadata.object_collider_id == collider_id) {
            return &entity;
        }
    }
    return nullptr;
}

bool GameplayWorld::is_active_pickup_collider(std::uint32_t collider_id) const noexcept {
    const GameplayEntity* entity = entity_for_object_collider(collider_id);
    return entity != nullptr && entity->kind == EntityKind::kPickup && entity->active;
}

bool GameplayWorld::deactivate_pickup_by_collider(std::uint32_t collider_id) noexcept {
    for (GameplayEntity& entity : entities_) {
        if (entity.metadata.object_collider_id == collider_id) {
            if (entity.kind != EntityKind::kPickup || !entity.active) {
                return false;
            }
            entity.active = false;
            return true;
        }
    }
    return false;
}

bool GameplayWorld::set_door_open_by_collision_mesh_name(std::string_view mesh_name,
                                                         bool open) noexcept {
    bool matched = false;
    for (GameplayEntity& entity : entities_) {
        if (entity.kind == EntityKind::kDoor && entity.metadata.name == mesh_name) {
            entity.open = open;
            entity.active = !open;
            matched = true;
        }
    }
    return matched;
}

GameplayWorldSnapshot GameplayWorld::snapshot() const {
    return {.entities = entities_, .player_bindings = player_bindings_};
}

EntityId GameplayWorld::allocate_entity_id() noexcept {
    const EntityId id = next_entity_id_;
    ++next_entity_id_;
    return id;
}

GameplayWorld build_gameplay_world(const stellar::world::RuntimeWorld& world,
                                   PlayerId local_player_id) {
    GameplayWorld gameplay_world;
    gameplay_world.reset_from_world(world, local_player_id);
    return gameplay_world;
}

const GameplayEntity* find_entity(std::span<const GameplayEntity> entities, EntityId id) noexcept {
    for (const GameplayEntity& entity : entities) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

} // namespace stellar::server
