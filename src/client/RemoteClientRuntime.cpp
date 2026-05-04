#include "stellar/client/RemoteClientRuntime.hpp"

#include <utility>

#include "stellar/network/SnapshotCodec.hpp"
#include "stellar/network/SocketTransport.hpp"

namespace stellar::client {
namespace {

[[nodiscard]] stellar::network::TransportPacket reliable_packet(
    std::vector<std::uint8_t> bytes) {
    return stellar::network::TransportPacket{
        .channel = stellar::network::TransportChannel::kReliable,
        .payload = stellar::network::to_payload(bytes),
    };
}

void append_receiver_diagnostics(ClientRuntimeFrame& frame,
                                 const ClientWorldReceiverDrainResult& drain) {
    for (const ClientWorldReceiverError& error : drain.errors) {
        frame.diagnostics.push_back(error.code + ": " + error.message);
    }
}

void append_pending_diagnostics(ClientRuntimeFrame& frame,
                                std::vector<std::string>& pending) {
    frame.diagnostics.insert(frame.diagnostics.end(), pending.begin(), pending.end());
    pending.clear();
}

void append_transport_error(ClientRuntimeFrame& frame,
                            const stellar::network::TransportError& error) {
    ++frame.rejected_packets;
    frame.diagnostics.push_back(error.code + ": " + error.message);
}

} // namespace

std::expected<RemoteClientRuntime, stellar::network::TransportError> RemoteClientRuntime::connect(
    RemoteClientRuntimeConfig config) {
    auto transport = stellar::network::connect_tcp_client(config.connect_endpoint);
    if (!transport) {
        return std::unexpected(transport.error());
    }
    return RemoteClientRuntime(std::move(*transport), std::move(config));
}

RemoteClientRuntime::RemoteClientRuntime(
    std::unique_ptr<stellar::network::ClientTransport> transport,
    RemoteClientRuntimeConfig config)
    : config_(std::move(config)), transport_(std::move(transport)) {
    send_client_hello();
}

ClientRuntimeFrame RemoteClientRuntime::update(const stellar::platform::Input& input,
                                               float delta_seconds) noexcept {
    ClientRuntimeFrame frame;
    append_pending_diagnostics(frame, pending_diagnostics_);
    view_state_ = update_client_view_state(input, delta_seconds, view_state_, config_.look_mapper);

    for (const stellar::network::TransportPacket& packet : transport_->receive_from_server()) {
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
        const ClientWorldReceiverDrainResult drain = receiver_.accept_packet(packet);
        frame.rejected_packets += drain.rejected_packets;
        append_receiver_diagnostics(frame, drain);
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
        } else if (auto sent = transport_->send_to_server(
                       reliable_packet(std::move(*encoded_command)));
                   !sent) {
            append_transport_error(frame, sent.error());
            session_state_ = stellar::network::SessionState::kDisconnected;
        }
    }

    for (const stellar::network::TransportPacket& packet : transport_->receive_from_server()) {
        if (session_state_ != stellar::network::SessionState::kConnected) {
            ++frame.rejected_packets;
            continue;
        }
        const ClientWorldReceiverDrainResult drain = receiver_.accept_packet(packet);
        frame.rejected_packets += drain.rejected_packets;
        append_receiver_diagnostics(frame, drain);
    }

    frame.events = receiver_.take_queued_events();
    frame.snapshot = receiver_.latest_snapshot();
    frame.local_player_id = assigned_player_id_;
    frame.session_state = session_state_;
    return frame;
}

ClientRuntimeMode RemoteClientRuntime::mode() const noexcept {
    return ClientRuntimeMode::kRemoteClient;
}

const std::optional<stellar::network::NetworkWorldSnapshot>& RemoteClientRuntime::latest_snapshot()
    const noexcept {
    return receiver_.latest_snapshot();
}

stellar::network::PlayerId RemoteClientRuntime::local_player_id() const noexcept {
    return assigned_player_id_;
}

stellar::network::SessionState RemoteClientRuntime::session_state() const noexcept {
    return session_state_;
}

std::uint64_t RemoteClientRuntime::next_command_sequence() const noexcept {
    return next_command_sequence_;
}

void RemoteClientRuntime::send_client_hello() noexcept {
    stellar::network::ClientHello hello{};
    hello.protocol_version = stellar::network::kCurrentProtocolVersion;
    hello.client_name = config_.client_name;
    hello.requested_map_id = config_.requested_map_id;
    hello.client_nonce = 1;

    auto encoded = stellar::network::encode_client_hello(hello);
    if (!encoded) {
        session_state_ = stellar::network::SessionState::kRejected;
        pending_diagnostics_.push_back(encoded.error().code + ": " + encoded.error().message);
        return;
    }
    if (auto sent = transport_->send_to_server(reliable_packet(std::move(*encoded))); !sent) {
        session_state_ = stellar::network::SessionState::kRejected;
        pending_diagnostics_.push_back(sent.error().code + ": " + sent.error().message);
    }
}

} // namespace stellar::client
