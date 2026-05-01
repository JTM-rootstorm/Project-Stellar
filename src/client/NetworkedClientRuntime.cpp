#include "stellar/client/NetworkedClientRuntime.hpp"

#include <utility>

#include "stellar/network/SnapshotCodec.hpp"

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

} // namespace

NetworkedClientRuntime::NetworkedClientRuntime(const stellar::world::RuntimeWorld& world,
                                               NetworkedClientRuntimeConfig config)
    : config_(config),
      transport_(stellar::network::make_loopback_transport_pair()),
      bridge_(world, config_.bridge) {}

NetworkedClientRuntime::NetworkedClientRuntime(
    stellar::scripting::ScriptedWorldSession scripted_session,
    NetworkedClientRuntimeConfig config)
    : config_(config),
      transport_(stellar::network::make_loopback_transport_pair()),
      bridge_(std::move(scripted_session), config_.bridge) {}

NetworkedClientFrameResult NetworkedClientRuntime::update(const stellar::platform::Input& input,
                                                          float delta_seconds) noexcept {
    NetworkedClientFrameResult frame;

    stellar::network::NetworkPlayerCommand command{};
    command.player_id = config_.bridge.session.local_player_id;
    command.command_sequence = next_command_sequence_++;
    command.movement = make_movement_command(input, config_.input_mapper);

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

    const LocalServerBridgePumpResult pump = bridge_.pump(*transport_.server, delta_seconds);
    frame.ticks_run = pump.ticks_run;
    frame.rejected_packets += pump.rejected_packets;
    frame.dropped_excess_time = pump.dropped_excess_time;
    append_bridge_diagnostics(frame, pump);

    const ClientWorldReceiverDrainResult drain = receiver_.drain(*transport_.client);
    frame.rejected_packets += drain.rejected_packets;
    append_receiver_diagnostics(frame, drain);
    frame.events = receiver_.take_queued_events();

    return frame;
}

const std::optional<stellar::network::NetworkWorldSnapshot>&
NetworkedClientRuntime::latest_snapshot() const noexcept {
    return receiver_.latest_snapshot();
}

stellar::server::PlayerId NetworkedClientRuntime::local_player_id() const noexcept {
    return config_.bridge.session.local_player_id;
}

std::uint64_t NetworkedClientRuntime::next_command_sequence() const noexcept {
    return next_command_sequence_;
}

ClientWorldReceiverDrainResult NetworkedClientRuntime::accept_authoritative_packet_for_test(
    const stellar::network::TransportPacket& packet) noexcept {
    return receiver_.accept_packet(packet);
}

} // namespace stellar::client
