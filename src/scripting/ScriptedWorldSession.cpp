#include "stellar/scripting/ScriptedWorldSession.hpp"

#include "stellar/assets/WorldMetadataAsset.hpp"

#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace stellar::scripting {
namespace {

[[nodiscard]] bool contains_script_id(const std::vector<std::string>& script_ids,
                                      const std::string& script_id) noexcept {
    for (const std::string& existing : script_ids) {
        if (existing == script_id) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::vector<std::string> unique_script_ids(
    const stellar::world::RuntimeWorld& world) {
    std::vector<std::string> script_ids;
    for (const stellar::assets::WorldMarker& marker : world.world_metadata.markers) {
        if ((marker.type != stellar::assets::WorldMarkerType::kTrigger &&
             marker.type != stellar::assets::WorldMarkerType::kObjectCollider) ||
            !marker.script.has_value()) {
            continue;
        }
        if (!contains_script_id(script_ids, marker.script->script_id)) {
            script_ids.push_back(marker.script->script_id);
        }
    }
    return script_ids;
}

} // namespace

std::expected<ScriptedWorldSession, ScriptError> ScriptedWorldSession::create(
    const stellar::world::RuntimeWorld& world,
    stellar::server::WorldSessionConfig session_config,
    ScriptRegistry registry,
    LuaRuntimeConfig lua_config) {
    LuaRuntime runtime{lua_config};

    for (const std::string& script_id : unique_script_ids(world)) {
        const std::string* source = registry.find_script(script_id);
        if (source == nullptr) {
            return std::unexpected(ScriptError{script_id, "load_script",
                                               "Missing script source for id: " + script_id});
        }

        auto loaded = runtime.load_script(script_id, *source);
        if (!loaded.has_value()) {
            return std::unexpected(std::move(loaded.error()));
        }
    }

    return ScriptedWorldSession{world, session_config, std::move(registry), std::move(runtime)};
}

ScriptedWorldSession::ScriptedWorldSession(const stellar::world::RuntimeWorld& world,
                                           stellar::server::WorldSessionConfig session_config,
                                           ScriptRegistry registry,
                                           LuaRuntime runtime)
    : registry_(std::move(registry)),
      runtime_(std::move(runtime)),
      session_(world, session_config),
      trigger_scripts_(world),
      object_collider_scripts_(world),
      latest_snapshot_(session_.snapshot()) {}

ScriptedWorldFrame ScriptedWorldSession::tick(
    std::span<const stellar::server::PlayerCommand> commands) noexcept {
    latest_snapshot_ = session_.tick(commands);
    TriggerScriptResult trigger_result =
        trigger_scripts_.process_trigger_events(runtime_, latest_snapshot_);
    ObjectColliderScriptResult object_result =
        object_collider_scripts_.process_object_collider_events(runtime_, latest_snapshot_);

    std::vector<ScriptOutputEvent> output_events = std::move(trigger_result.output_events);
    output_events.insert(output_events.end(),
                         std::make_move_iterator(object_result.output_events.begin()),
                         std::make_move_iterator(object_result.output_events.end()));
    std::vector<ScriptError> errors = std::move(trigger_result.errors);
    errors.insert(errors.end(), std::make_move_iterator(object_result.errors.begin()),
                  std::make_move_iterator(object_result.errors.end()));

    ScriptCommandApplication command_application =
        apply_script_commands(session_, output_events);
    latest_snapshot_.gameplay_world = session_.gameplay_snapshot();

    return ScriptedWorldFrame{.snapshot = latest_snapshot_,
                                .script_events = std::move(output_events),
                                .script_errors = std::move(errors),
                                .command_results = std::move(command_application.results)};
}

const stellar::server::WorldSnapshot& ScriptedWorldSession::latest_snapshot() const noexcept {
    return latest_snapshot_;
}

stellar::server::WorldSnapshot ScriptedWorldSession::snapshot() const {
    return session_.snapshot();
}

void ScriptedWorldSession::set_object_colliders(
    std::span<const stellar::world::ObjectCollider> colliders) {
    session_.set_object_colliders(colliders);
    latest_snapshot_ = session_.snapshot();
}

std::vector<stellar::server::ObjectColliderEvent>
ScriptedWorldSession::replace_object_colliders_preserving_overlaps(
    std::span<const stellar::world::ObjectCollider> colliders) noexcept {
    const auto events = session_.replace_object_colliders_preserving_overlaps(colliders);
    latest_snapshot_ = session_.snapshot();
    return events;
}

stellar::server::ObjectColliderMutationResult ScriptedWorldSession::set_object_collider_enabled(
    std::uint32_t collider_id, bool enabled) noexcept {
    auto result = session_.set_object_collider_enabled(collider_id, enabled);
    latest_snapshot_ = session_.snapshot();
    return result;
}

stellar::server::ObjectColliderMutationResult ScriptedWorldSession::upsert_object_collider(
    const stellar::world::ObjectCollider& collider) noexcept {
    auto result = session_.upsert_object_collider(collider);
    latest_snapshot_ = session_.snapshot();
    return result;
}

stellar::server::ObjectColliderMutationResult ScriptedWorldSession::remove_object_collider(
    std::uint32_t collider_id) noexcept {
    auto result = session_.remove_object_collider(collider_id);
    latest_snapshot_ = session_.snapshot();
    return result;
}

} // namespace stellar::scripting
