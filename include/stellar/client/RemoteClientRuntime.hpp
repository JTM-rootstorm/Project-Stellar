#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "stellar/client/ClientRuntime.hpp"
#include "stellar/client/ClientWorldReceiver.hpp"
#include "stellar/client/MovementInputMapper.hpp"
#include "stellar/network/Messages.hpp"
#include "stellar/network/Session.hpp"
#include "stellar/network/Transport.hpp"
#include "stellar/platform/Input.hpp"

namespace stellar::client {

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

/**
 * @brief Socket-backed client runtime for remote presentation-only play.
 *
 * The runtime owns only a client transport, session state, input command encoding, and a
 * ClientWorldReceiver. It never constructs local authority, never loads scripts, and performs no
 * prediction, reconciliation, interpolation, or map transfer.
 */
class RemoteClientRuntime : public IClientRuntime {
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
    ~RemoteClientRuntime() override = default;

    /** @brief Submit input when accepted, drain authoritative network state, and expose events. */
    [[nodiscard]] ClientRuntimeFrame update(const stellar::platform::Input& input,
                                            float delta_seconds) noexcept override;

    /** @brief Return the remote client runtime mode for presentation dispatch. */
    [[nodiscard]] ClientRuntimeMode mode() const noexcept override;

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
