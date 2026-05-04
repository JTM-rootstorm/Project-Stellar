#include "stellar/client/ListenHostRuntime.hpp"

#include <utility>

#include "stellar/network/SnapshotCodec.hpp"
#include "stellar/network/SocketTransport.hpp"

namespace stellar::client {
namespace {

[[nodiscard]] stellar::network::TransportPacket reliable_packet(std::vector<std::uint8_t> bytes) {
    return stellar::network::TransportPacket{
        .channel = stellar::network::TransportChannel::kReliable,
        .payload = stellar::network::to_payload(bytes),
    };
}

void append_server_runtime_diagnostics(ClientRuntimeFrame& frame,
                                       const stellar::server::ServerRuntimePumpResult& pump) {
    for (const stellar::server::ServerRuntimeError& error : pump.errors) {
        frame.diagnostics.push_back(error.code + ": " + error.message);
    }
}

void append_receiver_diagnostics(ClientRuntimeFrame& frame,
                                 const ClientWorldReceiverDrainResult& drain) {
    for (const ClientWorldReceiverError& error : drain.errors) {
        frame.diagnostics.push_back(error.code + ": " + error.message);
    }
}

void append_pending_diagnostics(ClientRuntimeFrame& frame, std::vector<std::string>& pending) {
    frame.diagnostics.insert(frame.diagnostics.end(), pending.begin(), pending.end());
    pending.clear();
}

[[nodiscard]] stellar::platform::Error error(std::string message) {
    return stellar::platform::Error(std::move(message));
}

} // namespace

std::expected<ListenHostRuntime, stellar::platform::Error> ListenHostRuntime::create(
    stellar::authority::PreparedAuthority authority,
    ListenHostRuntimeConfig config) {
    if (!config.listen_endpoint.has_value()) {
        config.listen_endpoint = "127.0.0.1:29070";
    }
    if (config.server.map_identity.map_id.empty() || config.server.map_identity.map_id == "local") {
        config.server.map_identity = authority.map_identity;
    }

    std::unique_ptr<stellar::network::ServerTransport> listener;
    std::string bound_endpoint;
    if (config.listen_endpoint.has_value()) {
        auto handle = stellar::network::listen_tcp_server_once_with_bound_address(*config.listen_endpoint);
        if (!handle) {
            return std::unexpected(error("Failed to bind listen-host --listen endpoint: " +
                                         handle.error().message));
        }
        bound_endpoint = stellar::network::format_socket_address(handle->bound_address);
        listener = std::move(handle->server);
    }

    return ListenHostRuntime(std::move(authority), std::move(config),
                             stellar::network::make_loopback_transport_pair(), std::move(listener),
                             std::move(bound_endpoint));
}

ListenHostRuntime::ListenHostRuntime(
    stellar::authority::PreparedAuthority authority,
    ListenHostRuntimeConfig config,
    stellar::network::LoopbackTransportPair host_transport,
    std::unique_ptr<stellar::network::ServerTransport> listener,
    std::string bound_endpoint) noexcept
    : config_(std::move(config)),
      authority_(std::move(authority)),
      host_transport_(std::move(host_transport)),
      server_(std::move(authority_.session), config_.server),
      listener_(std::move(listener)),
      bound_endpoint_(std::move(bound_endpoint)) {
    send_client_hello();
}

ClientRuntimeFrame ListenHostRuntime::update(const stellar::platform::Input& input,
                                             float delta_seconds) noexcept {
    ClientRuntimeFrame frame;
    append_pending_diagnostics(frame, pending_diagnostics_);
    view_state_ = update_client_view_state(input, delta_seconds, view_state_, config_.look_mapper);

    if (session_state_ == stellar::network::SessionState::kConnecting) {
        const stellar::server::ServerRuntimePumpResult handshake =
            server_.pump(*host_transport_.server, 0.0F);
        frame.rejected_packets += handshake.rejected_packets;
        append_server_runtime_diagnostics(frame, handshake);
        drain_server_packets(frame);
    }

    if (session_state_ == stellar::network::SessionState::kConnected) {
        stellar::network::NetworkPlayerCommand command{};
        command.player_id = assigned_player_id_;
        command.command_sequence = next_command_sequence_++;
        command.movement = make_movement_command(input, view_state_, config_.input_mapper);

        auto encoded_command = stellar::network::encode_player_command(command);
        if (!encoded_command) {
            ++frame.rejected_packets;
            frame.diagnostics.push_back(encoded_command.error().code + ": " +
                                        encoded_command.error().message);
        } else if (auto sent = host_transport_.client->send_to_server(
                       reliable_packet(std::move(*encoded_command)));
                   !sent) {
            ++frame.rejected_packets;
            frame.diagnostics.push_back(sent.error().code + ": " + sent.error().message);
            session_state_ = stellar::network::SessionState::kDisconnected;
        }
    }

    const stellar::server::ServerRuntimePumpResult pump =
        server_.pump(*host_transport_.server, delta_seconds);
    frame.ticks_run = pump.ticks_run;
    frame.rejected_packets += pump.rejected_packets;
    frame.dropped_excess_time = pump.dropped_excess_time;
    append_server_runtime_diagnostics(frame, pump);

    drain_server_packets(frame);
    frame.events = receiver_.take_queued_events();
    frame.snapshot = receiver_.latest_snapshot();
    frame.local_player_id = assigned_player_id_;
    frame.session_state = session_state_;
    return frame;
}

ClientRuntimeMode ListenHostRuntime::mode() const noexcept {
    return ClientRuntimeMode::kListenHost;
}

ListenHostStatus ListenHostRuntime::status() const noexcept {
    return ListenHostStatus{.running = true,
                            .bound_endpoint = bound_endpoint_,
                            .accepted_clients = server_.connected_clients(),
                            .session_state = session_state_,
                            .map_id = config_.server.map_identity.map_id,
                            .tick = server_.latest_snapshot().tick,
                            .host_player_uses_loopback_slot = true,
                            .remote_clients_deferred = true};
}

const std::optional<stellar::network::NetworkWorldSnapshot>& ListenHostRuntime::latest_snapshot()
    const noexcept {
    return receiver_.latest_snapshot();
}

stellar::network::PlayerId ListenHostRuntime::local_player_id() const noexcept {
    return assigned_player_id_;
}

void ListenHostRuntime::send_client_hello() noexcept {
    stellar::network::ClientHello hello{};
    hello.protocol_version = stellar::network::kCurrentProtocolVersion;
    hello.client_name = config_.client_name;
    hello.requested_map_id = config_.server.map_identity.map_id;
    hello.client_nonce = 1;

    auto encoded = stellar::network::encode_client_hello(hello);
    if (!encoded) {
        session_state_ = stellar::network::SessionState::kRejected;
        pending_diagnostics_.push_back(encoded.error().code + ": " + encoded.error().message);
        return;
    }
    if (auto sent = host_transport_.client->send_to_server(reliable_packet(std::move(*encoded)));
        !sent) {
        session_state_ = stellar::network::SessionState::kRejected;
        pending_diagnostics_.push_back(sent.error().code + ": " + sent.error().message);
    }
}

void ListenHostRuntime::drain_server_packets(ClientRuntimeFrame& frame) noexcept {
    ClientWorldReceiverDrainResult total_drain;
    for (const stellar::network::TransportPacket& packet : host_transport_.client->receive_from_server()) {
        const std::vector<std::uint8_t> bytes = stellar::network::from_payload(packet.payload);
        if (auto welcome = stellar::network::decode_server_welcome(bytes)) {
            if (welcome->accepted) {
                session_state_ = stellar::network::SessionState::kConnected;
                assigned_player_id_ = welcome->assigned_player_id;
            } else {
                session_state_ = stellar::network::SessionState::kRejected;
                frame.diagnostics.push_back(welcome->rejection_code + ": " + welcome->message);
            }
            continue;
        }
        if (session_state_ != stellar::network::SessionState::kConnected) {
            ++frame.rejected_packets;
            frame.diagnostics.push_back(
                "packet_before_welcome: Authoritative packet arrived before welcome acceptance");
            continue;
        }
        ClientWorldReceiverDrainResult drain = receiver_.accept_packet(packet);
        total_drain.snapshots += drain.snapshots;
        total_drain.deltas += drain.deltas;
        total_drain.events += drain.events;
        total_drain.rejected_packets += drain.rejected_packets;
        total_drain.errors.insert(total_drain.errors.end(), drain.errors.begin(), drain.errors.end());
    }
    frame.rejected_packets += total_drain.rejected_packets;
    append_receiver_diagnostics(frame, total_drain);
}

} // namespace stellar::client
