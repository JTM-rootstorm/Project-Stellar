#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "stellar/network/Messages.hpp"
#include "stellar/network/Session.hpp"
#include "stellar/platform/Input.hpp"

namespace stellar::client {

/** @brief Presentation-facing runtime mode selected for the active client session. */
enum class ClientRuntimeMode {
    /** @brief No authoritative or connected runtime is active; render static/no-map fallback. */
    kNone,

    /** @brief In-process single-player authority facade. */
    kSinglePlayer,

    /** @brief Socket-backed presentation client connected to a remote authority. */
    kRemoteClient,

    /** @brief Host client connected to an in-process listen-server authority. */
    kListenHost,
};

/**
 * @brief Presentation data produced by one client-runtime update.
 *
 * The frame is non-authoritative: it contains only state that presentation systems need to render,
 * route audio/HUD events, and report session diagnostics.
 */
struct ClientRuntimeFrame {
    /** @brief Latest authoritative world snapshot available to presentation, if one exists. */
    std::optional<stellar::network::NetworkWorldSnapshot> snapshot;

    /** @brief Server-approved gameplay events drained this frame for presentation. */
    std::vector<stellar::network::GameplayEvent> events;

    /** @brief Player id assigned to the local presentation client, or zero when unavailable. */
    stellar::network::PlayerId local_player_id = 0;

    /** @brief Client-side session lifecycle state after this frame. */
    stellar::network::SessionState session_state = stellar::network::SessionState::kDisconnected;

    /** @brief Deterministic runtime/transport/codec diagnostics for tests and tools. */
    std::vector<std::string> diagnostics;

    /** @brief Number of authoritative fixed ticks run by in-process authoritative modes. */
    int ticks_run = 0;

    /** @brief Count of malformed or failed packets rejected without crashing. */
    std::uint32_t rejected_packets = 0;

    /** @brief True when an in-process authoritative runtime dropped excess accumulated time. */
    bool dropped_excess_time = false;
};

/** @brief Non-authoritative client runtime interface consumed by presentation application code. */
class IClientRuntime {
public:
    /** @brief Destroy the client runtime interface without exposing implementation ownership. */
    virtual ~IClientRuntime() = default;

    /** @brief Submit presentation input, advance client runtime state, and return frame data. */
    [[nodiscard]] virtual ClientRuntimeFrame update(const stellar::platform::Input& input,
                                                    float delta_seconds) noexcept = 0;

    /** @brief Return the concrete runtime mode hidden behind this presentation interface. */
    [[nodiscard]] virtual ClientRuntimeMode mode() const noexcept = 0;
};

} // namespace stellar::client
