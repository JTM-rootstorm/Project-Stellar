#include "stellar/network/SnapshotCodec.hpp"
#include "stellar/network/SnapshotDelta.hpp"

#include <cassert>
#include <cstdint>
#include <string>

namespace {

stellar::network::PlayerSnapshot player(std::uint32_t id, float x) {
    return stellar::network::PlayerSnapshot{.player_id = id,
                                           .position = {x, 2.0F, 3.0F},
                                           .velocity = {0.0F, 1.0F, 0.0F},
                                           .rotation = {0.0F, 0.0F, 0.0F, 1.0F},
                                           .grounded = true};
}

stellar::network::NetworkGameplayEntity entity(std::uint32_t id, std::string name, bool active) {
    stellar::network::NetworkGameplayEntity result{};
    result.id = id;
    result.kind = stellar::network::EntityKind::kPickup;
    result.transform.position = {static_cast<float>(id), 0.0F, 0.0F};
    result.transform.rotation = {0.0F, 0.0F, 0.0F, 1.0F};
    result.transform.scale = {1.0F, 1.0F, 1.0F};
    result.metadata.name = std::move(name);
    result.metadata.sprite_id = "sprite";
    result.metadata.alpha = 1.0F;
    result.active = active;
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
            a.metadata.name != b.metadata.name || a.metadata.sprite_id != b.metadata.sprite_id ||
            a.metadata.alpha != b.metadata.alpha || a.active != b.active || a.open != b.open) {
            return false;
        }
    }
    return true;
}

void delta_round_trip_and_apply_reproduces_target() {
    stellar::network::NetworkWorldSnapshot baseline{};
    baseline.tick = 10;
    baseline.players.push_back(player(1, 1.0F));
    baseline.entities.push_back(entity(1, "removed", true));
    baseline.entities.push_back(entity(2, "updated", true));

    stellar::network::NetworkWorldSnapshot target{};
    target.tick = 11;
    target.players.push_back(player(1, 9.0F));
    target.entities.push_back(entity(2, "updated", false));
    target.entities.push_back(entity(3, "added", true));

    const stellar::network::SnapshotDelta delta =
        stellar::network::make_snapshot_delta(baseline, target);
    assert(delta.baseline_tick == 10);
    assert(delta.target_tick == 11);
    assert(delta.added_entities.size() == 1);
    assert(delta.updated_entities.size() == 1);
    assert(delta.removed_entity_ids.size() == 1);
    assert(delta.updated_players.size() == 1);

    auto encoded = stellar::network::encode_snapshot_delta(delta);
    assert(encoded.has_value());
    auto decoded = stellar::network::decode_snapshot_delta(*encoded);
    assert(decoded.has_value());
    auto applied = stellar::network::apply_snapshot_delta(baseline, *decoded);
    assert(applied.has_value());
    assert(same_snapshot(target, *applied));
}

void delta_apply_rejects_wrong_baseline() {
    stellar::network::NetworkWorldSnapshot baseline{};
    baseline.tick = 1;
    stellar::network::SnapshotDelta delta{};
    delta.baseline_tick = 2;
    delta.target_tick = 3;
    assert(!stellar::network::apply_snapshot_delta(baseline, delta).has_value());
}

void deterministic_delta_bytes_for_identical_input() {
    stellar::network::NetworkWorldSnapshot baseline{};
    baseline.tick = 1;
    baseline.entities.push_back(entity(1, "a", true));
    stellar::network::NetworkWorldSnapshot target = baseline;
    target.tick = 2;
    target.entities[0].active = false;

    const auto delta = stellar::network::make_snapshot_delta(baseline, target);
    auto first = stellar::network::encode_snapshot_delta(delta);
    auto second = stellar::network::encode_snapshot_delta(delta);
    assert(first.has_value());
    assert(second.has_value());
    assert(*first == *second);
}

} // namespace

int main() {
    delta_round_trip_and_apply_reproduces_target();
    delta_apply_rejects_wrong_baseline();
    deterministic_delta_bytes_for_identical_input();
    return 0;
}
