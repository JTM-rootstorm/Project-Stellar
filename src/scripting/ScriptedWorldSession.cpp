#include "stellar/scripting/ScriptedWorldSession.hpp"

#include "stellar/assets/WorldMetadataAsset.hpp"

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

[[nodiscard]] std::vector<std::string> unique_trigger_script_ids(
    const stellar::world::RuntimeWorld& world) {
    std::vector<std::string> script_ids;
    for (const stellar::assets::WorldMarker& marker : world.world_metadata.markers) {
        if (marker.type != stellar::assets::WorldMarkerType::kTrigger ||
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

    for (const std::string& script_id : unique_trigger_script_ids(world)) {
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
      latest_snapshot_(session_.snapshot()) {}

ScriptedWorldFrame ScriptedWorldSession::tick(
    std::span<const stellar::server::PlayerCommand> commands) noexcept {
    latest_snapshot_ = session_.tick(commands);
    TriggerScriptResult script_result =
        trigger_scripts_.process_trigger_events(runtime_, latest_snapshot_);

    return ScriptedWorldFrame{.snapshot = latest_snapshot_,
                              .script_events = std::move(script_result.output_events),
                              .script_errors = std::move(script_result.errors)};
}

const stellar::server::WorldSnapshot& ScriptedWorldSession::latest_snapshot() const noexcept {
    return latest_snapshot_;
}

} // namespace stellar::scripting
