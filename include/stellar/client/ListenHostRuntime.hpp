#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "stellar/authority/AuthorityBootstrap.hpp"
#include "stellar/client/ClientRuntime.hpp"
#include "stellar/client/ClientWorldReceiver.hpp"
#include "stellar/client/MovementInputMapper.hpp"
#include "stellar/network/Session.hpp"
#include "stellar/network/Transport.hpp"
#include "stellar/platform/Error.hpp"
#include "stellar/server/ServerRuntime.hpp"

namespace stellar::client {

/** @brief Configuration for client-hosted listen-server mode. */
struct ListenHostRuntimeConfig {
    /** @brief Optional TCP bind endpoint for the deferred remote-client listener. */
    std::optional<std::string> listen_endpoint;

    /** @brief Human-readable host client name sent in the in-process ClientHello. */
    std::string client_name = "host";

    /** @brief Server runtime configuration for the authoritative listen host. */
    stellar::server::ServerRuntimeConfig server{};

    /** @brief Input mapper configuration used to convert host input to network commands. */
    MovementInputMapperConfig input_mapper{};

    /** @brief Look mapper configuration used to submit authoritative view angles. */
    LookInputMapperConfig look_mapper{};
};

/** @brief Observable listen-host status for tests, diagnostics, and UI. */
struct ListenHostStatus {
    /** @brief True once the listen host owns an authoritative server runtime. */
    bool running = false;

    /** @brief Concrete bound TCP endpoint, or empty when no listener was created. */
    std::string bound_endpoint;

    /** @brief Number of accepted clients. CS-6 supports only the loopback host client. */
    std::uint32_t accepted_clients = 0;

    /** @brief Current host-client session lifecycle state. */
    stellar::network::SessionState session_state = stellar::network::SessionState::kDisconnected;

    /** @brief Stable map compatibility identifier served by the listen host. */
    std::string map_id;

    /** @brief Latest authoritative server tick exposed to presentation. */
    std::uint64_t tick = 0;

    /** @brief True because the in-process host player consumes the single active player slot. */
    bool host_player_uses_loopback_slot = true;

    /** @brief True because remote-client serving is bound/deferred and not true multiplayer. */
    bool remote_clients_deferred = true;
};

/**
 * @brief Client-hosted authoritative server runtime with loopback host-player connection.
 *
 * The host player uses an in-process loopback ClientTransport plus ClientWorldReceiver, preserving
 * the same welcome/snapshot/input path used by remote clients. CS-6 intentionally supports one
 * accepted client and one active player: the host loopback client consumes that slot. A TCP listener
 * may be bound for endpoint/lifetime validation, but remote-client serving remains deferred and is
 * not true multiplayer.
 */
class ListenHostRuntime : public IClientRuntime {
public:
    /** @brief Create a listen host from prepared authoritative map/session state. */
    [[nodiscard]] static std::expected<ListenHostRuntime, stellar::platform::Error> create(
        stellar::authority::PreparedAuthority authority,
        ListenHostRuntimeConfig config = {});

    ListenHostRuntime(const ListenHostRuntime&) = delete;
    ListenHostRuntime& operator=(const ListenHostRuntime&) = delete;
    /** @brief Move a listen host and its server, loopback, and listener ownership. */
    ListenHostRuntime(ListenHostRuntime&& other) noexcept = default;
    /** @brief Move-assign a listen host and its server, loopback, and listener ownership. */
    ListenHostRuntime& operator=(ListenHostRuntime&& other) noexcept = default;
    ~ListenHostRuntime() override = default;

    /** @brief Submit host input through loopback, pump authority, and return presentation data. */
    [[nodiscard]] ClientRuntimeFrame update(const stellar::platform::Input& input,
                                            float delta_seconds) noexcept override;

    /** @brief Return listen-host presentation runtime mode. */
    [[nodiscard]] ClientRuntimeMode mode() const noexcept override;

    /** @brief Return current listen-host status without advancing simulation. */
    [[nodiscard]] ListenHostStatus status() const noexcept;

    /** @brief Return latest received authoritative snapshot, if one has arrived. */
    [[nodiscard]] const std::optional<stellar::network::NetworkWorldSnapshot>& latest_snapshot()
        const noexcept;

    /** @brief Return assigned host player id after accepted welcome, otherwise zero. */
    [[nodiscard]] stellar::network::PlayerId local_player_id() const noexcept;

private:
    ListenHostRuntime(stellar::authority::PreparedAuthority authority,
                      ListenHostRuntimeConfig config,
                      stellar::network::LoopbackTransportPair host_transport,
                      std::unique_ptr<stellar::network::ServerTransport> listener,
                      std::string bound_endpoint) noexcept;

    /** @brief Encode and send host ClientHello over the loopback transport. */
    void send_client_hello() noexcept;

    /** @brief Drain loopback server packets through welcome handling and ClientWorldReceiver. */
    void drain_server_packets(ClientRuntimeFrame& frame) noexcept;

    ListenHostRuntimeConfig config_{};
    stellar::authority::PreparedAuthority authority_;
    stellar::network::LoopbackTransportPair host_transport_{};
    stellar::server::ServerRuntime server_;
    std::unique_ptr<stellar::network::ServerTransport> listener_;
    std::string bound_endpoint_;
    ClientWorldReceiver receiver_{};
    std::uint64_t next_command_sequence_ = 1;
    stellar::network::SessionState session_state_ = stellar::network::SessionState::kConnecting;
    stellar::network::PlayerId assigned_player_id_ = 0;
    ClientViewState view_state_{};
    std::vector<std::string> pending_diagnostics_;
};

} // namespace stellar::client
