#include "stellar/client/LocalServerBridge.hpp"
#include "stellar/network/Session.hpp"
#include "stellar/network/SnapshotCodec.hpp"
#include "stellar/network/Transport.hpp"
#include "stellar/world/RuntimeWorld.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

namespace {

using Vec3 = std::array<float, 3>;

stellar::assets::WorldMarker player_spawn(Vec3 position) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kPlayerSpawn;
    marker.name = "SPAWN_Player";
    marker.position = position;
    return marker;
}

stellar::assets::LevelAsset scene(std::string source_uri = "maps/session_test.bsp") {
    stellar::assets::LevelAsset level{};
    level.source_uri = std::move(source_uri);
    level.world_metadata.markers.push_back(player_spawn({0.0F, 0.0F, 0.0F}));
    return level;
}

stellar::client::LocalServerBridgeConfig bridge_config(const stellar::network::MapIdentity& map) {
    stellar::client::LocalServerBridgeConfig config{};
    config.session.local_player_id = 9;
    config.session.movement.fixed_dt = 0.1F;
    config.session.movement.gravity = 0.0F;
    config.map_identity = map;
    return config;
}

stellar::network::TransportPacket packet(std::vector<std::uint8_t> bytes) {
    return stellar::network::TransportPacket{.channel = stellar::network::TransportChannel::kReliable,
                                             .payload = stellar::network::to_payload(bytes)};
}

void client_hello_round_trips() {
    stellar::network::ClientHello hello{};
    hello.protocol_version = stellar::network::kCurrentProtocolVersion;
    hello.client_name = "tester";
    hello.requested_map_id = "session_test.bsp";
    hello.client_nonce = 42;

    const auto encoded = stellar::network::encode_client_hello(hello);
    assert(encoded.has_value());
    const auto decoded = stellar::network::decode_client_hello(*encoded);

    assert(decoded.has_value());
    assert(decoded->protocol_version == hello.protocol_version);
    assert(decoded->client_name == hello.client_name);
    assert(decoded->requested_map_id == hello.requested_map_id);
    assert(decoded->client_nonce == hello.client_nonce);
}

void welcome_round_trips_with_assigned_player() {
    stellar::network::ServerWelcome welcome{};
    welcome.accepted = true;
    welcome.protocol_version = stellar::network::kCurrentProtocolVersion;
    welcome.session_id = 11;
    welcome.assigned_player_id = 9;
    welcome.map_id = "session_test.bsp";
    welcome.message = "session accepted";

    const auto encoded = stellar::network::encode_server_welcome(welcome);
    assert(encoded.has_value());
    const auto decoded = stellar::network::decode_server_welcome(*encoded);

    assert(decoded.has_value());
    assert(decoded->accepted);
    assert(decoded->assigned_player_id == 9);
    assert(decoded->session_id == 11);
}

void malformed_session_packets_are_rejected() {
    const std::vector<std::uint8_t> malformed{1, 2, 3};
    assert(!stellar::network::decode_client_hello(malformed).has_value());
    assert(!stellar::network::decode_server_welcome(malformed).has_value());

    stellar::network::ClientHello oversized{};
    oversized.client_name.assign(8, 'x');
    const auto rejected = stellar::network::encode_client_hello(
        oversized, stellar::network::CodecLimits{.max_string_bytes = 4, .max_vector_count = 4});
    assert(!rejected.has_value());
}

void protocol_mismatch_rejects() {
    const auto level = scene();
    const auto world = stellar::world::build_runtime_world(level);
    auto transports = stellar::network::make_loopback_transport_pair();
    stellar::client::LocalServerBridge bridge(world,
                                              bridge_config(stellar::network::make_map_identity(world)));

    stellar::network::ClientHello hello{};
    hello.protocol_version = stellar::network::kCurrentProtocolVersion + 1;
    hello.requested_map_id = stellar::network::make_map_identity(world).map_id;
    const auto encoded = stellar::network::encode_client_hello(hello);
    assert(encoded.has_value());
    assert(transports.client->send_to_server(packet(*encoded)).has_value());

    const auto pump = bridge.pump(*transports.server, 0.1F);
    assert(pump.session_state == stellar::network::SessionState::kRejected);
    const auto replies = transports.client->receive_from_server();
    assert(replies.size() == 1);
    const auto welcome = stellar::network::decode_server_welcome(
        stellar::network::from_payload(replies[0].payload));
    assert(welcome.has_value());
    assert(!welcome->accepted);
    assert(welcome->rejection_code == "protocol_mismatch");
}

void map_mismatch_rejects() {
    const auto level = scene();
    const auto world = stellar::world::build_runtime_world(level);
    auto transports = stellar::network::make_loopback_transport_pair();
    stellar::client::LocalServerBridge bridge(world,
                                              bridge_config(stellar::network::make_map_identity(world)));

    stellar::network::ClientHello hello{};
    hello.requested_map_id = "wrong.bsp";
    const auto encoded = stellar::network::encode_client_hello(hello);
    assert(encoded.has_value());
    assert(transports.client->send_to_server(packet(*encoded)).has_value());

    const auto pump = bridge.pump(*transports.server, 0.1F);
    assert(pump.session_state == stellar::network::SessionState::kRejected);
    const auto replies = transports.client->receive_from_server();
    assert(replies.size() == 1);
    const auto welcome = stellar::network::decode_server_welcome(
        stellar::network::from_payload(replies[0].payload));
    assert(welcome.has_value());
    assert(welcome->rejection_code == "map_mismatch");
}

void input_before_welcome_is_rejected_without_snapshot() {
    const auto level = scene();
    const auto world = stellar::world::build_runtime_world(level);
    auto transports = stellar::network::make_loopback_transport_pair();
    stellar::client::LocalServerBridge bridge(world,
                                              bridge_config(stellar::network::make_map_identity(world)));

    stellar::network::NetworkPlayerCommand command{};
    command.player_id = 123;
    command.command_sequence = 1;
    const auto encoded = stellar::network::encode_player_command(command);
    assert(encoded.has_value());
    assert(transports.client->send_to_server(packet(*encoded)).has_value());

    const auto pump = bridge.pump(*transports.server, 0.1F);
    assert(pump.rejected_packets == 1);
    assert(pump.errors[0].code == "input_before_welcome");
    assert(transports.client->receive_from_server().empty());
}

void accepted_welcome_then_first_snapshot_and_assignment() {
    const auto level = scene();
    const auto world = stellar::world::build_runtime_world(level);
    auto transports = stellar::network::make_loopback_transport_pair();
    const auto identity = stellar::network::make_map_identity(world);
    stellar::client::LocalServerBridge bridge(world, bridge_config(identity));

    stellar::network::ClientHello hello{};
    hello.requested_map_id = identity.map_id;
    const auto encoded = stellar::network::encode_client_hello(hello);
    assert(encoded.has_value());
    assert(transports.client->send_to_server(packet(*encoded)).has_value());

    const auto pump = bridge.pump(*transports.server, 0.1F);
    assert(pump.session_state == stellar::network::SessionState::kConnected);
    const auto replies = transports.client->receive_from_server();
    assert(replies.size() == 2);
    const auto welcome = stellar::network::decode_server_welcome(
        stellar::network::from_payload(replies[0].payload));
    assert(welcome.has_value());
    assert(welcome->accepted);
    assert(welcome->assigned_player_id == 9);
    const auto snapshot = stellar::network::decode_snapshot(
        stellar::network::from_payload(replies[1].payload));
    assert(snapshot.has_value());
    assert(snapshot->players[0].player_id == 9);
}

void map_identity_uses_basename_and_deterministic_hash() {
    const auto identity = stellar::network::make_map_identity("assets/maps/arena.bsp");
    assert(identity.map_id == "arena.bsp");
    assert(identity.source_uri == "assets/maps/arena.bsp");
    assert(identity.content_hash == stellar::network::deterministic_identity_hash(identity.source_uri));
}

} // namespace

int main() {
    client_hello_round_trips();
    welcome_round_trips_with_assigned_player();
    malformed_session_packets_are_rejected();
    protocol_mismatch_rejects();
    map_mismatch_rejects();
    input_before_welcome_is_rejected_without_snapshot();
    accepted_welcome_then_first_snapshot_and_assignment();
    map_identity_uses_basename_and_deterministic_hash();
    return 0;
}
