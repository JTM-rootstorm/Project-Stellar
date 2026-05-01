#include "stellar/client/LocalLoopbackRuntime.hpp"

#include <array>
#include <cmath>
#include <iterator>
#include <utility>
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
    : config_(config),
      session_(std::in_place_type<stellar::server::WorldSession>, world, config_.session),
      latest_snapshot_(std::get<stellar::server::WorldSession>(session_).snapshot()) {}

LocalLoopbackRuntime::LocalLoopbackRuntime(
    stellar::scripting::ScriptedWorldSession scripted_session,
    LocalLoopbackRuntimeConfig config)
    : config_(config),
      session_(std::in_place_type<stellar::scripting::ScriptedWorldSession>,
               std::move(scripted_session)),
      latest_snapshot_(std::get<stellar::scripting::ScriptedWorldSession>(session_).snapshot()) {}

const stellar::server::WorldSnapshot& LocalLoopbackRuntime::latest_snapshot() const noexcept {
    return latest_snapshot_;
}

void LocalLoopbackRuntime::set_object_colliders(
    std::span<const stellar::world::ObjectCollider> colliders) {
    std::visit([colliders](auto& session) { session.set_object_colliders(colliders); }, session_);
    latest_snapshot_ = std::visit([](const auto& session) { return session.snapshot(); }, session_);
}

std::vector<stellar::server::ObjectColliderEvent>
LocalLoopbackRuntime::replace_object_colliders_preserving_overlaps(
    std::span<const stellar::world::ObjectCollider> colliders) noexcept {
    const auto events = std::visit(
        [colliders](auto& session) {
            return session.replace_object_colliders_preserving_overlaps(colliders);
        },
        session_);
    latest_snapshot_ = std::visit([](const auto& session) { return session.snapshot(); }, session_);
    return events;
}

stellar::server::ObjectColliderMutationResult LocalLoopbackRuntime::set_object_collider_enabled(
    std::uint32_t collider_id, bool enabled) noexcept {
    auto result = std::visit(
        [collider_id, enabled](auto& session) {
            return session.set_object_collider_enabled(collider_id, enabled);
        },
        session_);
    latest_snapshot_ = std::visit([](const auto& session) { return session.snapshot(); }, session_);
    return result;
}

stellar::server::ObjectColliderMutationResult LocalLoopbackRuntime::upsert_object_collider(
    const stellar::world::ObjectCollider& collider) noexcept {
    auto result = std::visit(
        [&collider](auto& session) { return session.upsert_object_collider(collider); }, session_);
    latest_snapshot_ = std::visit([](const auto& session) { return session.snapshot(); }, session_);
    return result;
}

stellar::server::ObjectColliderMutationResult LocalLoopbackRuntime::remove_object_collider(
    std::uint32_t collider_id) noexcept {
    auto result = std::visit(
        [collider_id](auto& session) { return session.remove_object_collider(collider_id); }, session_);
    latest_snapshot_ = std::visit([](const auto& session) { return session.snapshot(); }, session_);
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
    result.scripted = std::holds_alternative<stellar::scripting::ScriptedWorldSession>(session_);

    while (accumulated_seconds_ >= fixed_dt && result.ticks_run < max_ticks) {
        stellar::server::WorldSnapshot tick_snapshot;
        if (auto* scripted = std::get_if<stellar::scripting::ScriptedWorldSession>(&session_)) {
            stellar::scripting::ScriptedWorldFrame scripted_frame = scripted->tick(commands);
            tick_snapshot = std::move(scripted_frame.snapshot);
            result.script_events.insert(result.script_events.end(),
                                        std::make_move_iterator(scripted_frame.script_events.begin()),
                                        std::make_move_iterator(scripted_frame.script_events.end()));
            result.script_errors.insert(result.script_errors.end(),
                                        std::make_move_iterator(scripted_frame.script_errors.begin()),
                                        std::make_move_iterator(scripted_frame.script_errors.end()));
            result.command_results.insert(
                result.command_results.end(),
                std::make_move_iterator(scripted_frame.command_results.begin()),
                std::make_move_iterator(scripted_frame.command_results.end()));
        } else {
            tick_snapshot = std::get<stellar::server::WorldSession>(session_).tick(commands);
        }
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

    latest_snapshot_ = std::visit([](const auto& session) { return session.snapshot(); }, session_);
    result.snapshot = latest_snapshot_;
    result.snapshot.trigger_events = std::move(frame_events);
    result.snapshot.object_collider_events = std::move(frame_object_events);
    return result;
}

} // namespace stellar::client
