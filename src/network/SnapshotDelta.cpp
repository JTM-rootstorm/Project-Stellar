#include "stellar/network/SnapshotDelta.hpp"

#include <algorithm>

namespace stellar::network {
namespace {

[[nodiscard]] bool same_transform(const TransformComponent& lhs,
                                  const TransformComponent& rhs) noexcept {
    return lhs.position == rhs.position && lhs.rotation == rhs.rotation && lhs.scale == rhs.scale;
}

[[nodiscard]] bool same_metadata(const GameplayEntityMetadata& lhs,
                                 const GameplayEntityMetadata& rhs) noexcept {
    return lhs.name == rhs.name && lhs.archetype == rhs.archetype &&
           lhs.sprite_id == rhs.sprite_id && lhs.source_type == rhs.source_type &&
           lhs.extras_json == rhs.extras_json && lhs.size == rhs.size && lhs.alpha == rhs.alpha &&
           lhs.object_collider_id == rhs.object_collider_id &&
           lhs.collision_mesh_index == rhs.collision_mesh_index;
}

[[nodiscard]] bool same_entity(const NetworkGameplayEntity& lhs,
                               const NetworkGameplayEntity& rhs) noexcept {
    return lhs.id == rhs.id && lhs.kind == rhs.kind && same_transform(lhs.transform, rhs.transform) &&
           same_metadata(lhs.metadata, rhs.metadata) && lhs.active == rhs.active &&
           lhs.open == rhs.open;
}

[[nodiscard]] bool same_player(const PlayerSnapshot& lhs,
                               const PlayerSnapshot& rhs) noexcept {
    return lhs.player_id == rhs.player_id && lhs.position == rhs.position &&
           lhs.velocity == rhs.velocity && lhs.rotation == rhs.rotation && lhs.grounded == rhs.grounded;
}

void sort_snapshot(NetworkWorldSnapshot& snapshot) {
    std::sort(snapshot.players.begin(), snapshot.players.end(),
              [](const PlayerSnapshot& lhs,
                 const PlayerSnapshot& rhs) {
                  return lhs.player_id < rhs.player_id;
              });
    std::sort(snapshot.entities.begin(), snapshot.entities.end(),
              [](const NetworkGameplayEntity& lhs, const NetworkGameplayEntity& rhs) {
                  return lhs.id < rhs.id;
              });
}

} // namespace

SnapshotDelta make_snapshot_delta(const NetworkWorldSnapshot& baseline,
                                  const NetworkWorldSnapshot& target) {
    NetworkWorldSnapshot sorted_baseline = baseline;
    NetworkWorldSnapshot sorted_target = target;
    sort_snapshot(sorted_baseline);
    sort_snapshot(sorted_target);

    SnapshotDelta delta{};
    delta.baseline_tick = sorted_baseline.tick;
    delta.target_tick = sorted_target.tick;

    std::size_t baseline_index = 0;
    std::size_t target_index = 0;
    while (baseline_index < sorted_baseline.entities.size() ||
           target_index < sorted_target.entities.size()) {
        if (baseline_index >= sorted_baseline.entities.size()) {
            delta.added_entities.push_back(sorted_target.entities[target_index++]);
            continue;
        }
        if (target_index >= sorted_target.entities.size()) {
            delta.removed_entity_ids.push_back(sorted_baseline.entities[baseline_index++].id);
            continue;
        }

        const NetworkGameplayEntity& baseline_entity = sorted_baseline.entities[baseline_index];
        const NetworkGameplayEntity& target_entity = sorted_target.entities[target_index];
        if (baseline_entity.id == target_entity.id) {
            if (!same_entity(baseline_entity, target_entity)) {
                delta.updated_entities.push_back(target_entity);
            }
            ++baseline_index;
            ++target_index;
        } else if (baseline_entity.id < target_entity.id) {
            delta.removed_entity_ids.push_back(baseline_entity.id);
            ++baseline_index;
        } else {
            delta.added_entities.push_back(target_entity);
            ++target_index;
        }
    }

    baseline_index = 0;
    target_index = 0;
    while (baseline_index < sorted_baseline.players.size() || target_index < sorted_target.players.size()) {
        if (baseline_index >= sorted_baseline.players.size()) {
            delta.updated_players.push_back(sorted_target.players[target_index++]);
            continue;
        }
        if (target_index >= sorted_target.players.size()) {
            delta.removed_player_ids.push_back(sorted_baseline.players[baseline_index++].player_id);
            continue;
        }

        const PlayerSnapshot& baseline_player = sorted_baseline.players[baseline_index];
        const PlayerSnapshot& target_player = sorted_target.players[target_index];
        if (baseline_player.player_id == target_player.player_id) {
            if (!same_player(baseline_player, target_player)) {
                delta.updated_players.push_back(target_player);
            }
            ++baseline_index;
            ++target_index;
        } else if (baseline_player.player_id < target_player.player_id) {
            delta.removed_player_ids.push_back(baseline_player.player_id);
            ++baseline_index;
        } else {
            delta.updated_players.push_back(target_player);
            ++target_index;
        }
    }
    return delta;
}

std::expected<NetworkWorldSnapshot, SnapshotDeltaError> apply_snapshot_delta(
    const NetworkWorldSnapshot& baseline,
    const SnapshotDelta& delta) {
    if (baseline.tick != delta.baseline_tick) {
        return std::unexpected(SnapshotDeltaError{"baseline_tick_mismatch",
                                                 "Snapshot delta baseline tick does not match"});
    }

    NetworkWorldSnapshot result = baseline;
    result.tick = delta.target_tick;
    sort_snapshot(result);

    for (EntityId removed_id : delta.removed_entity_ids) {
        result.entities.erase(std::remove_if(result.entities.begin(), result.entities.end(),
                                            [removed_id](const NetworkGameplayEntity& entity) {
                                                return entity.id == removed_id;
                                            }),
                              result.entities.end());
    }
    for (const NetworkGameplayEntity& entity : delta.updated_entities) {
        auto found = std::find_if(result.entities.begin(), result.entities.end(),
                                  [&entity](const NetworkGameplayEntity& existing) {
                                      return existing.id == entity.id;
                                  });
        if (found == result.entities.end()) {
            return std::unexpected(SnapshotDeltaError{"missing_entity_update",
                                                     "Snapshot delta updated an absent entity"});
        }
        *found = entity;
    }
    for (const NetworkGameplayEntity& entity : delta.added_entities) {
        auto found = std::find_if(result.entities.begin(), result.entities.end(),
                                  [&entity](const NetworkGameplayEntity& existing) {
                                      return existing.id == entity.id;
                                  });
        if (found != result.entities.end()) {
            return std::unexpected(SnapshotDeltaError{"duplicate_entity_add",
                                                     "Snapshot delta added an existing entity"});
        }
        result.entities.push_back(entity);
    }

    for (PlayerId removed_id : delta.removed_player_ids) {
        result.players.erase(std::remove_if(result.players.begin(), result.players.end(),
                                            [removed_id](const PlayerSnapshot& player) {
                                               return player.player_id == removed_id;
                                           }),
                             result.players.end());
    }
    for (const PlayerSnapshot& player : delta.updated_players) {
        auto found = std::find_if(result.players.begin(), result.players.end(),
                                   [&player](const PlayerSnapshot& existing) {
                                      return existing.player_id == player.player_id;
                                  });
        if (found == result.players.end()) {
            result.players.push_back(player);
        } else {
            *found = player;
        }
    }

    sort_snapshot(result);
    return result;
}

} // namespace stellar::network
