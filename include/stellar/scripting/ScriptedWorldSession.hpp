#pragma once

#include <expected>
#include <span>
#include <vector>

#include "stellar/server/WorldSession.hpp"
#include "stellar/scripting/LuaRuntime.hpp"
#include "stellar/scripting/ObjectColliderScriptSystem.hpp"
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
 * requirements. Lua scripting is mandatory server-authoritative infrastructure and always runs in
 * the restricted sandbox.
 */
class ScriptedWorldSession {
public:
    /** @brief Create a scripted session and load all scripts required by current world metadata. */
    [[nodiscard]] static std::expected<ScriptedWorldSession, ScriptError> create(
        const stellar::world::RuntimeWorld& world,
        stellar::server::WorldSessionConfig session_config,
        ScriptRegistry registry,
        LuaRuntimeConfig lua_config = {});

    /** @brief Copying is disabled because native and Lua authoritative state is single-owned. */
    ScriptedWorldSession(const ScriptedWorldSession&) = delete;
    /** @brief Copy assignment is disabled because authoritative script state is single-owned. */
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

    /** @brief Return current authoritative state without replaying script events. */
    [[nodiscard]] stellar::server::WorldSnapshot snapshot() const;

    /** @brief Hard-reset authoritative object colliders and clear all object overlap state. */
    void set_object_colliders(std::span<const stellar::world::ObjectCollider> colliders);

    /** @brief Replace colliders by id and synchronously return any removed/disabled exits. */
    [[nodiscard]] std::vector<stellar::server::ObjectColliderEvent>
    replace_object_colliders_preserving_overlaps(
        std::span<const stellar::world::ObjectCollider> colliders) noexcept;

    /** @brief Enable or disable one collider and synchronously return mutation exits only. */
    [[nodiscard]] stellar::server::ObjectColliderMutationResult set_object_collider_enabled(
        std::uint32_t collider_id, bool enabled) noexcept;

    /** @brief Insert or replace one collider and synchronously return mutation exits only. */
    [[nodiscard]] stellar::server::ObjectColliderMutationResult upsert_object_collider(
        const stellar::world::ObjectCollider& collider) noexcept;

    /** @brief Remove one collider and synchronously return mutation exits only. */
    [[nodiscard]] stellar::server::ObjectColliderMutationResult remove_object_collider(
        std::uint32_t collider_id) noexcept;

private:
    ScriptedWorldSession(const stellar::world::RuntimeWorld& world,
                         stellar::server::WorldSessionConfig session_config,
                         ScriptRegistry registry,
                         LuaRuntime runtime);

    ScriptRegistry registry_;
    LuaRuntime runtime_;
    stellar::server::WorldSession session_;
    TriggerScriptSystem trigger_scripts_;
    ObjectColliderScriptSystem object_collider_scripts_;
    stellar::server::WorldSnapshot latest_snapshot_{};
};

} // namespace stellar::scripting
