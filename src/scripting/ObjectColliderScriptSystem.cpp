#include "stellar/scripting/ObjectColliderScriptSystem.hpp"

#include "stellar/assets/WorldMetadataAsset.hpp"
#include "stellar/scripting/ScriptHookDispatcher.hpp"

#include <cstddef>
#include <cstdint>
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
    std::vector<ScriptHookCall> calls;

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

                calls.push_back(ScriptHookCall{.script_id = binding.script_id,
                                               .table_name = binding.table_name,
                                               .function_name = callback_name(phase),
                                               .context = make_context(snapshot, event, phase)});
            }
        }
    }

    ScriptHookDispatchResult dispatch_result = dispatch_script_hooks(runtime, calls);
    return ObjectColliderScriptResult{.output_events = std::move(dispatch_result.output_events),
                                      .errors = std::move(dispatch_result.errors)};
}

} // namespace stellar::scripting
