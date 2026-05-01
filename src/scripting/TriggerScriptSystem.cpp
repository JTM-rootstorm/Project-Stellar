#include "stellar/scripting/TriggerScriptSystem.hpp"

#include "stellar/assets/WorldMetadataAsset.hpp"

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

void append_drained_outputs(LuaRuntime& runtime, TriggerScriptResult& result) {
    std::vector<ScriptOutputEvent> events = runtime.drain_output_events();
    result.output_events.insert(result.output_events.end(), std::make_move_iterator(events.begin()),
                                std::make_move_iterator(events.end()));
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
    TriggerScriptResult result{};

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

                const char* function_name = callback_name(phase);
                if (!runtime.has_table(binding.table_name)) {
                    result.errors.push_back(ScriptError{binding.script_id, function_name,
                                                        "Lua script table is missing: " +
                                                            binding.table_name});
                    continue;
                }

                ScriptCallContext context = make_context(snapshot, event, phase);
                auto called = runtime.call_table_function(binding.script_id, binding.table_name,
                                                          function_name, context);
                if (!called.has_value()) {
                    result.errors.push_back(std::move(called.error()));
                }
                append_drained_outputs(runtime, result);
            }
        }
    }

    return result;
}

} // namespace stellar::scripting
