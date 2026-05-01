#include "stellar/scripting/TriggerScriptSystem.hpp"

#include "stellar/assets/WorldMetadataAsset.hpp"
#include "stellar/scripting/ScriptHookDispatcher.hpp"

#include <utility>

namespace stellar::scripting {
namespace {

[[nodiscard]] const char* phase_name(TriggerScriptPhase phase) noexcept {
    switch (phase) {
    case TriggerScriptPhase::kEnter:
        return "enter";
    case TriggerScriptPhase::kStay:
        return "stay";
    case TriggerScriptPhase::kExit:
        return "exit";
    }
    return "unknown";
}

[[nodiscard]] const char* callback_name(TriggerScriptPhase phase) noexcept {
    switch (phase) {
    case TriggerScriptPhase::kEnter:
        return "on_trigger_enter";
    case TriggerScriptPhase::kStay:
        return "on_trigger_stay";
    case TriggerScriptPhase::kExit:
        return "on_trigger_exit";
    }
    return "on_trigger_unknown";
}

[[nodiscard]] ScriptField string_field(std::string key, std::string value) {
    ScriptField field{};
    field.key = std::move(key);
    field.type = ScriptValueType::kString;
    field.string_value = std::move(value);
    return field;
}

[[nodiscard]] ScriptField number_field(std::string key, double value) {
    ScriptField field{};
    field.key = std::move(key);
    field.type = ScriptValueType::kNumber;
    field.number_value = value;
    return field;
}

[[nodiscard]] ScriptCallContext make_context(const stellar::server::WorldSnapshot& snapshot,
                                             const stellar::server::MovementTriggerEvent& event,
                                             TriggerScriptPhase phase) {
    ScriptCallContext context{};
    context.tick = snapshot.tick;
    context.source_name = event.trigger_name;
    context.fields.push_back(string_field("trigger_name", event.trigger_name));
    context.fields.push_back(string_field("source", event.trigger_name));
    context.fields.push_back(string_field("phase", phase_name(phase)));

    // Phase 10C mirrors WorldSession's current one-player authoritative session shape. When
    // multiplayer snapshots are introduced, the event data should carry the owning player id.
    if (!snapshot.players.empty()) {
        const stellar::server::PlayerSnapshot& player = snapshot.players.front();
        context.fields.push_back(number_field("player_id", static_cast<double>(player.player_id)));
        context.fields.push_back(number_field("player_position_x", player.position[0]));
        context.fields.push_back(number_field("player_position_y", player.position[1]));
        context.fields.push_back(number_field("player_position_z", player.position[2]));
    }
    return context;
}

} // namespace

TriggerScriptSystem::TriggerScriptSystem(const stellar::world::RuntimeWorld& world) {
    for (const stellar::assets::WorldMarker& marker : world.world_metadata.markers) {
        if (marker.type != stellar::assets::WorldMarkerType::kTrigger || !marker.script.has_value()) {
            continue;
        }

        bindings_.push_back(Binding{marker.name, marker.script->script_id, marker.script->table_name});
    }
}

TriggerScriptResult TriggerScriptSystem::process_trigger_events(
    LuaRuntime& runtime,
    const stellar::server::WorldSnapshot& snapshot) noexcept {
    std::vector<ScriptHookCall> calls;

    for (const stellar::server::MovementTriggerEvent& event : snapshot.trigger_events) {
        for (const Binding& binding : bindings_) {
            if (binding.trigger_name != event.trigger_name) {
                continue;
            }

            const TriggerScriptPhase phases[] = {TriggerScriptPhase::kEnter, TriggerScriptPhase::kStay,
                                                 TriggerScriptPhase::kExit};
            for (TriggerScriptPhase phase : phases) {
                if ((phase == TriggerScriptPhase::kEnter && !event.entered) ||
                    (phase == TriggerScriptPhase::kStay && !event.stayed) ||
                    (phase == TriggerScriptPhase::kExit && !event.exited)) {
                    continue;
                }

                calls.push_back(ScriptHookCall{.script_id = binding.script_id,
                                               .table_name = binding.table_name,
                                               .function_name = callback_name(phase),
                                               .context = make_context(snapshot, event, phase)});
            }
        }
    }

    ScriptHookDispatchResult dispatch_result = dispatch_script_hooks(runtime, calls);
    return TriggerScriptResult{.output_events = std::move(dispatch_result.output_events),
                               .errors = std::move(dispatch_result.errors)};
}

} // namespace stellar::scripting
