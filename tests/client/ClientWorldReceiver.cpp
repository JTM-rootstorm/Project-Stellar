#include "stellar/client/ClientWorldReceiver.hpp"
#include "stellar/client/LocalServerBridge.hpp"
#include "stellar/network/SnapshotCodec.hpp"
#include "stellar/network/SnapshotDelta.hpp"
#include "stellar/network/Transport.hpp"
#include "stellar/scripting/ScriptRegistry.hpp"
#include "stellar/scripting/ScriptedWorldSession.hpp"
#include "stellar/world/RuntimeWorld.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

namespace {

using Vec3 = std::array<float, 3>;

stellar::assets::WorldMarker player_spawn(Vec3 position) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kPlayerSpawn;
    marker.name = "SPAWN_Player";
    marker.position = position;
    return marker;
}

stellar::assets::WorldMarker trigger_marker(std::string name,
                                            Vec3 position,
                                            Vec3 scale,
                                            std::string script_id,
                                            std::string table_name) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kTrigger;
    marker.name = std::move(name);
    marker.position = position;
    marker.scale = scale;
    marker.script = stellar::assets::WorldScriptBinding{std::move(script_id), std::move(table_name)};
    return marker;
}

stellar::assets::CollisionTriangle triangle(Vec3 a, Vec3 b, Vec3 c, Vec3 normal) {
    return stellar::assets::CollisionTriangle{.a = a, .b = b, .c = c, .normal = normal};
}

stellar::assets::LevelAsset scene_with_markers(
    std::initializer_list<stellar::assets::WorldMarker> markers) {
    stellar::assets::LevelAsset scene{};
    scene.world_metadata.markers.assign(markers.begin(), markers.end());
    return scene;
}

stellar::assets::LevelAsset scene_with_trigger_and_wall() {
    auto scene = scene_with_markers({
        player_spawn({0.0F, 0.5F, 0.0F}),
        trigger_marker("DoorOpen", {0.0F, 0.5F, 0.0F}, {0.5F, 0.5F, 0.5F}, "door", "Door"),
    });
    stellar::assets::CollisionMesh wall;
    wall.name = "DoorBlocker";
    wall.triangles = {
        triangle({0.8F, -2.0F, -4.0F}, {0.8F, 4.0F, 4.0F}, {0.8F, -2.0F, 4.0F},
                 {-1.0F, 0.0F, 0.0F}),
        triangle({0.8F, -2.0F, -4.0F}, {0.8F, 4.0F, -4.0F}, {0.8F, 4.0F, 4.0F},
                 {-1.0F, 0.0F, 0.0F}),
    };
    scene.level_collision = stellar::assets::LevelCollisionAsset{.meshes = {wall}};
    return scene;
}

stellar::server::WorldSessionConfig test_session_config() {
    stellar::server::WorldSessionConfig config{};
    config.local_player_id = 7;
    config.movement.max_speed = 10.0F;
    config.movement.acceleration = 100.0F;
    config.movement.gravity = 0.0F;
    config.movement.terminal_fall_speed = 50.0F;
    config.movement.fixed_dt = 0.1F;
    config.movement.character.radius = 0.25F;
    config.movement.character.height = 1.0F;
    config.movement.character.skin_width = 0.0F;
    config.movement.character.ground_snap_distance = 0.0F;
    config.movement.character.max_slide_iterations = 4;
    return config;
}

stellar::network::TransportPacket reliable_packet(std::vector<std::uint8_t> bytes) {
    return stellar::network::TransportPacket{.channel = stellar::network::TransportChannel::kReliable,
                                             .payload = stellar::network::to_payload(bytes)};
}

void accept_local_session(stellar::client::LocalServerBridge& bridge,
                          stellar::network::LoopbackTransportPair& transport) {
    stellar::network::ClientHello hello{};
    hello.requested_map_id = "local";
    auto encoded = stellar::network::encode_client_hello(hello);
    assert(encoded.has_value());
    assert(transport.client->send_to_server(reliable_packet(*encoded)).has_value());
    const auto pump = bridge.pump(*transport.server, 0.0F);
    assert(pump.session_state == stellar::network::SessionState::kConnected);
    const auto replies = transport.client->receive_from_server();
    assert(!replies.empty());
    assert(stellar::network::decode_server_welcome(
               stellar::network::from_payload(replies[0].payload))
               .has_value());
}

stellar::network::NetworkWorldSnapshot snapshot(std::uint64_t tick, float x) {
    stellar::network::NetworkWorldSnapshot result{};
    result.tick = tick;
    result.players.push_back(stellar::network::PlayerSnapshot{.player_id = 7,
                                                              .position = {x, 0.0F, 0.0F},
                                                              .velocity = {},
                                                              .rotation = {0.0F, 0.0F, 0.0F, 1.0F},
                                                              .grounded = true});
    return result;
}

void server_snapshot_decodes_into_receiver() {
    stellar::client::ClientWorldReceiver receiver;
    auto encoded = stellar::network::encode_snapshot(snapshot(3, 1.0F));
    assert(encoded.has_value());

    const auto result = receiver.accept_packet(reliable_packet(*encoded));

    assert(result.snapshots == 1);
    assert(result.rejected_packets == 0);
    assert(receiver.has_snapshot());
    assert(receiver.latest_snapshot()->tick == 3);
}

void delta_applies_to_receiver_baseline() {
    stellar::client::ClientWorldReceiver receiver;
    const auto baseline = snapshot(1, 1.0F);
    const auto target = snapshot(2, 2.5F);
    auto encoded_baseline = stellar::network::encode_snapshot(baseline);
    auto encoded_delta = stellar::network::encode_snapshot_delta(
        stellar::network::make_snapshot_delta(baseline, target));
    assert(encoded_baseline.has_value());
    assert(encoded_delta.has_value());

    assert(receiver.accept_packet(reliable_packet(*encoded_baseline)).snapshots == 1);
    const auto result = receiver.accept_packet(reliable_packet(*encoded_delta));

    assert(result.deltas == 1);
    assert(receiver.latest_snapshot()->tick == 2);
    assert(receiver.latest_snapshot()->players[0].position[0] == 2.5F);
}

void gameplay_event_queues_for_presentation() {
    stellar::client::ClientWorldReceiver receiver;
    stellar::network::GameplayEvent event{.kind = stellar::network::GameplayEventKind::kPickupCollected,
                                          .tick = 9,
                                          .entity_id = 44,
                                          .player_id = 7,
                                          .code = "pickup",
                                          .message = "collected"};
    auto encoded = stellar::network::encode_gameplay_event(event);
    assert(encoded.has_value());

    const auto result = receiver.accept_packet(reliable_packet(*encoded));

    assert(result.events == 1);
    assert(receiver.queued_events().size() == 1);
    assert(receiver.queued_events()[0].entity_id == 44);
    auto taken = receiver.take_queued_events();
    assert(taken.size() == 1);
    assert(receiver.queued_events().empty());
}

void local_bridge_emits_snapshot_after_input_command() {
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::LocalServerBridgeConfig config{};
    config.session = test_session_config();
    config.emit_deltas = false;
    stellar::client::LocalServerBridge bridge(world, config);
    stellar::client::ClientWorldReceiver receiver;
    auto transport = stellar::network::make_loopback_transport_pair();
    accept_local_session(bridge, transport);

    stellar::network::NetworkPlayerCommand command{};
    command.player_id = 999;
    command.command_sequence = 1;
    command.movement.wish_direction = {0.0F, 1.0F, 99.0F};
    auto encoded = stellar::network::encode_player_command(command);
    assert(encoded.has_value());
    assert(transport.client->send_to_server(reliable_packet(*encoded)).has_value());

    const auto pump = bridge.pump(*transport.server, 0.1F);
    const auto drain = receiver.drain(*transport.client);

    assert(pump.ticks_run == 1);
    assert(pump.rejected_packets == 0);
    assert(drain.snapshots == 1);
    assert(receiver.latest_snapshot()->tick == 1);
    assert(receiver.latest_snapshot()->players[0].player_id == 7);
    assert(receiver.latest_snapshot()->players[0].position[0] == 0.0F);
    assert(receiver.latest_snapshot()->players[0].position[1] > 0.9F);
    assert(receiver.latest_snapshot()->players[0].position[2] == 0.0F);
}

void scripted_bridge_propagates_server_approved_event() {
    const auto scene = scene_with_trigger_and_wall();
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::scripting::ScriptRegistry registry;
    registry.set_script("door",
                        "Door = {}\n"
                        "function Door.on_trigger_enter(event)\n"
                        "  stellar.emit_event('collision.set_mesh_enabled', "
                        "{mesh = 'DoorBlocker', enabled = false})\n"
                        "end\n");
    auto scripted = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                     std::move(registry));
    assert(scripted.has_value());
    stellar::client::LocalServerBridgeConfig config{};
    config.session = test_session_config();
    config.emit_deltas = false;
    stellar::client::LocalServerBridge bridge(std::move(*scripted), config);
    stellar::client::ClientWorldReceiver receiver;
    auto transport = stellar::network::make_loopback_transport_pair();
    accept_local_session(bridge, transport);

    const auto pump = bridge.pump(*transport.server, 0.1F);
    const auto drain = receiver.drain(*transport.client);

    assert(pump.ticks_run == 1);
    assert(pump.rejected_packets == 0);
    assert(drain.snapshots == 1);
    assert(drain.events == 1);
    assert(receiver.queued_events().size() == 1);
    assert(receiver.queued_events()[0].kind == stellar::network::GameplayEventKind::kDoorStateChanged);
    assert(receiver.queued_events()[0].code == "DoorBlocker");
}

void malformed_packets_rejected_without_crash() {
    stellar::client::ClientWorldReceiver receiver;
    const auto result = receiver.accept_packet(reliable_packet({0x01, 0x02, 0x03}));
    assert(result.rejected_packets == 1);
    assert(!receiver.has_snapshot());

    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::client::LocalServerBridgeConfig config{};
    config.session = test_session_config();
    stellar::client::LocalServerBridge bridge(world, config);
    auto transport = stellar::network::make_loopback_transport_pair();
    assert(transport.client->send_to_server(reliable_packet({0xAA, 0xBB})).has_value());
    const auto pump = bridge.pump(*transport.server, 0.0F);
    assert(pump.rejected_packets == 1);
}

} // namespace

int main() {
    server_snapshot_decodes_into_receiver();
    delta_applies_to_receiver_baseline();
    gameplay_event_queues_for_presentation();
    local_bridge_emits_snapshot_after_input_command();
    scripted_bridge_propagates_server_approved_event();
    malformed_packets_rejected_without_crash();
    return 0;
}
