#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "stellar/client/ClientWorldReceiver.hpp"
#include "stellar/client/LocalServerBridge.hpp"
#include "stellar/client/MovementInputMapper.hpp"
#include "stellar/network/Messages.hpp"
#include "stellar/network/Transport.hpp"
#include "stellar/platform/Input.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::client {

/** @brief Configuration for local mapped play over the transport/receiver path. */
struct NetworkedClientRuntimeConfig {
    /** @brief Authoritative server bridge configuration. */
    LocalServerBridgeConfig bridge{};

    /** @brief Input mapper configuration used to convert client input to network commands. */
    MovementInputMapperConfig input_mapper{};
};

/** @brief Result of one local networked client frame update. */
struct NetworkedClientFrameResult {
    /** @brief Number of authoritative fixed ticks run by the local server bridge. */
    int ticks_run = 0;

    /** @brief Count of malformed or failed packets rejected without crashing. */
    std::uint32_t rejected_packets = 0;

    /** @brief True when the authoritative bridge dropped excess accumulated time. */
    bool dropped_excess_time = false;

    /** @brief Server-approved gameplay events drained this frame for presentation. */
    std::vector<stellar::network::GameplayEvent> events;

    /** @brief Deterministic local transport/codec/receiver diagnostics for tests and tools. */
    std::vector<std::string> diagnostics;
};

/**
 * @brief Client-owned local networked runtime for mapped play.
 *
 * The runtime sends local input through a loopback ClientTransport, pumps a server-authoritative
 * LocalServerBridge, drains ClientWorldReceiver, and exposes only latest authoritative network
 * snapshots for presentation. It performs no prediction or reconciliation.
 */
class NetworkedClientRuntime {
public:
    /** @brief Construct a local networked runtime over caller-owned backend-neutral world data. */
    explicit NetworkedClientRuntime(const stellar::world::RuntimeWorld& world,
                                    NetworkedClientRuntimeConfig config = {});

    /** @brief Construct a local networked runtime over preloaded scripted authority. */
    explicit NetworkedClientRuntime(stellar::scripting::ScriptedWorldSession scripted_session,
                                    NetworkedClientRuntimeConfig config = {});

    /** @brief Submit local input, pump authority, and drain authoritative network state. */
    [[nodiscard]] NetworkedClientFrameResult update(const stellar::platform::Input& input,
                                                    float delta_seconds) noexcept;

    /** @brief Return the latest received authoritative network snapshot, if any. */
    [[nodiscard]] const std::optional<stellar::network::NetworkWorldSnapshot>& latest_snapshot()
        const noexcept;

    /** @brief Return the local player id assigned for this local transport phase. */
    [[nodiscard]] stellar::server::PlayerId local_player_id() const noexcept;

    /** @brief Return the next command sequence value that will be assigned. */
    [[nodiscard]] std::uint64_t next_command_sequence() const noexcept;

    /** @brief Test hook: decode one authoritative packet through the receiver without crashing. */
    [[nodiscard]] ClientWorldReceiverDrainResult accept_authoritative_packet_for_test(
        const stellar::network::TransportPacket& packet) noexcept;

private:
    NetworkedClientRuntimeConfig config_{};
    stellar::network::LoopbackTransportPair transport_{};
    LocalServerBridge bridge_;
    ClientWorldReceiver receiver_{};
    std::uint64_t next_command_sequence_ = 1;
};

} // namespace stellar::client
