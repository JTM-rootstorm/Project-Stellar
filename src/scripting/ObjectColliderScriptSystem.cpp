#include "stellar/scripting/ObjectColliderScriptSystem.hpp"

#include "stellar/assets/WorldMetadataAsset.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>

namespace stellar::scripting {
namespace {

[[nodiscard]] const char* phase_name(ObjectColliderScriptPhase phase) noexcept {
    switch (phase) {
    case ObjectColliderScriptPhase::kEnter:
        return "enter";
    case ObjectColliderScriptPhase::kStay:
        return "stay";
    case ObjectColliderScriptPhase::kExit:
        return "exit";
    }
    return "unknown";
}

[[nodiscard]] const char* callback_name(ObjectColliderScriptPhase phase) noexcept {
    switch (phase) {
    case ObjectColliderScriptPhase::kEnter:
        return "on_object_collider_enter";
    case ObjectColliderScriptPhase::kStay:
        return "on_object_collider_stay";
    case ObjectColliderScriptPhase::kExit:
        return "on_object_collider_exit";
    }
    return "on_object_collider_unknown";
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
                                             const stellar::server::ObjectColliderEvent& event,
                                             ObjectColliderScriptPhase phase) {
    ScriptCallContext context{};
    context.tick = snapshot.tick;
    context.source_name = event.name;
    context.fields.push_back(number_field("tick", static_cast<double>(snapshot.tick)));
    context.fields.push_back(number_field("player_id", static_cast<double>(event.player_id)));
    context.fields.push_back(number_field("collider_id", static_cast<double>(event.collider_id)));
    context.fields.push_back(string_field("collider_name", event.name));
    context.fields.push_back(string_field("archetype", event.archetype));
    context.fields.push_back(string_field("phase", phase_name(phase)));
    return context;
}

void append_drained_outputs(LuaRuntime& runtime, ObjectColliderScriptResult& result) {
    std::vector<ScriptOutputEvent> events = runtime.drain_output_events();
    result.output_events.insert(result.output_events.end(), std::make_move_iterator(events.begin()),
                                std::make_move_iterator(events.end()));
}

} // namespace

ObjectColliderScriptSystem::ObjectColliderScriptSystem(const stellar::world::RuntimeWorld& world) {
    for (std::size_t marker_index = 0; marker_index < world.world_metadata.markers.size();
         ++marker_index) {
        const stellar::assets::WorldMarker& marker = world.world_metadata.markers[marker_index];
        if (marker.type != stellar::assets::WorldMarkerType::kObjectCollider ||
            !marker.script.has_value()) {
            continue;
        }

        bindings_.push_back(Binding{.collider_id = static_cast<std::uint32_t>(marker_index + 1U),
                                    .collider_name = marker.name,
                                    .script_id = marker.script->script_id,
                                    .table_name = marker.script->table_name});
    }
}

ObjectColliderScriptResult ObjectColliderScriptSystem::process_object_collider_events(
    LuaRuntime& runtime,
    const stellar::server::WorldSnapshot& snapshot) noexcept {
    ObjectColliderScriptResult result{};

    for (const stellar::server::ObjectColliderEvent& event : snapshot.object_collider_events) {
        for (const Binding& binding : bindings_) {
            if (binding.collider_id != event.collider_id) {
                continue;
            }

            const ObjectColliderScriptPhase phases[] = {ObjectColliderScriptPhase::kEnter,
                                                        ObjectColliderScriptPhase::kStay,
                                                        ObjectColliderScriptPhase::kExit};
            for (ObjectColliderScriptPhase phase : phases) {
                if ((phase == ObjectColliderScriptPhase::kEnter && !event.entered) ||
                    (phase == ObjectColliderScriptPhase::kStay && !event.stayed) ||
                    (phase == ObjectColliderScriptPhase::kExit && !event.exited)) {
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
