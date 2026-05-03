#include "stellar/network/SnapshotCodec.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

stellar::server::PlayerSnapshot player(std::uint32_t id) {
    return stellar::server::PlayerSnapshot{.player_id = id,
                                           .position = {1.0F, 2.0F, 3.0F},
                                           .velocity = {4.0F, 5.0F, 6.0F},
                                           .rotation = {0.0F, 0.0F, 0.0F, 1.0F},
                                           .grounded = true};
}

stellar::network::NetworkGameplayEntity entity(std::uint32_t id,
                                                stellar::server::EntityKind kind,
                                                std::string name) {
    stellar::network::NetworkGameplayEntity result{};
    result.id = id;
    result.kind = kind;
    result.transform.position = {static_cast<float>(id), 2.0F, 3.0F};
    result.transform.rotation = {0.0F, 0.0F, 0.0F, 1.0F};
    result.transform.scale = {16.0F, 24.0F, 1.0F};
    result.metadata.name = std::move(name);
    result.metadata.archetype = "pickup_ammo";
    result.metadata.sprite_id = "sprites/test";
    result.metadata.source_type = "object_collider";
    result.metadata.extras_json = "{\"stable\":true}";
    result.metadata.size = {8.0F, 8.0F, 8.0F};
    result.metadata.alpha = 0.75F;
    result.metadata.object_collider_id = id + 100;
    result.metadata.collision_mesh_index = id + 200;
    result.active = true;
    result.open = kind == stellar::server::EntityKind::kDoor;
    return result;
}

bool same_snapshot(const stellar::network::NetworkWorldSnapshot& lhs,
                   const stellar::network::NetworkWorldSnapshot& rhs) {
    if (lhs.tick != rhs.tick || lhs.players.size() != rhs.players.size() ||
        lhs.entities.size() != rhs.entities.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.players.size(); ++i) {
        if (lhs.players[i].player_id != rhs.players[i].player_id ||
            lhs.players[i].position != rhs.players[i].position ||
            lhs.players[i].velocity != rhs.players[i].velocity ||
            lhs.players[i].rotation != rhs.players[i].rotation ||
            lhs.players[i].grounded != rhs.players[i].grounded) {
            return false;
        }
    }
    for (std::size_t i = 0; i < lhs.entities.size(); ++i) {
        const auto& a = lhs.entities[i];
        const auto& b = rhs.entities[i];
        if (a.id != b.id || a.kind != b.kind || a.transform.position != b.transform.position ||
            a.transform.rotation != b.transform.rotation || a.transform.scale != b.transform.scale ||
            a.metadata.name != b.metadata.name || a.metadata.archetype != b.metadata.archetype ||
            a.metadata.sprite_id != b.metadata.sprite_id ||
            a.metadata.source_type != b.metadata.source_type ||
            a.metadata.extras_json != b.metadata.extras_json ||
            a.metadata.size != b.metadata.size || a.metadata.alpha != b.metadata.alpha ||
            a.metadata.object_collider_id != b.metadata.object_collider_id ||
            a.metadata.collision_mesh_index != b.metadata.collision_mesh_index ||
            a.active != b.active || a.open != b.open) {
            return false;
        }
    }
    return true;
}

void snapshot_round_trip_with_player_and_no_entities() {
    stellar::network::NetworkWorldSnapshot snapshot{};
    snapshot.tick = 7;
    snapshot.players.push_back(player(1));

    auto encoded = stellar::network::encode_snapshot(snapshot);
    assert(encoded.has_value());
    auto decoded = stellar::network::decode_snapshot(*encoded);
    assert(decoded.has_value());
    assert(same_snapshot(snapshot, *decoded));
}

void snapshot_round_trip_with_sprite_pickup_and_door_entities() {
    stellar::network::NetworkWorldSnapshot snapshot{};
    snapshot.tick = 8;
    snapshot.players.push_back(player(2));
    snapshot.entities.push_back(entity(10, stellar::server::EntityKind::kSprite, "sprite"));
    snapshot.entities.push_back(entity(11, stellar::server::EntityKind::kPickup, "pickup"));
    snapshot.entities.push_back(entity(12, stellar::server::EntityKind::kDoor, "door"));

    auto encoded = stellar::network::encode_snapshot(snapshot);
    assert(encoded.has_value());
    auto decoded = stellar::network::decode_snapshot(*encoded);
    assert(decoded.has_value());
    assert(same_snapshot(snapshot, *decoded));
}

void gameplay_event_round_trip() {
    stellar::network::GameplayEvent event{.kind = stellar::network::GameplayEventKind::kPickupCollected,
                                          .tick = 42,
                                          .entity_id = 99,
                                          .player_id = 1,
                                          .code = "pickup",
                                          .message = "collected"};
    auto encoded = stellar::network::encode_gameplay_event(event);
    assert(encoded.has_value());
    auto decoded = stellar::network::decode_gameplay_event(*encoded);
    assert(decoded.has_value());
    assert(decoded->kind == event.kind);
    assert(decoded->tick == event.tick);
    assert(decoded->entity_id == event.entity_id);
    assert(decoded->player_id == event.player_id);
    assert(decoded->code == event.code);
    assert(decoded->message == event.message);
}

void player_command_round_trip_includes_view_angles() {
    stellar::network::NetworkPlayerCommand command{};
    command.player_id = 3;
    command.command_sequence = 7;
    command.movement.wish_direction = {0.25F, 0.5F, 0.0F};
    command.movement.jump = true;
    command.movement.view_yaw_degrees = 123.0F;
    command.movement.view_pitch_degrees = -45.0F;
    command.movement.has_view_angles = true;

    auto encoded = stellar::network::encode_player_command(command);
    assert(encoded.has_value());
    auto decoded = stellar::network::decode_player_command(*encoded);
    assert(decoded.has_value());

    assert(decoded->player_id == command.player_id);
    assert(decoded->command_sequence == command.command_sequence);
    assert(decoded->movement.wish_direction == command.movement.wish_direction);
    assert(decoded->movement.jump == command.movement.jump);
    assert(decoded->movement.view_yaw_degrees == command.movement.view_yaw_degrees);
    assert(decoded->movement.view_pitch_degrees == command.movement.view_pitch_degrees);
    assert(decoded->movement.has_view_angles == command.movement.has_view_angles);
}

void invalid_and_truncated_data_fail_cleanly() {
    assert(!stellar::network::decode_snapshot({}).has_value());
    auto encoded = stellar::network::encode_snapshot(stellar::network::NetworkWorldSnapshot{});
    assert(encoded.has_value());
    encoded->pop_back();
    assert(!stellar::network::decode_snapshot(*encoded).has_value());
}

void oversized_string_and_vector_fail_cleanly() {
    stellar::network::GameplayEvent event{};
    event.code.assign(9, 'x');
    assert(!stellar::network::encode_gameplay_event(
                event, stellar::network::CodecLimits{.max_string_bytes = 8}).has_value());

    std::vector<std::uint8_t> bytes = {0x33, 0x4E, 0x50, 0x53, 0x02, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
    assert(!stellar::network::decode_snapshot(
                bytes, stellar::network::CodecLimits{.max_vector_count = 1}).has_value());
}

void non_finite_float_rejected() {
    stellar::network::NetworkWorldSnapshot snapshot{};
    snapshot.players.push_back(player(1));
    snapshot.players[0].position[0] = std::numeric_limits<float>::infinity();
    assert(!stellar::network::encode_snapshot(snapshot).has_value());

    stellar::network::NetworkPlayerCommand command{};
    command.movement.view_yaw_degrees = std::numeric_limits<float>::infinity();
    command.movement.has_view_angles = true;
    assert(!stellar::network::encode_player_command(command).has_value());

    auto clean = stellar::network::encode_snapshot(stellar::network::NetworkWorldSnapshot{});
    assert(clean.has_value());
    clean->push_back(0x00);
    assert(!stellar::network::decode_snapshot(*clean).has_value());
}

void deterministic_byte_output_for_identical_input() {
    stellar::network::NetworkWorldSnapshot snapshot{};
    snapshot.tick = 99;
    snapshot.players.push_back(player(5));
    snapshot.entities.push_back(entity(20, stellar::server::EntityKind::kPickup, "pickup"));
    auto first = stellar::network::encode_snapshot(snapshot);
    auto second = stellar::network::encode_snapshot(snapshot);
    assert(first.has_value());
    assert(second.has_value());
    assert(*first == *second);
}

} // namespace

int main() {
    snapshot_round_trip_with_player_and_no_entities();
    snapshot_round_trip_with_sprite_pickup_and_door_entities();
    gameplay_event_round_trip();
    player_command_round_trip_includes_view_angles();
    invalid_and_truncated_data_fail_cleanly();
    oversized_string_and_vector_fail_cleanly();
    non_finite_float_rejected();
    deterministic_byte_output_for_identical_input();
    return 0;
}
