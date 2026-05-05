#include "stellar/server/ServerRuntime.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "stellar/authority/NetworkConversion.hpp"
#include "stellar/network/SnapshotCodec.hpp"
#include "stellar/network/Transport.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace {

using Vec3 = std::array<float, 3>;

stellar::assets::WorldMarker player_spawn(Vec3 position) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kPlayerSpawn;
    marker.name = "SPAWN_Player";
    marker.position = position;
    return marker;
}

stellar::assets::LevelAsset test_level() {
    stellar::assets::LevelAsset level{};
    level.source_uri = "maps/server_runtime_test.bsp";
    level.world_metadata.markers.push_back(player_spawn({0.0F, 0.0F, 0.0F}));
    return level;
}

stellar::assets::CollisionTriangle triangle(Vec3 a, Vec3 b, Vec3 c, Vec3 normal) {
    return stellar::assets::CollisionTriangle{.a = a, .b = b, .c = c, .normal = normal};
}

stellar::assets::LevelAsset footstep_level() {
    stellar::assets::LevelAsset level{};
    level.source_uri = "maps/server_runtime_test.bsp";
    level.world_metadata.markers.push_back(player_spawn({0.0F, 0.0F, 0.55F}));
    auto floor_a = triangle({-10.0F, -10.0F, 0.0F}, {10.0F, -10.0F, 0.0F},
                            {10.0F, 10.0F, 0.0F}, {0.0F, 0.0F, 1.0F});
    auto floor_b = triangle({-10.0F, -10.0F, 0.0F}, {10.0F, 10.0F, 0.0F},
                            {-10.0F, 10.0F, 0.0F}, {0.0F, 0.0F, 1.0F});
    floor_a.surface.source_material_name = "metal/grate01";
    floor_a.surface.footstep_surface_id = "metal";
    floor_b.surface = floor_a.surface;
    stellar::assets::CollisionMesh floor;
    floor.name = "Floor";
    floor.triangles = {floor_a, floor_b};
    level.level_collision = stellar::assets::LevelCollisionAsset{.meshes = {floor}};
    return level;
}

stellar::server::ServerRuntimeConfig test_config(const stellar::world::RuntimeWorld& world) {
    stellar::server::ServerRuntimeConfig config{};
    config.session.local_player_id = 9;
    config.session.movement.fixed_dt = 0.1F;
    config.session.movement.gravity = 0.0F;
    config.max_ticks_per_pump = 2;
    config.emit_deltas = true;
    config.map_identity = stellar::network::make_map_identity("maps/server_runtime_test.bsp");
    return config;
}

stellar::server::ServerRuntimeConfig footstep_config(const stellar::world::RuntimeWorld& world) {
    auto config = test_config(world);
    config.session.movement.max_speed = 10.0F;
    config.session.movement.acceleration = 100.0F;
    config.session.movement.gravity = 24.0F;
    config.session.movement.character.radius = 0.25F;
    config.session.movement.character.height = 1.0F;
    config.session.movement.character.ground_snap_distance = 0.2F;
    config.session.footsteps.min_horizontal_speed = 0.0F;
    config.session.footsteps.walk_step_distance = 0.01F;
    config.session.footsteps.run_step_distance = 0.01F;
    config.max_ticks_per_pump = 4;
    config.emit_deltas = false;
    return config;
}

stellar::network::TransportPacket packet(std::vector<std::uint8_t> bytes) {
    return stellar::network::TransportPacket{.channel = stellar::network::TransportChannel::kReliable,
                                             .payload = stellar::network::to_payload(bytes)};
}

void send_hello(stellar::network::ClientTransport& client, std::string map_id) {
    stellar::network::ClientHello hello{};
    hello.protocol_version = stellar::network::kCurrentProtocolVersion;
    hello.requested_map_id = std::move(map_id);
    auto encoded = stellar::network::encode_client_hello(hello);
    assert(encoded.has_value());
    assert(client.send_to_server(packet(*encoded)).has_value());
}

stellar::network::ServerWelcome expect_welcome(
    const std::vector<stellar::network::TransportPacket>& packets) {
    for (const auto& reply : packets) {
        auto welcome = stellar::network::decode_server_welcome(
            stellar::network::from_payload(reply.payload));
        if (welcome) {
            return *welcome;
        }
    }
    assert(false && "missing welcome");
    return {};
}

void accepted_hello_assigns_player_and_sends_first_full_snapshot() {
    const auto level = test_level();
    const auto world = stellar::world::build_runtime_world(level);
    auto transports = stellar::network::make_loopback_transport_pair();
    stellar::server::ServerRuntime runtime(world, test_config(world));

    send_hello(*transports.client, test_config(world).map_identity.map_id);
    const auto pump = runtime.pump(*transports.server, 0.0F);
    const auto replies = transports.client->receive_from_server();

    assert(pump.session_state == stellar::network::SessionState::kConnected);
    assert(runtime.connected_clients() == 1);
    const auto welcome = expect_welcome(replies);
    assert(welcome.accepted);
    assert(welcome.assigned_player_id == 9);
    bool saw_snapshot = false;
    for (const auto& reply : replies) {
        auto snapshot = stellar::network::decode_snapshot(stellar::network::from_payload(reply.payload));
        if (snapshot) {
            saw_snapshot = true;
            assert(snapshot->tick == 0);
            assert(snapshot->players[0].player_id == 9);
        }
    }
    assert(saw_snapshot);
}

void rejects_protocol_map_and_premature_input() {
    const auto level = test_level();
    const auto world = stellar::world::build_runtime_world(level);
    {
        auto transports = stellar::network::make_loopback_transport_pair();
        stellar::server::ServerRuntime runtime(world, test_config(world));
        stellar::network::ClientHello hello{};
        hello.protocol_version = stellar::network::kCurrentProtocolVersion + 1;
        auto encoded = stellar::network::encode_client_hello(hello);
        assert(encoded.has_value());
        assert(transports.client->send_to_server(packet(*encoded)).has_value());
        const auto pump = runtime.pump(*transports.server, 0.0F);
        const auto welcome = expect_welcome(transports.client->receive_from_server());
        assert(pump.session_state == stellar::network::SessionState::kRejected);
        assert(!welcome.accepted);
        assert(welcome.rejection_code == "protocol_mismatch");
    }
    {
        auto transports = stellar::network::make_loopback_transport_pair();
        stellar::server::ServerRuntime runtime(world, test_config(world));
        send_hello(*transports.client, "wrong.bsp");
        const auto pump = runtime.pump(*transports.server, 0.0F);
        const auto welcome = expect_welcome(transports.client->receive_from_server());
        assert(pump.session_state == stellar::network::SessionState::kRejected);
        assert(welcome.rejection_code == "map_mismatch");
    }
    {
        auto transports = stellar::network::make_loopback_transport_pair();
        stellar::server::ServerRuntime runtime(world, test_config(world));
        stellar::network::NetworkPlayerCommand command{};
        auto encoded = stellar::network::encode_player_command(command);
        assert(encoded.has_value());
        assert(transports.client->send_to_server(packet(*encoded)).has_value());
        const auto pump = runtime.pump(*transports.server, 0.1F);
        assert(pump.rejected_packets == 1);
        assert(pump.errors[0].code == "input_before_welcome");
        assert(transports.client->receive_from_server().empty());
    }
}

void second_client_is_rejected_while_active() {
    const auto level = test_level();
    const auto world = stellar::world::build_runtime_world(level);
    auto transports = stellar::network::make_loopback_transport_pair();
    stellar::server::ServerRuntime runtime(world, test_config(world));

    send_hello(*transports.client, test_config(world).map_identity.map_id);
    assert(runtime.pump(*transports.server, 0.0F).session_state ==
           stellar::network::SessionState::kConnected);
    static_cast<void>(transports.client->receive_from_server());

    send_hello(*transports.client, test_config(world).map_identity.map_id);
    const auto pump = runtime.pump(*transports.server, 0.0F);
    const auto welcome = expect_welcome(transports.client->receive_from_server());
    assert(pump.session_state == stellar::network::SessionState::kConnected);
    assert(!welcome.accepted);
    assert(welcome.rejection_code == "server_full");
}

void server_runtime_forwards_footstep_event_packets() {
    const auto level = footstep_level();
    const auto world = stellar::world::build_runtime_world(level);
    auto transports = stellar::network::make_loopback_transport_pair();
    stellar::server::ServerRuntime runtime(world, footstep_config(world));

    send_hello(*transports.client, footstep_config(world).map_identity.map_id);
    assert(runtime.pump(*transports.server, 0.0F).session_state ==
           stellar::network::SessionState::kConnected);
    static_cast<void>(transports.client->receive_from_server());

    stellar::network::NetworkPlayerCommand command{};
    command.player_id = 9;
    command.movement.wish_direction = {1.0F, 0.0F, 0.0F};
    auto encoded = stellar::network::encode_player_command(command);
    assert(encoded.has_value());
    assert(transports.client->send_to_server(packet(*encoded)).has_value());

    const auto pump = runtime.pump(*transports.server, 0.3F);
    const auto replies = transports.client->receive_from_server();

    assert(pump.ticks_run > 0);
    bool saw_footstep = false;
    for (const auto& reply : replies) {
        auto event = stellar::network::decode_gameplay_event(
            stellar::network::from_payload(reply.payload));
        if (event && event->kind == stellar::network::GameplayEventKind::kFootstep &&
            event->code == "metal") {
            saw_footstep = true;
        }
    }
    assert(saw_footstep);
}

} // namespace

int main() {
    accepted_hello_assigns_player_and_sends_first_full_snapshot();
    rejects_protocol_map_and_premature_input();
    second_client_is_rejected_while_active();
    server_runtime_forwards_footstep_event_packets();
    return 0;
}
