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
#include "stellar/network/Session.hpp"
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

    /** @brief Look mapper configuration used to submit authoritative view angles. */
    LookInputMapperConfig look_mapper{};
};

/** @brief Configuration for socket-backed remote presentation client mode. */
struct RemoteClientRuntimeConfig {
    /** @brief Host:port endpoint used to create the socket client transport. */
    std::string connect_endpoint;

    /** @brief Client name sent in ClientHello. */
    std::string client_name = "local";

    /** @brief Optional expected map id for future presentation-map validation. */
    std::string requested_map_id;

    /** @brief Input mapper configuration used to convert client input to network commands. */
    MovementInputMapperConfig input_mapper{};

    /** @brief Look mapper configuration used to submit authoritative view angles. */
    LookInputMapperConfig look_mapper{};
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

    /** @brief Client-side session lifecycle state after this frame. */
    stellar::network::SessionState session_state = stellar::network::SessionState::kDisconnected;
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
    [[nodiscard]] stellar::network::PlayerId local_player_id() const noexcept;

    /** @brief Return the current client-side session lifecycle state. */
    [[nodiscard]] stellar::network::SessionState session_state() const noexcept;

    /** @brief Return the next command sequence value that will be assigned. */
    [[nodiscard]] std::uint64_t next_command_sequence() const noexcept;

    /** @brief Test hook: decode one authoritative packet through the receiver without crashing. */
    [[nodiscard]] ClientWorldReceiverDrainResult accept_authoritative_packet_for_test(
        const stellar::network::TransportPacket& packet) noexcept;

private:
    /** @brief Encode and send the deterministic session hello over the local transport. */
    void send_client_hello() noexcept;

    NetworkedClientRuntimeConfig config_{};
    stellar::network::LoopbackTransportPair transport_{};
    LocalServerBridge bridge_;
    ClientWorldReceiver receiver_{};
    std::uint64_t next_command_sequence_ = 1;
    stellar::network::SessionState session_state_ = stellar::network::SessionState::kConnecting;
    stellar::network::PlayerId assigned_player_id_ = 0;
    ClientViewState view_state_{};
    std::vector<std::string> pending_diagnostics_;
};

/**
 * @brief Socket-backed client runtime for remote presentation-only play.
 *
 * The runtime owns only a client transport, session state, input command encoding, and a
 * ClientWorldReceiver. It never constructs local authority, never loads scripts, and performs no
 * prediction, reconciliation, interpolation, or map transfer.
 */
class RemoteClientRuntime {
public:
    /** @brief Connect to a remote host:port endpoint and immediately send ClientHello. */
    [[nodiscard]] static std::expected<RemoteClientRuntime, stellar::network::TransportError>
    connect(RemoteClientRuntimeConfig config);

    /** @brief Construct over an already connected client transport and send ClientHello. */
    RemoteClientRuntime(std::unique_ptr<stellar::network::ClientTransport> transport,
                        RemoteClientRuntimeConfig config);

    RemoteClientRuntime(const RemoteClientRuntime&) = delete;
    RemoteClientRuntime& operator=(const RemoteClientRuntime&) = delete;
    /** @brief Move a remote runtime and its transport/receiver ownership. */
    RemoteClientRuntime(RemoteClientRuntime&& other) noexcept = default;
    /** @brief Move-assign a remote runtime and its transport/receiver ownership. */
    RemoteClientRuntime& operator=(RemoteClientRuntime&& other) noexcept = default;
    ~RemoteClientRuntime() = default;

    /** @brief Submit input when accepted, drain authoritative network state, and expose events. */
    [[nodiscard]] NetworkedClientFrameResult update(const stellar::platform::Input& input,
                                                    float delta_seconds) noexcept;

    /** @brief Return latest received authoritative network snapshot, if any. */
    [[nodiscard]] const std::optional<stellar::network::NetworkWorldSnapshot>& latest_snapshot()
        const noexcept;

    /** @brief Return assigned remote player id after accepted welcome, otherwise zero. */
    [[nodiscard]] stellar::network::PlayerId local_player_id() const noexcept;

    /** @brief Return current remote session lifecycle state. */
    [[nodiscard]] stellar::network::SessionState session_state() const noexcept;

    /** @brief Return next command sequence value that will be assigned. */
    [[nodiscard]] std::uint64_t next_command_sequence() const noexcept;

private:
    /** @brief Encode and send ClientHello over the remote transport. */
    void send_client_hello() noexcept;

    RemoteClientRuntimeConfig config_{};
    std::unique_ptr<stellar::network::ClientTransport> transport_;
    ClientWorldReceiver receiver_{};
    std::uint64_t next_command_sequence_ = 1;
    stellar::network::SessionState session_state_ = stellar::network::SessionState::kConnecting;
    stellar::network::PlayerId assigned_player_id_ = 0;
    ClientViewState view_state_{};
    std::vector<std::string> pending_diagnostics_;
};

} // namespace stellar::client
