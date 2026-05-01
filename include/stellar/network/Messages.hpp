#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "stellar/server/WorldSession.hpp"
#include "stellar/scripting/ScriptCommandProcessor.hpp"
#include "stellar/scripting/ScriptError.hpp"
#include "stellar/scripting/ScriptValue.hpp"

namespace stellar::network {

/** @brief Client-to-server transport message categories. */
enum class ClientMessageType : std::uint8_t {
    kInputCommand = 1,
    kClientHello = 2,
};

/** @brief Server-to-client transport message categories. */
enum class ServerMessageType : std::uint8_t {
    kWelcome = 1,
    kWorldSnapshot = 2,
    kWorldDelta = 3,
    kGameplayEvent = 4,
};

/** @brief Transport-ready client movement command request. */
struct NetworkPlayerCommand {
    /** @brief Authoritative player slot requested by the client. */
    stellar::server::PlayerId player_id = 0;

    /** @brief Monotonic client command sequence for later acknowledgement paths. */
    std::uint64_t command_sequence = 0;

    /** @brief Movement intent to be validated and sanitized by the server. */
    stellar::server::MovementCommand movement{};

};

/** @brief Transport-ready server-owned gameplay entity state. */
struct NetworkGameplayEntity {
    /** @brief Stable authoritative entity identifier. */
    stellar::server::EntityId id = 0;

    /** @brief Authoritative entity category. */
    stellar::server::EntityKind kind = stellar::server::EntityKind::kSprite;

    /** @brief Plain authoritative transform. */
    stellar::server::TransformComponent transform{};

    /** @brief Serializable authoritative gameplay metadata. */
    stellar::server::GameplayEntityMetadata metadata{};

    /** @brief True when the authoritative entity is active/presentable. */
    bool active = true;

    /** @brief Door/gate authoritative open state when applicable. */
    bool open = false;

};

/** @brief Transport-ready authoritative world snapshot. */
struct NetworkWorldSnapshot {
    /** @brief Authoritative server tick that produced this snapshot. */
    std::uint64_t tick = 0;

    /** @brief Deterministically ordered authoritative player states. */
    std::vector<stellar::server::PlayerSnapshot> players;

    /** @brief Deterministically ordered authoritative gameplay entity states. */
    std::vector<NetworkGameplayEntity> entities;

};

/** @brief Server-authored gameplay/presentation event categories. */
enum class GameplayEventKind : std::uint8_t {
    kPickupCollected = 1,
    kDoorStateChanged = 2,
    kScriptCommandApplied = 3,
    kScriptError = 4,
};

/** @brief Server-approved presentation event derived from authoritative runtime activity. */
struct GameplayEvent {
    /** @brief Event category. */
    GameplayEventKind kind = GameplayEventKind::kScriptCommandApplied;

    /** @brief Authoritative tick associated with the event. */
    std::uint64_t tick = 0;

    /** @brief Related authoritative entity/collider id when known, otherwise zero. */
    std::uint32_t entity_id = 0;

    /** @brief Related authoritative player id when known, otherwise zero. */
    std::uint32_t player_id = 0;

    /** @brief Stable event or result code for tools/presentation routing. */
    std::string code;

    /** @brief Human-readable deterministic event diagnostic. */
    std::string message;

};

/** @brief Convert a server snapshot into deterministic transport snapshot order. */
[[nodiscard]] NetworkWorldSnapshot make_network_snapshot(
    const stellar::server::WorldSnapshot& snapshot);

/** @brief Convert a server gameplay entity into transport form. */
[[nodiscard]] NetworkGameplayEntity make_network_entity(
    const stellar::server::GameplayEntity& entity);

/** @brief Convert authoritative script command results into server-approved gameplay events. */
[[nodiscard]] std::vector<GameplayEvent> make_gameplay_events(
    std::uint64_t tick,
    const std::vector<stellar::scripting::ScriptOutputEvent>& script_events,
    const std::vector<stellar::scripting::ScriptCommandResult>& command_results,
    const std::vector<stellar::scripting::ScriptError>& script_errors);

/** @brief Convert one authoritative script error into a server-approved gameplay event. */
[[nodiscard]] GameplayEvent make_script_error_event(std::uint64_t tick,
                                                    const stellar::scripting::ScriptError& error);

} // namespace stellar::network
