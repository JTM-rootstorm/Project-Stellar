#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <variant>
#include <vector>

#include "stellar/network/Messages.hpp"
#include "stellar/network/Session.hpp"
#include "stellar/network/Transport.hpp"
#include "stellar/server/WorldSession.hpp"
#include "stellar/scripting/ScriptedWorldSession.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::server {

/** @brief Server-owned authoritative runtime configuration for one active client session. */
struct ServerRuntimeConfig {
    /** @brief Authoritative world-session configuration. */
    stellar::server::WorldSessionConfig session{};

    /** @brief Maximum fixed authoritative ticks allowed during one runtime pump. */
    int max_ticks_per_pump = 4;

    /** @brief Authoritative snapshot send rate in hertz. Zero emits state every pump. */
    int snapshot_rate = 0;

    /** @brief Emit structural deltas after the first full authoritative snapshot when possible. */
    bool emit_deltas = true;

    /** @brief Expected server map identity for deterministic session compatibility checks. */
    stellar::network::MapIdentity map_identity = stellar::network::make_map_identity("local");
};

/** @brief One server runtime packet decode/send failure kept for diagnostics without crashing. */
struct ServerRuntimeError {
    /** @brief Stable machine-readable failure code. */
    std::string code;

    /** @brief Human-readable deterministic diagnostic. */
    std::string message;
};

/** @brief Result of one authoritative server runtime pump. */
struct ServerRuntimePumpResult {
    /** @brief Number of authoritative fixed ticks run by this pump. */
    int ticks_run = 0;

    /** @brief True when accumulated time was dropped after hitting max_ticks_per_pump. */
    bool dropped_excess_time = false;

    /** @brief Number of malformed or transport-failed packets rejected without crashing. */
    std::uint32_t rejected_packets = 0;

    /** @brief Current server-side session lifecycle state after this pump. */
    stellar::network::SessionState session_state = stellar::network::SessionState::kDisconnected;

    /** @brief Diagnostics for rejected packets. */
    std::vector<ServerRuntimeError> errors;
};

/**
 * @brief Reusable authoritative server runtime behind transport-neutral packets.
 *
 * The runtime decodes client hello/command requests, assigns the authoritative player slot,
 * advances the server-owned session, then emits encoded welcomes, snapshots, deltas, and gameplay
 * events. It intentionally supports one active client/player for this phase.
 */
class ServerRuntime {
public:
    /** @brief Construct a plain authoritative runtime over caller-owned runtime world data. */
    explicit ServerRuntime(const stellar::world::RuntimeWorld& world,
                           ServerRuntimeConfig config = {});

    /** @brief Construct a scripted authoritative runtime over a preloaded scripted session. */
    explicit ServerRuntime(stellar::scripting::ScriptedWorldSession scripted_session,
                           ServerRuntimeConfig config = {});

    /** @brief Construct over an already prepared authoritative session variant. */
    explicit ServerRuntime(
        std::variant<stellar::server::WorldSession, stellar::scripting::ScriptedWorldSession> session,
        ServerRuntimeConfig config = {});

    /** @brief Return the latest authoritative transport snapshot without pumping messages. */
    [[nodiscard]] const stellar::network::NetworkWorldSnapshot& latest_snapshot() const noexcept;

    /** @brief Return the current server-side session lifecycle state. */
    [[nodiscard]] stellar::network::SessionState session_state() const noexcept;

    /** @brief Return number of currently accepted clients, zero or one for this runtime. */
    [[nodiscard]] std::uint32_t connected_clients() const noexcept;

    /** @brief Drain client packets, advance fixed authoritative ticks, and emit server packets. */
    [[nodiscard]] ServerRuntimePumpResult pump(stellar::network::ServerTransport& transport,
                                               float delta_seconds) noexcept;

private:
    ServerRuntimeConfig config_{};
    std::variant<stellar::server::WorldSession, stellar::scripting::ScriptedWorldSession> session_;
    stellar::network::NetworkPlayerCommand pending_command_{};
    stellar::network::NetworkWorldSnapshot latest_snapshot_{};
    stellar::network::SessionState session_state_ = stellar::network::SessionState::kConnecting;
    stellar::network::SessionId session_id_ = 1;
    std::uint32_t connected_clients_ = 0;
    bool has_sent_snapshot_ = false;
    float accumulated_seconds_ = 0.0F;
    float snapshot_seconds_ = 0.0F;
};

} // namespace stellar::server
