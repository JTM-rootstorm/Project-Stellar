#include "stellar/server/DedicatedServer.hpp"

#include "BspFixture.hpp"

#include <cassert>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "stellar/network/SnapshotCodec.hpp"
#include "stellar/network/SnapshotDelta.hpp"
#include "stellar/network/SocketTransport.hpp"
#include "stellar/network/Transport.hpp"

namespace {

std::filesystem::path temp_root() {
    const std::filesystem::path root = std::filesystem::temp_directory_path() /
                                      "stellar_dedicated_server_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

std::filesystem::path write_map(const std::filesystem::path& root,
                                const std::string& name,
                                std::string_view fixture = "minimal_zup_room") {
    const std::filesystem::path path = root / name;
    stellar::tests::fixtures::write_bsp_fixture(path, fixture);
    return path;
}

void write_text(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << text;
}

std::vector<stellar::network::TransportPacket> receive_for(
    stellar::network::ClientTransport& client,
    int attempts = 64) {
    std::vector<stellar::network::TransportPacket> packets;
    for (int i = 0; i < attempts; ++i) {
        std::vector<stellar::network::TransportPacket> next = client.receive_from_server();
        packets.insert(packets.end(), std::make_move_iterator(next.begin()),
                       std::make_move_iterator(next.end()));
        if (!packets.empty()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return packets;
}

void pump_until_packets(stellar::server::DedicatedServer& server,
                        stellar::network::ClientTransport& client,
                        float dt,
                        std::vector<stellar::network::TransportPacket>& packets) {
    for (int i = 0; i < 64 && packets.empty(); ++i) {
        auto pump = server.pump_once(dt);
        assert(pump.has_value());
        packets = receive_for(client, 1);
    }
}

void config_parse_success_and_failure() {
    const char* success[] = {"stellar-server", "--validate-config", "--map", "room.bsp",
                             "--script-root", "scripts", "--listen", "127.0.0.1:29070",
                             "--tick-rate", "30", "--snapshot-rate", "10", "--max-clients", "1"};
    auto parsed = stellar::server::DedicatedServer::parse_cli(14, success);
    assert(parsed.has_value());
    assert(parsed->validate_only);
    assert(parsed->map_path == "room.bsp");
    assert(parsed->script_root == "scripts");
    assert(parsed->tick_rate == 30);
    assert(parsed->snapshot_rate == 10);
    assert(parsed->max_clients == 1);

    const char* unknown[] = {"stellar-server", "--bogus"};
    assert(!stellar::server::DedicatedServer::parse_cli(2, unknown));
    const char* missing_map[] = {"stellar-server", "--validate-config"};
    assert(!stellar::server::DedicatedServer::parse_cli(2, missing_map));
    const char* bad_rate[] = {"stellar-server", "--map", "room.bsp", "--tick-rate", "0"};
    assert(!stellar::server::DedicatedServer::parse_cli(5, bad_rate));
    const char* max_clients[] = {"stellar-server", "--map", "room.bsp", "--max-clients", "2"};
    assert(!stellar::server::DedicatedServer::parse_cli(5, max_clients));
}

void validate_only_and_map_failures() {
    const std::filesystem::path root = temp_root();
    const std::filesystem::path map = write_map(root, "room.bsp");
    stellar::server::DedicatedServerConfig config;
    config.map_path = map.string();
    config.validate_only = true;
    auto server = stellar::server::DedicatedServer::create(config);
    assert(server.has_value());
    assert(server->status().running);
    assert(!server->status().map_id.empty());

    stellar::server::DedicatedServerConfig missing = config;
    missing.map_path = (root / "missing.bsp").string();
    assert(!stellar::server::DedicatedServer::create(missing));

    stellar::server::DedicatedServerConfig non_bsp = config;
    non_bsp.map_path = (root / "map.txt").string();
    write_text(non_bsp.map_path, "not a bsp");
    assert(!stellar::server::DedicatedServer::create(non_bsp));

    stellar::server::DedicatedServerConfig invalid = config;
    invalid.map_path = (root / "invalid.bsp").string();
    write_text(invalid.map_path, "bad");
    assert(!stellar::server::DedicatedServer::create(invalid));
}

void missing_script_fails() {
    const std::filesystem::path root = temp_root();
    const std::filesystem::path map = write_map(root, "scripted.bsp", "playable");
    stellar::server::DedicatedServerConfig config;
    config.map_path = map.string();
    config.validate_only = true;
    auto server = stellar::server::DedicatedServer::create(config);
    assert(!server.has_value());
    assert(server.error().message.find("Missing script source") != std::string::npos);
}

stellar::server::DedicatedServer make_socket_server(const std::filesystem::path& map) {
    stellar::server::DedicatedServerConfig config;
    config.map_path = map.string();
    config.listen_endpoint = "127.0.0.1:0";
    config.tick_rate = 60;
    config.snapshot_rate = 60;
    auto server = stellar::server::DedicatedServer::create(config);
    assert(server.has_value());
    return std::move(*server);
}

std::unique_ptr<stellar::network::ClientTransport> connect_client(
    const stellar::server::DedicatedServerStatus& status) {
    auto client = stellar::network::connect_tcp_client(status.bound_endpoint);
    assert(client.has_value());
    return std::move(*client);
}

void send_hello(stellar::network::ClientTransport& client, std::string map_id) {
    auto bytes = stellar::network::encode_client_hello(stellar::network::ClientHello{
        .protocol_version = stellar::network::kCurrentProtocolVersion,
        .client_name = "dedicated_server_test",
        .requested_map_id = std::move(map_id),
        .client_nonce = 7,
    });
    assert(bytes.has_value());
    auto sent = client.send_to_server(stellar::network::TransportPacket{
        .channel = stellar::network::TransportChannel::kReliable,
        .payload = stellar::network::to_payload(*bytes),
    });
    assert(sent.has_value());
}

stellar::network::ServerWelcome expect_welcome(
    const std::vector<stellar::network::TransportPacket>& packets) {
    for (const auto& packet : packets) {
        auto welcome = stellar::network::decode_server_welcome(
            stellar::network::from_payload(packet.payload));
        if (welcome) {
            return *welcome;
        }
    }
    assert(false && "missing welcome");
    return {};
}

stellar::network::NetworkWorldSnapshot expect_snapshot(
    const std::vector<stellar::network::TransportPacket>& packets) {
    for (const auto& packet : packets) {
        auto snapshot = stellar::network::decode_snapshot(stellar::network::from_payload(packet.payload));
        if (snapshot) {
            return *snapshot;
        }
    }
    assert(false && "missing snapshot");
    return {};
}

void socket_hello_welcome_and_first_snapshot() {
    const std::filesystem::path root = temp_root();
    const std::filesystem::path map = write_map(root, "room.bsp");
    auto server = make_socket_server(map);
    auto client = connect_client(server.status());
    send_hello(*client, server.status().map_id);

    std::vector<stellar::network::TransportPacket> packets;
    pump_until_packets(server, *client, 0.0F, packets);
    const stellar::network::ServerWelcome welcome = expect_welcome(packets);
    assert(welcome.accepted);
    assert(welcome.assigned_player_id == 1);
    const stellar::network::NetworkWorldSnapshot snapshot = expect_snapshot(packets);
    assert(!snapshot.players.empty());
    assert(snapshot.tick == 0);
}

void input_updates_authoritative_snapshot() {
    const std::filesystem::path root = temp_root();
    const std::filesystem::path map = write_map(root, "room.bsp");
    auto server = make_socket_server(map);
    auto client = connect_client(server.status());
    send_hello(*client, server.status().map_id);
    std::vector<stellar::network::TransportPacket> packets;
    pump_until_packets(server, *client, 0.0F, packets);
    const auto first = expect_snapshot(packets);

    stellar::network::NetworkPlayerCommand command{};
    command.player_id = 999;
    command.command_sequence = 1;
    command.movement.wish_direction = {1.0F, 0.0F, 0.0F};
    command.movement.view_yaw_degrees = 90.0F;
    command.movement.has_view_angles = true;
    auto command_bytes = stellar::network::encode_player_command(command);
    assert(command_bytes.has_value());
    auto sent = client->send_to_server(stellar::network::TransportPacket{
        .channel = stellar::network::TransportChannel::kReliable,
        .payload = stellar::network::to_payload(*command_bytes),
    });
    assert(sent.has_value());

    packets.clear();
    for (int i = 0; i < 8; ++i) {
        auto pump = server.pump_once(1.0F / 60.0F);
        assert(pump.has_value());
        auto next = receive_for(*client, 1);
        packets.insert(packets.end(), std::make_move_iterator(next.begin()),
                       std::make_move_iterator(next.end()));
    }
    bool saw_delta = false;
    stellar::network::NetworkWorldSnapshot current = first;
    for (const auto& packet : packets) {
        auto delta = stellar::network::decode_snapshot_delta(
            stellar::network::from_payload(packet.payload));
        if (delta) {
            auto applied = stellar::network::apply_snapshot_delta(current, *delta);
            assert(applied.has_value());
            current = *applied;
            saw_delta = true;
        }
    }
    assert(saw_delta);
    assert(current.tick > first.tick);
    assert(current.players[0].position[0] > first.players[0].position[0]);
    assert(current.players[0].rotation != first.players[0].rotation);
}

void disconnect_does_not_crash() {
    const std::filesystem::path root = temp_root();
    const std::filesystem::path map = write_map(root, "room.bsp");
    auto server = make_socket_server(map);
    {
        auto client = connect_client(server.status());
        send_hello(*client, server.status().map_id);
        std::vector<stellar::network::TransportPacket> packets;
        pump_until_packets(server, *client, 0.0F, packets);
    }
    for (int i = 0; i < 4; ++i) {
        auto pump = server.pump_once(1.0F / 60.0F);
        assert(pump.has_value());
    }
}

} // namespace

int main() {
    config_parse_success_and_failure();
    validate_only_and_map_failures();
    missing_script_fails();
    socket_hello_welcome_and_first_snapshot();
    input_updates_authoritative_snapshot();
    disconnect_does_not_crash();
    return 0;
}
