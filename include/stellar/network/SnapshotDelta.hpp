#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "stellar/network/Messages.hpp"

namespace stellar::network {

/** @brief Structural authoritative snapshot delta between explicit baseline and target ticks. */
struct SnapshotDelta {
    /** @brief Tick of the snapshot used as delta baseline. */
    std::uint64_t baseline_tick = 0;

    /** @brief Tick of the target snapshot reproduced by applying this delta. */
    std::uint64_t target_tick = 0;

    /** @brief Deterministically ordered newly visible entities. */
    std::vector<NetworkGameplayEntity> added_entities;

    /** @brief Deterministically ordered changed existing entities. */
    std::vector<NetworkGameplayEntity> updated_entities;

    /** @brief Deterministically ordered removed entity identifiers. */
    std::vector<stellar::server::EntityId> removed_entity_ids;

    /** @brief Deterministically ordered changed or newly visible players. */
    std::vector<stellar::server::PlayerSnapshot> updated_players;

    /** @brief Deterministically ordered removed player identifiers. */
    std::vector<stellar::server::PlayerId> removed_player_ids;

};

/** @brief Failure while applying a snapshot delta. */
struct SnapshotDeltaError {
    /** @brief Stable machine-readable error code. */
    std::string code;

    /** @brief Human-readable deterministic diagnostic. */
    std::string message;
};

/** @brief Build a deterministic structural delta from baseline to target snapshot. */
[[nodiscard]] SnapshotDelta make_snapshot_delta(const NetworkWorldSnapshot& baseline,
                                                const NetworkWorldSnapshot& target);

/** @brief Apply a structural delta to its baseline to reproduce the target snapshot. */
[[nodiscard]] std::expected<NetworkWorldSnapshot, SnapshotDeltaError> apply_snapshot_delta(
    const NetworkWorldSnapshot& baseline,
    const SnapshotDelta& delta);

} // namespace stellar::network
