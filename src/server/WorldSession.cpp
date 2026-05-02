#include "stellar/server/WorldSession.hpp"

#include "stellar/core/WorldAxes.hpp"

#include <utility>

namespace stellar::server {
namespace {

[[nodiscard]] stellar::world::TriggerCapsule make_authoritative_capsule(
    std::array<float, 3> position,
    const MovementSimulationConfig& config) noexcept {
    const SanitizedMovementSimulationConfig sanitized_config =
        sanitize_movement_simulation_config(config);
    return {.center = position,
             .up = stellar::core::kWorldUp,
            .radius = sanitized_config.value.character.radius,
            .height = sanitized_config.value.character.height};
}

[[nodiscard]] std::string archetype_for_collider(
    const stellar::world::ObjectColliderSystem& system,
    std::uint32_t collider_id,
    std::span<const stellar::world::ObjectCollider> fallback_colliders = {}) {
    for (const stellar::world::ObjectCollider& collider : system.colliders()) {
        if (collider.id == collider_id) {
            return collider.archetype;
        }
    }
    for (const stellar::world::ObjectCollider& collider : fallback_colliders) {
        if (collider.id == collider_id) {
            return collider.archetype;
        }
    }
    return {};
}

[[nodiscard]] ObjectColliderEvent to_server_object_event(
    PlayerId player_id,
    const stellar::world::ObjectColliderSystem& system,
    stellar::world::ObjectColliderOverlapEvent event,
    std::span<const stellar::world::ObjectCollider> fallback_colliders = {}) {
    return {.player_id = player_id,
            .collider_id = event.collider_id,
            .name = std::move(event.name),
            .archetype = archetype_for_collider(system, event.collider_id, fallback_colliders),
            .entered = event.entered,
            .stayed = event.stayed,
            .exited = event.exited};
}

[[nodiscard]] std::vector<ObjectColliderEvent> to_server_object_events(
    PlayerId player_id,
    const stellar::world::ObjectColliderSystem& system,
    std::vector<stellar::world::ObjectColliderOverlapEvent> events,
    std::span<const stellar::world::ObjectCollider> fallback_colliders = {}) {
    std::vector<ObjectColliderEvent> converted;
    converted.reserve(events.size());
    for (auto& event : events) {
        converted.push_back(
            to_server_object_event(player_id, system, std::move(event), fallback_colliders));
    }
    return converted;
}

[[nodiscard]] ObjectColliderMutationResult to_server_mutation_result(
    PlayerId player_id,
    const stellar::world::ObjectColliderSystem& system,
    stellar::world::ObjectColliderMutationResult result,
    std::span<const stellar::world::ObjectCollider> fallback_colliders = {}) {
    return {.applied = result.applied,
            .code = std::move(result.code),
            .message = std::move(result.message),
            .object_collider_events =
                to_server_object_events(player_id, system, std::move(result.exit_events),
                                        fallback_colliders)};
}

} // namespace

WorldSession::WorldSession(const stellar::world::RuntimeWorld& world, WorldSessionConfig config) {
    reset(world, config);
}

void WorldSession::reset(const stellar::world::RuntimeWorld& world, WorldSessionConfig config) {
    world_ = &world;
    config_ = config;
    player_state_ = make_spawn_movement_state(world);
    gameplay_world_.reset_from_world(world, config_.local_player_id);
    trigger_tracker_.reset_from_world(world);
    object_collider_system_.set_colliders(stellar::world::build_object_colliders(world));
    collision_state_ = stellar::world::RuntimeCollisionState::from_world(world);
    tick_index_ = 0;
}

WorldSnapshot WorldSession::snapshot() const {
    return make_snapshot();
}

GameplayWorldSnapshot WorldSession::gameplay_snapshot() const {
    return gameplay_world_.snapshot();
}

std::optional<EntityId> WorldSession::entity_for_player(PlayerId player_id) const noexcept {
    return gameplay_world_.entity_for_player(player_id);
}

WorldSnapshot WorldSession::tick(std::span<const PlayerCommand> commands) noexcept {
    const MovementCommand command = select_local_command(commands);
    const MovementTriggerTickResult result = simulate_movement_tick_and_update_triggers(
        *world_, player_state_, command, config_.movement, &collision_state_, trigger_tracker_);
    player_state_ = result.movement.state;
    auto object_events = to_server_object_events(
        config_.local_player_id, object_collider_system_,
        object_collider_system_.update_player_capsule(
            make_authoritative_capsule(player_state_.position, config_.movement)));
    ++tick_index_;
    return make_snapshot(result.trigger_events, std::move(object_events));
}

std::uint64_t WorldSession::tick_index() const noexcept {
    return tick_index_;
}

stellar::world::RuntimeCollisionStateResult WorldSession::set_collision_mesh_enabled(
    std::string_view name,
    bool enabled) noexcept {
    stellar::world::RuntimeCollisionStateResult result =
        collision_state_.set_mesh_enabled(name, enabled);
    if (result.applied) {
        (void)gameplay_world_.set_door_open_by_collision_mesh_name(name, !enabled);
    }
    return result;
}

void WorldSession::set_object_colliders(
    std::span<const stellar::world::ObjectCollider> colliders) {
    object_collider_system_.set_colliders(colliders);
}

std::vector<ObjectColliderEvent> WorldSession::replace_object_colliders_preserving_overlaps(
    std::span<const stellar::world::ObjectCollider> colliders) noexcept {
    const std::vector<stellar::world::ObjectCollider> old_colliders =
        object_collider_system_.colliders();
    return to_server_object_events(
        config_.local_player_id, object_collider_system_,
        object_collider_system_.replace_colliders_preserving_overlaps(colliders), old_colliders);
}

ObjectColliderMutationResult WorldSession::set_object_collider_enabled(
    std::uint32_t collider_id, bool enabled) noexcept {
    return to_server_mutation_result(config_.local_player_id, object_collider_system_,
                                     object_collider_system_.set_collider_enabled(collider_id,
                                                                                   enabled));
}

PickupCollectionResult WorldSession::collect_pickup(std::uint32_t collider_id) noexcept {
    if (!gameplay_world_.is_active_pickup_collider(collider_id)) {
        const bool known_collider =
            gameplay_world_.entity_for_object_collider(collider_id) != nullptr;
        return PickupCollectionResult{.applied = false,
                                      .code = known_collider ? "already_collected" : "not_found",
                                      .message = known_collider ? "Pickup is already inactive"
                                                                : "Pickup collider was not found"};
    }

    ObjectColliderMutationResult disabled = set_object_collider_enabled(collider_id, false);
    if (!disabled.applied) {
        return PickupCollectionResult{.applied = false,
                                      .code = std::move(disabled.code),
                                      .message = std::move(disabled.message),
                                      .object_collider_events =
                                          std::move(disabled.object_collider_events)};
    }

    if (!gameplay_world_.deactivate_pickup_by_collider(collider_id)) {
        return PickupCollectionResult{.applied = false,
                                      .code = "invalid_pickup",
                                      .message = "Collider is not an active pickup"};
    }

    return PickupCollectionResult{.applied = true,
                                  .code = "collected",
                                  .message = "Pickup collected",
                                  .object_collider_events =
                                      std::move(disabled.object_collider_events)};
}

ObjectColliderMutationResult WorldSession::upsert_object_collider(
    const stellar::world::ObjectCollider& collider) noexcept {
    const std::vector<stellar::world::ObjectCollider> old_colliders =
        object_collider_system_.colliders();
    return to_server_mutation_result(config_.local_player_id, object_collider_system_,
                                     object_collider_system_.upsert_collider(collider),
                                     old_colliders);
}

ObjectColliderMutationResult WorldSession::remove_object_collider(
    std::uint32_t collider_id) noexcept {
    const std::vector<stellar::world::ObjectCollider> old_colliders =
        object_collider_system_.colliders();
    return to_server_mutation_result(config_.local_player_id, object_collider_system_,
                                     object_collider_system_.remove_collider(collider_id),
                                     old_colliders);
}

WorldSnapshot WorldSession::make_snapshot(
    std::vector<MovementTriggerEvent> trigger_events,
    std::vector<ObjectColliderEvent> object_collider_events) const {
    WorldSnapshot snapshot;
    snapshot.tick = tick_index_;
    snapshot.players.push_back(PlayerSnapshot{.player_id = config_.local_player_id,
                                               .position = player_state_.position,
                                               .velocity = player_state_.velocity,
                                               .rotation = {0.0F, 0.0F, 0.0F, 1.0F},
                                               .grounded = player_state_.grounded});
    snapshot.trigger_events = std::move(trigger_events);
    snapshot.object_collider_events = std::move(object_collider_events);
    snapshot.gameplay_world = gameplay_world_.snapshot();
    return snapshot;
}

MovementCommand WorldSession::select_local_command(
    std::span<const PlayerCommand> commands) const noexcept {
    for (const PlayerCommand& command : commands) {
        if (command.player_id == config_.local_player_id) {
            return command.movement;
        }
    }
    return {};
}

} // namespace stellar::server
