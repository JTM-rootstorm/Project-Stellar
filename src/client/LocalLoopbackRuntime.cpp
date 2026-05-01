#include "stellar/client/LocalLoopbackRuntime.hpp"

#include <array>
#include <cmath>
#include <vector>

namespace stellar::client {
namespace {

[[nodiscard]] float fixed_dt_or_default(
    const stellar::server::MovementSimulationConfig& movement) noexcept {
    if (std::isfinite(movement.fixed_dt) && movement.fixed_dt > 0.0F) {
        return movement.fixed_dt;
    }
    return stellar::server::MovementSimulationConfig{}.fixed_dt;
}

[[nodiscard]] int sanitized_max_ticks(int max_ticks_per_frame) noexcept {
    return max_ticks_per_frame > 0 ? max_ticks_per_frame : 0;
}

} // namespace

LocalLoopbackRuntime::LocalLoopbackRuntime(const stellar::world::RuntimeWorld& world,
                                           LocalLoopbackRuntimeConfig config)
    : config_(config), session_(world, config_.session), latest_snapshot_(session_.snapshot()) {}

const stellar::server::WorldSnapshot& LocalLoopbackRuntime::latest_snapshot() const noexcept {
    return latest_snapshot_;
}

void LocalLoopbackRuntime::set_object_colliders(
    std::span<const stellar::world::ObjectCollider> colliders) {
    session_.set_object_colliders(colliders);
    latest_snapshot_ = session_.snapshot();
}

std::vector<stellar::server::ObjectColliderEvent>
LocalLoopbackRuntime::replace_object_colliders_preserving_overlaps(
    std::span<const stellar::world::ObjectCollider> colliders) noexcept {
    const auto events = session_.replace_object_colliders_preserving_overlaps(colliders);
    latest_snapshot_ = session_.snapshot();
    return events;
}

stellar::server::ObjectColliderMutationResult LocalLoopbackRuntime::set_object_collider_enabled(
    std::uint32_t collider_id, bool enabled) noexcept {
    auto result = session_.set_object_collider_enabled(collider_id, enabled);
    latest_snapshot_ = session_.snapshot();
    return result;
}

stellar::server::ObjectColliderMutationResult LocalLoopbackRuntime::upsert_object_collider(
    const stellar::world::ObjectCollider& collider) noexcept {
    auto result = session_.upsert_object_collider(collider);
    latest_snapshot_ = session_.snapshot();
    return result;
}

stellar::server::ObjectColliderMutationResult LocalLoopbackRuntime::remove_object_collider(
    std::uint32_t collider_id) noexcept {
    auto result = session_.remove_object_collider(collider_id);
    latest_snapshot_ = session_.snapshot();
    return result;
}

LocalLoopbackFrameResult LocalLoopbackRuntime::update(const stellar::platform::Input& input,
                                                      float delta_seconds) noexcept {
    LocalLoopbackFrameResult result;

    const float fixed_dt = fixed_dt_or_default(config_.session.movement);
    const int max_ticks = sanitized_max_ticks(config_.max_ticks_per_frame);
    if (std::isfinite(delta_seconds) && delta_seconds > 0.0F) {
        accumulated_seconds_ += delta_seconds;
    }

    const stellar::server::MovementCommand movement = make_movement_command(input, config_.input_mapper);
    const stellar::server::PlayerCommand command{.player_id = config_.session.local_player_id,
                                                 .movement = movement};
    const std::array<stellar::server::PlayerCommand, 1> commands{command};
    std::vector<stellar::server::MovementTriggerEvent> frame_events;
    std::vector<stellar::server::ObjectColliderEvent> frame_object_events;

    while (accumulated_seconds_ >= fixed_dt && result.ticks_run < max_ticks) {
        stellar::server::WorldSnapshot tick_snapshot = session_.tick(commands);
        frame_events.insert(frame_events.end(), tick_snapshot.trigger_events.begin(),
                            tick_snapshot.trigger_events.end());
        frame_object_events.insert(frame_object_events.end(),
                                   tick_snapshot.object_collider_events.begin(),
                                   tick_snapshot.object_collider_events.end());
        accumulated_seconds_ -= fixed_dt;
        ++result.ticks_run;
    }

    if (accumulated_seconds_ >= fixed_dt) {
        result.dropped_excess_time = true;
        accumulated_seconds_ = 0.0F;
    }

    latest_snapshot_ = session_.snapshot();
    result.snapshot = latest_snapshot_;
    result.snapshot.trigger_events = std::move(frame_events);
    result.snapshot.object_collider_events = std::move(frame_object_events);
    return result;
}

} // namespace stellar::client
