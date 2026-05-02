#include "stellar/network/SocketTransport.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "stellar/client/ClientWorldReceiver.hpp"
#include "stellar/network/Session.hpp"
#include "stellar/network/SnapshotCodec.hpp"

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

void pump_until_packets(stellar::network::ClientTransport& client,
                        stellar::network::ServerTransport& server,
                        std::size_t expected_server_packets) {
    for (int i = 0; i < 1024; ++i) {
        auto packets = server.receive_from_client();
        if (packets.size() >= expected_server_packets) {
            for (auto& pkt : packets) {
                assert(server.send_to_client(std::move(pkt)).has_value());
            }
            return;
        }
        (void)client.receive_from_server();
    }
    assert(false && "timed out waiting for socket packets");
}

struct SocketPair {
    stellar::network::TcpServerTransportHandle server_handle;
    std::unique_ptr<stellar::network::ClientTransport> client;
};

SocketPair make_pair(stellar::network::SocketTransportConfig config = {}) {
    auto server = stellar::network::listen_tcp_server_once_with_bound_address("127.0.0.1:0", config);
    assert(server.has_value());
    auto client = stellar::network::connect_tcp_client(
        stellar::network::format_socket_address(server->bound_address), config);
    assert(client.has_value());
    for (int i = 0; i < 1024; ++i) {
        (void)server->server->receive_from_client();
    }
    return SocketPair{.server_handle = std::move(*server), .client = std::move(*client)};
}

void endpoint_parser_reports_deterministic_results() {
    auto loopback = stellar::network::parse_socket_address("127.0.0.1:29070");
    assert(loopback.has_value());
    assert(loopback->host == "127.0.0.1");
    assert(loopback->port == 29070);
    assert(stellar::network::parse_socket_address("localhost:1").has_value());
    assert(stellar::network::parse_socket_address("0.0.0.0:0").has_value());

    auto empty = stellar::network::parse_socket_address("");
    assert(!empty.has_value());
    assert(empty.error().code == "empty_endpoint");
    auto no_host = stellar::network::parse_socket_address(":29070");
    assert(!no_host.has_value());
    assert(no_host.error().code == "empty_host");
    auto bad_port = stellar::network::parse_socket_address("localhost:notaport");
    assert(!bad_port.has_value());
    assert(bad_port.error().code == "invalid_port");
    auto range = stellar::network::parse_socket_address("localhost:70000");
    assert(!range.has_value());
    assert(range.error().code == "port_out_of_range");
}

void envelope_preserves_boundaries_and_rejects_oversized_payloads() {
    stellar::network::SocketTransportConfig config{.max_packet_bytes = 4, .nonblocking = true};
    auto encoded = stellar::network::encode_socket_packet(packet("abcd"), config);
    assert(encoded.has_value());
    std::vector<std::byte> stream(encoded->begin(), encoded->begin() + 3);
    auto partial = stellar::network::decode_socket_packets(stream, config);
    assert(partial.has_value());
    assert(partial->empty());
    stream.insert(stream.end(), encoded->begin() + 3, encoded->end());
    auto decoded = stellar::network::decode_socket_packets(stream, config);
    assert(decoded.has_value());
    assert(decoded->size() == 1);
    assert(text((*decoded)[0]) == "abcd");
    assert(stream.empty());

    auto oversized_send = stellar::network::encode_socket_packet(packet("abcde"), config);
    assert(!oversized_send.has_value());
    assert(oversized_send.error().code == "payload_too_large");

    auto oversized = stellar::network::encode_socket_packet(packet("abcde"), {.max_packet_bytes = 8});
    assert(oversized.has_value());
    std::vector<std::byte> oversized_stream = *oversized;
    auto rejected = stellar::network::decode_socket_packets(oversized_stream, config);
    assert(!rejected.has_value());
    assert(rejected.error().code == "payload_too_large");
}

void client_server_connection_fifo_and_boundaries() {
    auto pair = make_pair();
    assert(pair.client->send_to_server(packet("one")).has_value());
    assert(pair.client->send_to_server(packet("two")).has_value());
    assert(pair.client->send_to_server(packet("three")).has_value());

    for (int i = 0; i < 1024; ++i) {
        auto packets = pair.server_handle.server->receive_from_client();
        if (packets.size() == 3) {
            assert(text(packets[0]) == "one");
            assert(text(packets[1]) == "two");
            assert(text(packets[2]) == "three");
            assert(pair.server_handle.server->send_to_client(packet("alpha")).has_value());
            assert(pair.server_handle.server->send_to_client(packet("beta")).has_value());
            break;
        }
    }

    for (int i = 0; i < 1024; ++i) {
        auto packets = pair.client->receive_from_server();
        if (packets.size() == 2) {
            assert(text(packets[0]) == "alpha");
            assert(text(packets[1]) == "beta");
            return;
        }
    }
    assert(false && "timed out waiting for echoed server packets");
}

void oversized_payload_rejected_by_transport_send() {
    stellar::network::SocketTransportConfig config{.max_packet_bytes = 4, .nonblocking = true};
    auto pair = make_pair(config);
    auto sent = pair.client->send_to_server(packet("abcde"));
    assert(!sent.has_value());
    assert(sent.error().code == "payload_too_large");
}

void disconnect_is_reported_on_send() {
    auto pair = make_pair();
    pair.client.reset();
    for (int i = 0; i < 1024; ++i) {
        (void)pair.server_handle.server->receive_from_client();
        auto sent = pair.server_handle.server->send_to_client(packet("after-close"));
        if (!sent.has_value()) {
            assert(sent.error().code == "disconnected" || sent.error().code == "send_failed");
            return;
        }
    }
    assert(false && "disconnect was not surfaced");
}

void session_hello_and_welcome_cross_sockets() {
    auto pair = make_pair();
    stellar::network::ClientHello hello{.protocol_version = stellar::network::kCurrentProtocolVersion,
                                        .client_name = "socket-test",
                                        .requested_map_id = "map-a",
                                        .client_nonce = 99};
    auto encoded_hello = stellar::network::encode_client_hello(hello);
    assert(encoded_hello.has_value());
    assert(pair.client->send_to_server(stellar::network::TransportPacket{
               .channel = stellar::network::TransportChannel::kReliable,
               .payload = stellar::network::to_payload(*encoded_hello),
           }).has_value());

    for (int i = 0; i < 1024; ++i) {
        auto packets = pair.server_handle.server->receive_from_client();
        if (!packets.empty()) {
            auto decoded_hello = stellar::network::decode_client_hello(
                stellar::network::from_payload(packets[0].payload));
            assert(decoded_hello.has_value());
            assert(decoded_hello->requested_map_id == "map-a");
            stellar::network::ServerWelcome welcome{.accepted = true,
                                                    .protocol_version = stellar::network::kCurrentProtocolVersion,
                                                    .session_id = 7,
                                                    .assigned_player_id = 11,
                                                    .map_id = "map-a",
                                                    .rejection_code = {},
                                                    .message = "session accepted"};
            auto encoded_welcome = stellar::network::encode_server_welcome(welcome);
            assert(encoded_welcome.has_value());
            assert(pair.server_handle.server->send_to_client(stellar::network::TransportPacket{
                       .channel = stellar::network::TransportChannel::kReliable,
                       .payload = stellar::network::to_payload(*encoded_welcome),
                   }).has_value());
            break;
        }
    }

    for (int i = 0; i < 1024; ++i) {
        auto packets = pair.client->receive_from_server();
        if (!packets.empty()) {
            auto decoded = stellar::network::decode_server_welcome(
                stellar::network::from_payload(packets[0].payload));
            assert(decoded.has_value());
            assert(decoded->accepted);
            assert(decoded->assigned_player_id == 11);
            return;
        }
    }
    assert(false && "timed out waiting for welcome");
}

void snapshot_crosses_to_client_world_receiver() {
    auto pair = make_pair();
    stellar::network::NetworkWorldSnapshot snapshot{};
    snapshot.tick = 123;
    stellar::server::PlayerSnapshot player{};
    player.player_id = 5;
    player.position = {1.0F, 2.0F, 3.0F};
    snapshot.players.push_back(player);
    auto encoded = stellar::network::encode_snapshot(snapshot);
    assert(encoded.has_value());
    assert(pair.server_handle.server->send_to_client(stellar::network::TransportPacket{
               .channel = stellar::network::TransportChannel::kReliable,
               .payload = stellar::network::to_payload(*encoded),
           }).has_value());

    stellar::client::ClientWorldReceiver receiver;
    for (int i = 0; i < 1024; ++i) {
        auto result = receiver.drain(*pair.client);
        if (result.snapshots == 1) {
            assert(receiver.has_snapshot());
            assert(receiver.latest_snapshot()->tick == 123);
            assert(receiver.latest_snapshot()->players.size() == 1);
            assert(receiver.latest_snapshot()->players[0].player_id == 5);
            return;
        }
    }
    assert(false && "timed out waiting for snapshot");
}

} // namespace

int main() {
    endpoint_parser_reports_deterministic_results();
    envelope_preserves_boundaries_and_rejects_oversized_payloads();
    client_server_connection_fifo_and_boundaries();
    oversized_payload_rejected_by_transport_send();
    disconnect_is_reported_on_send();
    session_hello_and_welcome_cross_sockets();
    snapshot_crosses_to_client_world_receiver();
    return 0;
}
