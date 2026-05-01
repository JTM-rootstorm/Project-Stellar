#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <variant>
#include <vector>

#include "stellar/network/Messages.hpp"
#include "stellar/network/Transport.hpp"
#include "stellar/server/WorldSession.hpp"
#include "stellar/scripting/ScriptedWorldSession.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::client {

/** @brief Configuration for the in-process authoritative transport bridge. */
struct LocalServerBridgeConfig {
    /** @brief Authoritative world-session configuration. */
    stellar::server::WorldSessionConfig session{};

    /** @brief Maximum fixed authoritative ticks allowed during one bridge pump. */
    int max_ticks_per_pump = 4;

    /** @brief Emit structural deltas after the first full authoritative snapshot when possible. */
    bool emit_deltas = true;
};

/** @brief One bridge packet decode/send failure kept for diagnostics without crashing. */
struct LocalServerBridgeError {
    /** @brief Stable machine-readable failure code. */
    std::string code;

    /** @brief Human-readable deterministic diagnostic. */
    std::string message;
};

/** @brief Result of one authoritative bridge pump. */
struct LocalServerBridgePumpResult {
    /** @brief Number of authoritative fixed ticks run by this pump. */
    int ticks_run = 0;

    /** @brief True when accumulated time was dropped after hitting max_ticks_per_pump. */
    bool dropped_excess_time = false;

    /** @brief Number of malformed or transport-failed packets rejected without crashing. */
    std::uint32_t rejected_packets = 0;

    /** @brief Diagnostics for rejected packets. */
    std::vector<LocalServerBridgeError> errors;
};

/**
 * @brief In-process authoritative server adapter behind transport-neutral packets.
 *
 * The bridge decodes client command requests, overwrites player authority with the configured local
 * player slot, advances the authoritative session, then emits encoded snapshots/deltas/events.
 */
class LocalServerBridge {
public:
    /** @brief Construct a plain authoritative bridge over caller-owned runtime world data. */
    explicit LocalServerBridge(const stellar::world::RuntimeWorld& world,
                               LocalServerBridgeConfig config = {});

    /** @brief Construct a scripted authoritative bridge over a preloaded scripted session. */
    explicit LocalServerBridge(stellar::scripting::ScriptedWorldSession scripted_session,
                               LocalServerBridgeConfig config = {});

    /** @brief Return the latest authoritative transport snapshot without pumping messages. */
    [[nodiscard]] const stellar::network::NetworkWorldSnapshot& latest_snapshot() const noexcept;

    /** @brief Drain client packets, advance fixed authoritative ticks, and emit server packets. */
    [[nodiscard]] LocalServerBridgePumpResult pump(stellar::network::ServerTransport& transport,
                                                   float delta_seconds) noexcept;

private:
    LocalServerBridgeConfig config_{};
    std::variant<stellar::server::WorldSession, stellar::scripting::ScriptedWorldSession> session_;
    stellar::network::NetworkPlayerCommand pending_command_{};
    stellar::network::NetworkWorldSnapshot latest_snapshot_{};
    bool has_sent_snapshot_ = false;
    float accumulated_seconds_ = 0.0F;
};

} // namespace stellar::client
