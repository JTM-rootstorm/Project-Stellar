#include "stellar/scripting/ScriptCommandProcessor.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace stellar::scripting {
namespace {

constexpr std::string_view kSetMeshEnabledEvent = "collision.set_mesh_enabled";
constexpr std::string_view kSetObjectColliderEnabledEvent = "object_collider.set_enabled";
constexpr std::string_view kCollectPickupEvent = "gameplay.collect_pickup";

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
                               .message = std::string{"Invalid field for "} + event.name + ": " +
                                           std::string{field_name}};
}

[[nodiscard]] bool read_uint32_field(const ScriptOutputEvent& event,
                                     std::string_view field_name,
                                     std::uint32_t& out_value) noexcept {
    const ScriptField* field = find_field(event, field_name);
    if (field == nullptr || field->type != ScriptValueType::kNumber ||
        !std::isfinite(field->number_value) || field->number_value < 0.0 ||
        field->number_value > static_cast<double>(std::numeric_limits<std::uint32_t>::max()) ||
        std::floor(field->number_value) != field->number_value) {
        return false;
    }
    out_value = static_cast<std::uint32_t>(field->number_value);
    return true;
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

[[nodiscard]] ScriptCommandResult apply_set_object_collider_enabled(
    stellar::server::WorldSession& session,
    const ScriptOutputEvent& event) noexcept {
    std::uint32_t collider_id = 0;
    if (!read_uint32_field(event, "id", collider_id)) {
        return invalid_field_result(event, "id");
    }

    const ScriptField* enabled = find_field(event, "enabled");
    if (enabled == nullptr || enabled->type != ScriptValueType::kBoolean) {
        return invalid_field_result(event, "enabled");
    }

    stellar::server::ObjectColliderMutationResult result =
        session.set_object_collider_enabled(collider_id, enabled->bool_value);
    return ScriptCommandResult{.event_name = event.name,
                               .applied = result.applied,
                               .code = std::move(result.code),
                               .message = std::move(result.message),
                               .object_collider_events =
                                   std::move(result.object_collider_events)};
}

[[nodiscard]] ScriptCommandResult apply_collect_pickup(
    stellar::server::WorldSession& session,
    const ScriptOutputEvent& event) noexcept {
    std::uint32_t collider_id = 0;
    if (!read_uint32_field(event, "id", collider_id)) {
        return invalid_field_result(event, "id");
    }

    stellar::server::PickupCollectionResult result = session.collect_pickup(collider_id);
    return ScriptCommandResult{.event_name = event.name,
                               .applied = result.applied,
                               .code = std::move(result.code),
                               .message = std::move(result.message),
                               .object_collider_events =
                                   std::move(result.object_collider_events)};
}

} // namespace

ScriptCommandApplication apply_script_commands(stellar::server::WorldSession& session,
                                               std::span<const ScriptOutputEvent> events) noexcept {
    ScriptCommandApplication application{};
    application.results.reserve(events.size());
    for (const ScriptOutputEvent& event : events) {
        if (event.name == kSetMeshEnabledEvent) {
            application.results.push_back(apply_set_mesh_enabled(session, event));
            continue;
        }
        if (event.name == kSetObjectColliderEnabledEvent) {
            application.results.push_back(apply_set_object_collider_enabled(session, event));
            continue;
        }
        if (event.name == kCollectPickupEvent) {
            application.results.push_back(apply_collect_pickup(session, event));
            continue;
        }
        application.results.push_back(unsupported_event_result(event));
    }
    return application;
}

} // namespace stellar::scripting
