#include "stellar/server/WorldSession.hpp"

#include <utility>

namespace stellar::server {

WorldSession::WorldSession(const stellar::world::RuntimeWorld& world, WorldSessionConfig config) {
    reset(world, config);
}

void WorldSession::reset(const stellar::world::RuntimeWorld& world, WorldSessionConfig config) {
    world_ = &world;
    config_ = config;
    player_state_ = make_spawn_movement_state(world);
    trigger_tracker_.reset_from_world(world);
    collision_state_ = stellar::world::RuntimeCollisionState::from_world(world);
    tick_index_ = 0;
}

WorldSnapshot WorldSession::snapshot() const {
    return make_snapshot();
}

WorldSnapshot WorldSession::tick(std::span<const PlayerCommand> commands) noexcept {
    const MovementCommand command = select_local_command(commands);
    const MovementTriggerTickResult result = simulate_movement_tick_and_update_triggers(
        *world_, player_state_, command, config_.movement, &collision_state_, trigger_tracker_);
    player_state_ = result.movement.state;
    ++tick_index_;
    return make_snapshot(result.trigger_events);
}

std::uint64_t WorldSession::tick_index() const noexcept {
    return tick_index_;
}

stellar::world::RuntimeCollisionStateResult WorldSession::set_collision_mesh_enabled(
    std::string_view name,
    bool enabled) noexcept {
    return collision_state_.set_mesh_enabled(name, enabled);
}

WorldSnapshot WorldSession::make_snapshot(
    std::vector<MovementTriggerEvent> trigger_events) const {
    WorldSnapshot snapshot;
    snapshot.tick = tick_index_;
    snapshot.players.push_back(PlayerSnapshot{.player_id = config_.local_player_id,
                                               .position = player_state_.position,
                                               .velocity = player_state_.velocity,
                                               .rotation = {0.0F, 0.0F, 0.0F, 1.0F},
                                               .grounded = player_state_.grounded});
    snapshot.trigger_events = std::move(trigger_events);
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
