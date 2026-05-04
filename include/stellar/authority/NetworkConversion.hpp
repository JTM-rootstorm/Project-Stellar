#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "stellar/network/Messages.hpp"
#include "stellar/network/Session.hpp"
#include "stellar/server/WorldSession.hpp"
#include "stellar/scripting/ScriptCommandProcessor.hpp"
#include "stellar/scripting/ScriptError.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::authority {

/** @brief Convert a server snapshot into deterministic protocol snapshot order. */
[[nodiscard]] stellar::network::NetworkWorldSnapshot make_network_snapshot(
    const stellar::server::WorldSnapshot& snapshot);

/** @brief Convert a server gameplay entity into protocol transport form. */
[[nodiscard]] stellar::network::NetworkGameplayEntity make_network_entity(
    const stellar::server::GameplayEntity& entity);

/** @brief Convert one protocol movement command into authority simulation form. */
[[nodiscard]] stellar::server::MovementCommand make_server_movement_command(
    const stellar::network::MovementCommand& movement) noexcept;

/** @brief Convert authoritative script command results into server-approved gameplay events. */
[[nodiscard]] std::vector<stellar::network::GameplayEvent> make_gameplay_events(
    std::uint64_t tick,
    const std::vector<stellar::scripting::ScriptOutputEvent>& script_events,
    const std::vector<stellar::scripting::ScriptCommandResult>& command_results,
    const std::vector<stellar::scripting::ScriptError>& script_errors);

/** @brief Convert one authoritative script error into a server-approved gameplay event. */
[[nodiscard]] stellar::network::GameplayEvent make_script_error_event(
    std::uint64_t tick,
    const stellar::scripting::ScriptError& error);

/** @brief Build a deterministic map identity from runtime world source metadata. */
[[nodiscard]] stellar::network::MapIdentity make_map_identity(
    const stellar::world::RuntimeWorld& world);

} // namespace stellar::authority
