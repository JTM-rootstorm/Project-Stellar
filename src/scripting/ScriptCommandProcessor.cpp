#include "stellar/scripting/ScriptCommandProcessor.hpp"

#include <string_view>

namespace stellar::scripting {
namespace {

constexpr std::string_view kSetMeshEnabledEvent = "collision.set_mesh_enabled";

[[nodiscard]] const ScriptField* find_field(const ScriptOutputEvent& event,
                                            std::string_view key) noexcept {
    for (const ScriptField& field : event.fields) {
        if (field.key == key) {
            return &field;
        }
    }
    return nullptr;
}

[[nodiscard]] ScriptCommandResult unsupported_event_result(const ScriptOutputEvent& event) {
    return ScriptCommandResult{.event_name = event.name,
                               .applied = false,
                               .code = "unsupported_event",
                               .message = "Unsupported script output event"};
}

[[nodiscard]] ScriptCommandResult invalid_field_result(const ScriptOutputEvent& event,
                                                       std::string_view field_name) {
    return ScriptCommandResult{.event_name = event.name,
                               .applied = false,
                               .code = "invalid_field",
                               .message = "Invalid field for collision.set_mesh_enabled: " +
                                          std::string{field_name}};
}

[[nodiscard]] ScriptCommandResult apply_set_mesh_enabled(stellar::server::WorldSession& session,
                                                         const ScriptOutputEvent& event) noexcept {
    const ScriptField* mesh = find_field(event, "mesh");
    if (mesh == nullptr || mesh->type != ScriptValueType::kString || mesh->string_value.empty()) {
        return invalid_field_result(event, "mesh");
    }

    const ScriptField* enabled = find_field(event, "enabled");
    if (enabled == nullptr || enabled->type != ScriptValueType::kBoolean) {
        return invalid_field_result(event, "enabled");
    }

    const stellar::world::RuntimeCollisionStateResult result =
        session.set_collision_mesh_enabled(mesh->string_value, enabled->bool_value);
    return ScriptCommandResult{.event_name = event.name,
                               .applied = result.applied,
                               .code = result.code,
                               .message = result.message};
}

} // namespace

ScriptCommandApplication apply_script_commands(stellar::server::WorldSession& session,
                                               std::span<const ScriptOutputEvent> events) noexcept {
    ScriptCommandApplication application{};
    application.results.reserve(events.size());
    for (const ScriptOutputEvent& event : events) {
        if (event.name != kSetMeshEnabledEvent) {
            application.results.push_back(unsupported_event_result(event));
            continue;
        }

        application.results.push_back(apply_set_mesh_enabled(session, event));
    }
    return application;
}

} // namespace stellar::scripting
