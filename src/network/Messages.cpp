#include "stellar/network/Messages.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace stellar::network {
namespace {

constexpr std::string_view kCollectPickupEvent = "gameplay.collect_pickup";
constexpr std::string_view kSetMeshEnabledEvent = "collision.set_mesh_enabled";
constexpr std::string_view kSetObjectColliderEnabledEvent = "object_collider.set_enabled";

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

NetworkGameplayEntity make_network_entity(const stellar::server::GameplayEntity& entity) {
    return NetworkGameplayEntity{.id = entity.id,
                                 .kind = entity.kind,
                                 .transform = entity.transform,
                                 .metadata = entity.metadata,
                                 .active = entity.active,
                                 .open = entity.open};
}

NetworkWorldSnapshot make_network_snapshot(const stellar::server::WorldSnapshot& snapshot) {
    NetworkWorldSnapshot network_snapshot{};
    network_snapshot.tick = snapshot.tick;
    network_snapshot.players = snapshot.players;
    std::sort(network_snapshot.players.begin(), network_snapshot.players.end(),
              [](const stellar::server::PlayerSnapshot& lhs,
                 const stellar::server::PlayerSnapshot& rhs) {
                  return lhs.player_id < rhs.player_id;
              });

    network_snapshot.entities.reserve(snapshot.gameplay_world.entities.size());
    for (const stellar::server::GameplayEntity& entity : snapshot.gameplay_world.entities) {
        network_snapshot.entities.push_back(make_network_entity(entity));
    }
    std::sort(network_snapshot.entities.begin(), network_snapshot.entities.end(),
              [](const NetworkGameplayEntity& lhs, const NetworkGameplayEntity& rhs) {
                  return lhs.id < rhs.id;
              });
    return network_snapshot;
}

GameplayEvent make_script_error_event(std::uint64_t tick,
                                       const stellar::scripting::ScriptError& error) {
    return GameplayEvent{.kind = GameplayEventKind::kScriptError,
                         .tick = tick,
                         .entity_id = 0,
                         .player_id = 0,
                         .code = error.script_id + ":" + error.operation,
                         .message = error.message};
}

std::vector<GameplayEvent> make_gameplay_events(
    std::uint64_t tick,
    const std::vector<stellar::scripting::ScriptOutputEvent>& script_events,
    const std::vector<stellar::scripting::ScriptCommandResult>& command_results,
    const std::vector<stellar::scripting::ScriptError>& script_errors) {
    std::vector<GameplayEvent> events;
    events.reserve(command_results.size() + script_errors.size());

    const std::size_t count = std::min(script_events.size(), command_results.size());
    for (std::size_t index = 0; index < count; ++index) {
        const stellar::scripting::ScriptOutputEvent& script_event = script_events[index];
        const stellar::scripting::ScriptCommandResult& result = command_results[index];
        if (!result.applied) {
            continue;
        }

        GameplayEvent event{};
        event.tick = tick;
        event.code = result.code.empty() ? result.event_name : result.code;
        event.message = result.message;
        if (script_event.name == kCollectPickupEvent) {
            event.kind = GameplayEventKind::kPickupCollected;
            event.entity_id = read_uint32_field(script_event, "id");
        } else if (script_event.name == kSetMeshEnabledEvent) {
            event.kind = GameplayEventKind::kDoorStateChanged;
            event.code = read_string_field(script_event, "mesh");
            if (event.code.empty()) {
                event.code = result.code;
            }
            const bool collision_enabled = read_bool_field(script_event, "enabled", true);
            event.message = collision_enabled ? "Door closed" : "Door opened";
        } else if (script_event.name == kSetObjectColliderEnabledEvent) {
            event.kind = GameplayEventKind::kScriptCommandApplied;
            event.entity_id = read_uint32_field(script_event, "id");
        } else {
            event.kind = GameplayEventKind::kScriptCommandApplied;
        }
        events.push_back(std::move(event));
    }

    for (const stellar::scripting::ScriptError& error : script_errors) {
        events.push_back(make_script_error_event(tick, error));
    }
    return events;
}

} // namespace stellar::network
