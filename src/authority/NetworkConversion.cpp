#include "stellar/authority/NetworkConversion.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace stellar::authority {
namespace {

constexpr std::string_view kCollectPickupEvent = "gameplay.collect_pickup";
constexpr std::string_view kSetMeshEnabledEvent = "collision.set_mesh_enabled";
constexpr std::string_view kSetObjectColliderEnabledEvent = "object_collider.set_enabled";

[[nodiscard]] stellar::network::EntityKind convert_entity_kind(
    stellar::server::EntityKind kind) noexcept {
    switch (kind) {
        case stellar::server::EntityKind::kPlayer:
            return stellar::network::EntityKind::kPlayer;
        case stellar::server::EntityKind::kSprite:
            return stellar::network::EntityKind::kSprite;
        case stellar::server::EntityKind::kPickup:
            return stellar::network::EntityKind::kPickup;
        case stellar::server::EntityKind::kDoor:
            return stellar::network::EntityKind::kDoor;
        case stellar::server::EntityKind::kTrigger:
            return stellar::network::EntityKind::kTrigger;
        case stellar::server::EntityKind::kObjectCollider:
            return stellar::network::EntityKind::kObjectCollider;
    }
    return stellar::network::EntityKind::kSprite;
}

[[nodiscard]] stellar::network::TransformComponent convert_transform(
    const stellar::server::TransformComponent& transform) {
    return stellar::network::TransformComponent{.position = transform.position,
                                                .rotation = transform.rotation,
                                                .scale = transform.scale};
}

[[nodiscard]] stellar::network::GameplayEntityMetadata convert_metadata(
    const stellar::server::GameplayEntityMetadata& metadata) {
    return stellar::network::GameplayEntityMetadata{
        .name = metadata.name,
        .archetype = metadata.archetype,
        .sprite_id = metadata.sprite_id,
        .source_type = metadata.source_type,
        .extras_json = metadata.extras_json,
        .size = metadata.size,
        .alpha = metadata.alpha,
        .object_collider_id = metadata.object_collider_id,
        .collision_mesh_index = metadata.collision_mesh_index};
}

[[nodiscard]] stellar::network::PlayerSnapshot convert_player(
    const stellar::server::PlayerSnapshot& player) {
    return stellar::network::PlayerSnapshot{.player_id = player.player_id,
                                            .position = player.position,
                                            .velocity = player.velocity,
                                            .rotation = player.rotation,
                                            .grounded = player.grounded};
}

[[nodiscard]] std::uint32_t read_uint32_field(
    const stellar::scripting::ScriptOutputEvent& event,
    std::string_view name) noexcept {
    for (const stellar::scripting::ScriptField& field : event.fields) {
        if (field.key == name && field.type == stellar::scripting::ScriptValueType::kNumber &&
            field.number_value >= 0.0 && field.number_value <= 4294967295.0) {
            return static_cast<std::uint32_t>(field.number_value);
        }
    }
    return 0;
}

[[nodiscard]] std::string read_string_field(const stellar::scripting::ScriptOutputEvent& event,
                                            std::string_view name) {
    for (const stellar::scripting::ScriptField& field : event.fields) {
        if (field.key == name && field.type == stellar::scripting::ScriptValueType::kString) {
            return field.string_value;
        }
    }
    return {};
}

[[nodiscard]] bool read_bool_field(const stellar::scripting::ScriptOutputEvent& event,
                                   std::string_view name,
                                   bool fallback) noexcept {
    for (const stellar::scripting::ScriptField& field : event.fields) {
        if (field.key == name && field.type == stellar::scripting::ScriptValueType::kBoolean) {
            return field.bool_value;
        }
    }
    return fallback;
}

} // namespace

stellar::network::NetworkGameplayEntity make_network_entity(
    const stellar::server::GameplayEntity& entity) {
    return stellar::network::NetworkGameplayEntity{.id = entity.id,
                                                   .kind = convert_entity_kind(entity.kind),
                                                   .transform = convert_transform(entity.transform),
                                                   .metadata = convert_metadata(entity.metadata),
                                                   .active = entity.active,
                                                   .open = entity.open};
}

stellar::network::NetworkWorldSnapshot make_network_snapshot(
    const stellar::server::WorldSnapshot& snapshot) {
    stellar::network::NetworkWorldSnapshot network_snapshot{};
    network_snapshot.tick = snapshot.tick;
    network_snapshot.players.reserve(snapshot.players.size());
    for (const stellar::server::PlayerSnapshot& player : snapshot.players) {
        network_snapshot.players.push_back(convert_player(player));
    }
    std::sort(network_snapshot.players.begin(), network_snapshot.players.end(),
              [](const stellar::network::PlayerSnapshot& lhs,
                 const stellar::network::PlayerSnapshot& rhs) {
                  return lhs.player_id < rhs.player_id;
              });

    network_snapshot.entities.reserve(snapshot.gameplay_world.entities.size());
    for (const stellar::server::GameplayEntity& entity : snapshot.gameplay_world.entities) {
        network_snapshot.entities.push_back(make_network_entity(entity));
    }
    std::sort(network_snapshot.entities.begin(), network_snapshot.entities.end(),
              [](const stellar::network::NetworkGameplayEntity& lhs,
                 const stellar::network::NetworkGameplayEntity& rhs) {
                  return lhs.id < rhs.id;
              });
    return network_snapshot;
}

stellar::server::MovementCommand make_server_movement_command(
    const stellar::network::MovementCommand& movement) noexcept {
    return stellar::server::MovementCommand{.wish_direction = movement.wish_direction,
                                            .jump = movement.jump,
                                            .view_yaw_degrees = movement.view_yaw_degrees,
                                            .view_pitch_degrees = movement.view_pitch_degrees,
                                            .has_view_angles = movement.has_view_angles};
}

stellar::network::GameplayEvent make_script_error_event(
    std::uint64_t tick,
    const stellar::scripting::ScriptError& error) {
    return stellar::network::GameplayEvent{.kind = stellar::network::GameplayEventKind::kScriptError,
                                           .tick = tick,
                                           .entity_id = 0,
                                           .player_id = 0,
                                           .code = error.script_id + ":" + error.operation,
                                           .message = error.message};
}

std::vector<stellar::network::GameplayEvent> make_gameplay_events(
    std::uint64_t tick,
    const std::vector<stellar::scripting::ScriptOutputEvent>& script_events,
    const std::vector<stellar::scripting::ScriptCommandResult>& command_results,
    const std::vector<stellar::scripting::ScriptError>& script_errors) {
    std::vector<stellar::network::GameplayEvent> events;
    events.reserve(command_results.size() + script_errors.size());

    const std::size_t count = std::min(script_events.size(), command_results.size());
    for (std::size_t index = 0; index < count; ++index) {
        const stellar::scripting::ScriptOutputEvent& script_event = script_events[index];
        const stellar::scripting::ScriptCommandResult& result = command_results[index];
        if (!result.applied) {
            continue;
        }

        stellar::network::GameplayEvent event{};
        event.tick = tick;
        event.code = result.code.empty() ? result.event_name : result.code;
        event.message = result.message;
        if (script_event.name == kCollectPickupEvent) {
            event.kind = stellar::network::GameplayEventKind::kPickupCollected;
            event.entity_id = read_uint32_field(script_event, "id");
        } else if (script_event.name == kSetMeshEnabledEvent) {
            event.kind = stellar::network::GameplayEventKind::kDoorStateChanged;
            event.code = read_string_field(script_event, "mesh");
            if (event.code.empty()) {
                event.code = result.code;
            }
            const bool collision_enabled = read_bool_field(script_event, "enabled", true);
            event.message = collision_enabled ? "Door closed" : "Door opened";
        } else if (script_event.name == kSetObjectColliderEnabledEvent) {
            event.kind = stellar::network::GameplayEventKind::kScriptCommandApplied;
            event.entity_id = read_uint32_field(script_event, "id");
        } else {
            event.kind = stellar::network::GameplayEventKind::kScriptCommandApplied;
        }
        events.push_back(std::move(event));
    }

    for (const stellar::scripting::ScriptError& error : script_errors) {
        events.push_back(make_script_error_event(tick, error));
    }
    return events;
}

stellar::network::MapIdentity make_map_identity(const stellar::world::RuntimeWorld& world) {
    if (world.level_asset != nullptr) {
        return stellar::network::make_map_identity(world.level_asset->source_uri);
    }
    return stellar::network::make_map_identity("local");
}

} // namespace stellar::authority
