#include "stellar/client/NetworkedClientRuntime.hpp"

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

void append_bridge_diagnostics(NetworkedClientFrameResult& frame,
                               const LocalServerBridgePumpResult& pump) {
    for (const LocalServerBridgeError& error : pump.errors) {
        frame.diagnostics.push_back(error.code + ": " + error.message);
    }
}

void append_receiver_diagnostics(NetworkedClientFrameResult& frame,
                                  const ClientWorldReceiverDrainResult& drain) {
    for (const ClientWorldReceiverError& error : drain.errors) {
        frame.diagnostics.push_back(error.code + ": " + error.message);
    }
}

void append_pending_diagnostics(NetworkedClientFrameResult& frame,
                                std::vector<std::string>& pending) {
    frame.diagnostics.insert(frame.diagnostics.end(), pending.begin(), pending.end());
    pending.clear();
}

void append_transport_error(NetworkedClientFrameResult& frame,
                            const stellar::network::TransportError& error) {
    ++frame.rejected_packets;
    frame.diagnostics.push_back(error.code + ": " + error.message);
}

} // namespace

NetworkedClientRuntime::NetworkedClientRuntime(const stellar::world::RuntimeWorld& world,
                                               NetworkedClientRuntimeConfig config)
    : config_(config),
      transport_(stellar::network::make_loopback_transport_pair()),
      bridge_(world, config_.bridge) {
    send_client_hello();
}

NetworkedClientRuntime::NetworkedClientRuntime(
    stellar::scripting::ScriptedWorldSession scripted_session,
    NetworkedClientRuntimeConfig config)
    : config_(config),
      transport_(stellar::network::make_loopback_transport_pair()),
      bridge_(std::move(scripted_session), config_.bridge) {
    send_client_hello();
}

NetworkedClientFrameResult NetworkedClientRuntime::update(const stellar::platform::Input& input,
                                                            float delta_seconds) noexcept {
    NetworkedClientFrameResult frame;
    append_pending_diagnostics(frame, pending_diagnostics_);
    view_state_ = update_client_view_state(input, delta_seconds, view_state_, config_.look_mapper);

    auto drain_server_packets = [&]() {
        ClientWorldReceiverDrainResult total_drain;
        for (const stellar::network::TransportPacket& packet :
             transport_.client->receive_from_server()) {
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
            total_drain.errors.insert(total_drain.errors.end(), drain.errors.begin(),
                                      drain.errors.end());
        }
        frame.rejected_packets += total_drain.rejected_packets;
        append_receiver_diagnostics(frame, total_drain);
    };

    if (session_state_ == stellar::network::SessionState::kConnecting) {
        const LocalServerBridgePumpResult handshake = bridge_.pump(*transport_.server, 0.0F);
        frame.rejected_packets += handshake.rejected_packets;
        append_bridge_diagnostics(frame, handshake);
        drain_server_packets();
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
        } else if (auto sent = transport_.client->send_to_server(
                       reliable_packet(std::move(*encoded_command)));
                   !sent) {
            ++frame.rejected_packets;
            frame.diagnostics.push_back(sent.error().code + ": " + sent.error().message);
        }
    }

    const LocalServerBridgePumpResult pump = bridge_.pump(*transport_.server, delta_seconds);
    frame.ticks_run = pump.ticks_run;
    frame.rejected_packets += pump.rejected_packets;
    frame.dropped_excess_time = pump.dropped_excess_time;
    append_bridge_diagnostics(frame, pump);

    drain_server_packets();
    frame.events = receiver_.take_queued_events();
    frame.session_state = session_state_;

    return frame;
}

const std::optional<stellar::network::NetworkWorldSnapshot>&
NetworkedClientRuntime::latest_snapshot() const noexcept {
    return receiver_.latest_snapshot();
}

stellar::server::PlayerId NetworkedClientRuntime::local_player_id() const noexcept {
    return assigned_player_id_;
}

stellar::network::SessionState NetworkedClientRuntime::session_state() const noexcept {
    return session_state_;
}

std::uint64_t NetworkedClientRuntime::next_command_sequence() const noexcept {
    return next_command_sequence_;
}

ClientWorldReceiverDrainResult NetworkedClientRuntime::accept_authoritative_packet_for_test(
    const stellar::network::TransportPacket& packet) noexcept {
    return receiver_.accept_packet(packet);
}

void NetworkedClientRuntime::send_client_hello() noexcept {
    stellar::network::ClientHello hello{};
    hello.protocol_version = stellar::network::kCurrentProtocolVersion;
    hello.client_name = "local-client";
    hello.requested_map_id = config_.bridge.map_identity.map_id;
    hello.client_nonce = 1;

    auto encoded = stellar::network::encode_client_hello(hello);
    if (!encoded) {
        session_state_ = stellar::network::SessionState::kRejected;
        pending_diagnostics_.push_back(encoded.error().code + ": " + encoded.error().message);
        return;
    }
    if (auto sent = transport_.client->send_to_server(reliable_packet(std::move(*encoded))); !sent) {
        session_state_ = stellar::network::SessionState::kRejected;
        pending_diagnostics_.push_back(sent.error().code + ": " + sent.error().message);
    }
}

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

NetworkedClientFrameResult RemoteClientRuntime::update(const stellar::platform::Input& input,
                                                        float delta_seconds) noexcept {
    NetworkedClientFrameResult frame;
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
    frame.session_state = session_state_;
    return frame;
}

const std::optional<stellar::network::NetworkWorldSnapshot>& RemoteClientRuntime::latest_snapshot()
    const noexcept {
    return receiver_.latest_snapshot();
}

stellar::server::PlayerId RemoteClientRuntime::local_player_id() const noexcept {
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
