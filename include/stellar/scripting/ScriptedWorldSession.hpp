#pragma once

#include <expected>
#include <span>
#include <vector>

#include "stellar/server/WorldSession.hpp"
#include "stellar/scripting/LuaRuntime.hpp"
#include "stellar/scripting/ScriptCommandProcessor.hpp"
#include "stellar/scripting/ScriptError.hpp"
#include "stellar/scripting/ScriptRegistry.hpp"
#include "stellar/scripting/ScriptValue.hpp"
#include "stellar/scripting/TriggerScriptSystem.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::scripting {

/** @brief Result of one authoritative scripted world tick. */
struct ScriptedWorldFrame {
    /** @brief Authoritative snapshot after native movement/session simulation. */
    stellar::server::WorldSnapshot snapshot;

    /** @brief Events emitted by scripts during this frame. */
    std::vector<ScriptOutputEvent> script_events;

    /** @brief Script errors produced during this frame. */
    std::vector<ScriptError> script_errors;

    /** @brief Results of native authoritative command processing for script output events. */
    std::vector<ScriptCommandResult> command_results;
};

/**
 * @brief Authoritative world-session wrapper that invokes server-side scripts after native ticks.
 *
 * RuntimeWorld remains caller-owned and must outlive this session, matching WorldSession lifetime
 * requirements. Scripting is opt-in through the stellar_scripting target.
 */
class ScriptedWorldSession {
public:
    /** @brief Create a scripted session and load all scripts required by current world metadata. */
    [[nodiscard]] static std::expected<ScriptedWorldSession, ScriptError> create(
        const stellar::world::RuntimeWorld& world,
        stellar::server::WorldSessionConfig session_config,
        ScriptRegistry registry,
        LuaRuntimeConfig lua_config = {});

    ScriptedWorldSession(const ScriptedWorldSession&) = delete;
    ScriptedWorldSession& operator=(const ScriptedWorldSession&) = delete;
    /** @brief Move a scripted session, transferring native and Lua runtime ownership. */
    ScriptedWorldSession(ScriptedWorldSession&& other) noexcept = default;
    /** @brief Move-assign a scripted session, replacing native and Lua runtime ownership. */
    ScriptedWorldSession& operator=(ScriptedWorldSession&& other) noexcept = default;

    /** @brief Advance one authoritative tick and process script callbacks from emitted events. */
    [[nodiscard]] ScriptedWorldFrame tick(
        std::span<const stellar::server::PlayerCommand> commands) noexcept;

    /** @brief Return latest authoritative snapshot without replaying script events. */
    [[nodiscard]] const stellar::server::WorldSnapshot& latest_snapshot() const noexcept;

private:
    ScriptedWorldSession(const stellar::world::RuntimeWorld& world,
                         stellar::server::WorldSessionConfig session_config,
                         ScriptRegistry registry,
                         LuaRuntime runtime);

    ScriptRegistry registry_;
    LuaRuntime runtime_;
    stellar::server::WorldSession session_;
    TriggerScriptSystem trigger_scripts_;
    stellar::server::WorldSnapshot latest_snapshot_{};
};

} // namespace stellar::scripting
