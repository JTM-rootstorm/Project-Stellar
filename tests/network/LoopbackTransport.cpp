#include "stellar/network/SnapshotCodec.hpp"
#include "stellar/network/Transport.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

stellar::network::TransportPacket packet(std::string text) {
    std::vector<std::byte> payload;
    payload.reserve(text.size());
    for (char ch : text) {
        payload.push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
    }
    return stellar::network::TransportPacket{.channel = stellar::network::TransportChannel::kReliable,
                                             .payload = std::move(payload)};
}

std::string text(const stellar::network::TransportPacket& packet) {
    std::string result;
    result.reserve(packet.payload.size());
    for (std::byte byte : packet.payload) {
        result.push_back(static_cast<char>(byte));
    }
    return result;
}

void reliable_packets_preserve_fifo_order() {
    auto pair = stellar::network::make_loopback_transport_pair();

    assert(pair.client->send_to_server(packet("first")).has_value());
    assert(pair.client->send_to_server(packet("second")).has_value());
    assert(pair.client->send_to_server(packet("third")).has_value());

    const auto packets = pair.server->receive_from_client();
    assert(packets.size() == 3);
    assert(text(packets[0]) == "first");
    assert(text(packets[1]) == "second");
    assert(text(packets[2]) == "third");
    assert(pair.server->receive_from_client().empty());
}

void server_to_client_preserves_fifo_order() {
    auto pair = stellar::network::make_loopback_transport_pair();

    assert(pair.server->send_to_client(packet("snapshot")).has_value());
    assert(pair.server->send_to_client(packet("event")).has_value());

    const auto packets = pair.client->receive_from_server();
    assert(packets.size() == 2);
    assert(text(packets[0]) == "snapshot");
    assert(text(packets[1]) == "event");
}

void client_input_command_serializes_crosses_and_decodes() {
    auto pair = stellar::network::make_loopback_transport_pair();
    stellar::network::NetworkPlayerCommand command{};
    command.player_id = 777;
    command.command_sequence = 42;
    command.movement.wish_direction = {1.0F, 1.0F, 0.0F};
    command.movement.jump = true;
    command.movement.view_yaw_degrees = 270.0F;
    command.movement.view_pitch_degrees = -15.0F;
    command.movement.has_view_angles = true;

    auto encoded = stellar::network::encode_player_command(command);
    assert(encoded.has_value());
    assert(pair.client->send_to_server(stellar::network::TransportPacket{
               .channel = stellar::network::TransportChannel::kReliable,
               .payload = stellar::network::to_payload(*encoded),
           }).has_value());

    const auto packets = pair.server->receive_from_client();
    assert(packets.size() == 1);
    auto decoded = stellar::network::decode_player_command(
        stellar::network::from_payload(packets[0].payload));
    assert(decoded.has_value());
    assert(decoded->player_id == 777);
    assert(decoded->command_sequence == 42);
    assert(decoded->movement.wish_direction == command.movement.wish_direction);
    assert(decoded->movement.jump);
    assert(decoded->movement.view_yaw_degrees == command.movement.view_yaw_degrees);
    assert(decoded->movement.view_pitch_degrees == command.movement.view_pitch_degrees);
    assert(decoded->movement.has_view_angles);
}

} // namespace

int main() {
    reliable_packets_preserve_fifo_order();
    server_to_client_preserves_fifo_order();
    client_input_command_serializes_crosses_and_decodes();
    return 0;
}
