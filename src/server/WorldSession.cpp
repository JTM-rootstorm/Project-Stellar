#include "stellar/server/WorldSession.hpp"

#include "stellar/core/WorldAxes.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string>
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

[[nodiscard]] const std::string* marker_property(
    const stellar::assets::WorldEntityProperty* begin,
    const stellar::assets::WorldEntityProperty* end,
    std::string_view key) noexcept {
    for (auto* it = begin; it != end; ++it) {
        if (it->key == key) {
            return &it->value;
        }
    }
    return nullptr;
}

[[nodiscard]] float parse_float_or(std::string_view value, float fallback) noexcept {
    float parsed = fallback;
    const auto* begin = value.data();
    const auto* end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    return (result.ec == std::errc{} && result.ptr == end) ? parsed : fallback;
}

[[nodiscard]] std::array<float, 3> direction_from_angle(float angle_degrees) noexcept {
    if (std::abs(angle_degrees + 1.0F) < 0.001F) {
        return stellar::core::kWorldUp;
    }
    if (std::abs(angle_degrees + 2.0F) < 0.001F) {
        return stellar::core::kWorldDown;
    }
    constexpr float kPi = 3.14159265358979323846F;
    const float radians = angle_degrees * kPi / 180.0F;
    return {std::cos(radians), std::sin(radians), 0.0F};
}

[[nodiscard]] std::array<float, 3> mul3(std::array<float, 3> value, float scale) noexcept {
    return {value[0] * scale, value[1] * scale, value[2] * scale};
}

[[nodiscard]] std::array<float, 3> add3(std::array<float, 3> a,
                                        std::array<float, 3> b) noexcept {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
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
    brush_movers_.clear();
    scheduled_target_fires_.clear();
    target_diagnostics_.clear();
    if (world.level_asset != nullptr && world.level_asset->level_collision.has_value()) {
        for (const auto& brush : world.level_asset->geometry.brush_entities) {
            if (brush.classname != "func_door" && brush.classname != "func_button") {
                continue;
            }
            const auto& props = brush.properties;
            const std::string* angle_text = props.empty() ? nullptr
                : marker_property(props.data(), props.data() + props.size(), "angle");
            const std::string* speed_text = props.empty() ? nullptr
                : marker_property(props.data(), props.data() + props.size(), "speed");
            const std::string* wait_text = props.empty() ? nullptr
                : marker_property(props.data(), props.data() + props.size(), "wait");
            const std::string* lip_text = props.empty() ? nullptr
                : marker_property(props.data(), props.data() + props.size(), "lip");
            const std::string* delay_text = props.empty() ? nullptr
                : marker_property(props.data(), props.data() + props.size(), "delay");
            const float angle = angle_text == nullptr ? 0.0F : parse_float_or(*angle_text, 0.0F);
            const auto direction = direction_from_angle(angle);
            const std::array<float, 3> extents{brush.bounds_max[0] - brush.bounds_min[0],
                                               brush.bounds_max[1] - brush.bounds_min[1],
                                               brush.bounds_max[2] - brush.bounds_min[2]};
            const float axis_size = std::abs(direction[0]) * extents[0] +
                                    std::abs(direction[1]) * extents[1] +
                                    std::abs(direction[2]) * extents[2];
            const float lip = lip_text == nullptr ? 8.0F : parse_float_or(*lip_text, 8.0F);
            std::uint32_t mesh_index = brush.bsp_model_index;
            if (world.level_asset->level_collision.has_value()) {
                const auto& meshes = world.level_asset->level_collision->meshes;
                for (std::size_t index = 0; index < meshes.size(); ++index) {
                    if (meshes[index].name == brush.collision_mesh_name) {
                        mesh_index = static_cast<std::uint32_t>(index);
                        break;
                    }
                }
            }
            EntityId entity_id = 0;
            for (const auto& entity : gameplay_world_.entities()) {
                if (entity.metadata.name == brush.collision_mesh_name || entity.metadata.name == brush.name) {
                    entity_id = entity.id;
                    break;
                }
            }
            brush_movers_.push_back(BrushMover{.entity_id = entity_id,
                                               .collision_mesh_index = mesh_index,
                                               .name = brush.name,
                                               .classname = brush.classname,
                                               .target = brush.target,
                                               .direction = direction,
                                               .distance = std::max(0.0F, axis_size - lip),
                                               .speed = speed_text == nullptr ? 100.0F : parse_float_or(*speed_text, 100.0F),
                                               .wait = wait_text == nullptr ? 1.0F : parse_float_or(*wait_text, 1.0F),
                                               .delay = delay_text == nullptr ? 0.0F : parse_float_or(*delay_text, 0.0F)});
        }
    }
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
    const float dt = sanitize_movement_simulation_config(config_.movement).value.fixed_dt;
    for (auto it = scheduled_target_fires_.begin(); it != scheduled_target_fires_.end();) {
        it->remaining_seconds -= dt;
        if (it->remaining_seconds <= 0.0F) {
            const std::string target = it->targetname;
            it = scheduled_target_fires_.erase(it);
            (void)fire_target(target);
        } else {
            ++it;
        }
    }
    for (BrushMover& mover : brush_movers_) {
        if (mover.phase == BrushMoverPhase::kOpening || mover.phase == BrushMoverPhase::kPressed) {
            mover.progress = std::min(mover.distance, mover.progress + std::max(0.0F, mover.speed) * dt);
            if (mover.progress >= mover.distance) {
                mover.phase = BrushMoverPhase::kOpen;
                mover.wait_remaining = std::max(0.0F, mover.wait);
            }
        } else if (mover.phase == BrushMoverPhase::kOpen) {
            if (mover.wait >= 0.0F) {
                mover.wait_remaining -= dt;
                if (mover.wait_remaining <= 0.0F) {
                    mover.phase = BrushMoverPhase::kClosing;
                }
            }
        } else if (mover.phase == BrushMoverPhase::kClosing) {
            mover.progress = std::max(0.0F, mover.progress - std::max(0.0F, mover.speed) * dt);
            if (mover.progress <= 0.0F) {
                mover.phase = BrushMoverPhase::kClosed;
            }
        }
        (void)collision_state_.set_mesh_translation(mover.collision_mesh_index,
                                                    mul3(mover.direction, mover.progress));
    }
    const MovementCommand command = select_local_command(commands);
    const MovementTriggerTickResult result = simulate_movement_tick_and_update_triggers(
        *world_, player_state_, command, config_.movement, &collision_state_, trigger_tracker_);
    player_state_ = result.movement.state;
    auto object_events = to_server_object_events(
        config_.local_player_id, object_collider_system_,
        object_collider_system_.update_player_capsule(
            make_authoritative_capsule(player_state_.position, config_.movement)));
    for (const MovementTriggerEvent& event : result.trigger_events) {
        if (event.entered) {
            for (const auto& marker : world_->world_metadata.markers) {
                if (marker.name == event.trigger_name) {
                    for (const auto& prop : marker.properties) {
                        if (prop.key == "target" && !prop.value.empty()) {
                            float delay = 0.0F;
                            for (const auto& delay_prop : marker.properties) {
                                if (delay_prop.key == "delay") {
                                    delay = parse_float_or(delay_prop.value, 0.0F);
                                }
                            }
                            if (delay > 0.0F) {
                                scheduled_target_fires_.push_back({.targetname = prop.value,
                                                                   .remaining_seconds = delay});
                            } else {
                                (void)fire_target(prop.value);
                            }
                        }
                    }
                }
            }
        }
    }
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

bool WorldSession::fire_target(std::string_view targetname) noexcept {
    if (targetname.empty()) {
        target_diagnostics_.push_back("target router ignored empty targetname");
        return false;
    }
    bool matched = false;
    for (BrushMover& mover : brush_movers_) {
        if (mover.name != targetname) {
            continue;
        }
        matched = true;
        if (mover.classname == "func_button") {
            mover.phase = BrushMoverPhase::kPressed;
            if (!mover.target.empty()) {
                if (mover.delay > 0.0F) {
                    scheduled_target_fires_.push_back({.targetname = mover.target,
                                                       .remaining_seconds = mover.delay});
                } else {
                    (void)fire_target(mover.target);
                }
            }
        } else {
            mover.phase = BrushMoverPhase::kOpening;
        }
    }
    if (!matched) {
        target_diagnostics_.push_back("target router missing target: " + std::string(targetname));
    }
    return matched;
}

std::span<const std::string> WorldSession::target_diagnostics() const noexcept {
    return target_diagnostics_;
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
    for (const BrushMover& mover : brush_movers_) {
        const auto translation = mul3(mover.direction, mover.progress);
        snapshot.brush_movers.push_back(BrushMoverSnapshot{.entity_id = mover.entity_id,
                                                           .name = mover.name,
                                                           .classname = mover.classname,
                                                           .translation = translation,
                                                           .open = mover.progress >= mover.distance});
    }
    for (BrushMoverSnapshot& mover : snapshot.brush_movers) {
        for (stellar::server::GameplayEntity& entity : snapshot.gameplay_world.entities) {
            if (entity.id == mover.entity_id) {
                entity.transform.position = add3(entity.transform.position, mover.translation);
                entity.open = mover.open;
            }
        }
    }
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
