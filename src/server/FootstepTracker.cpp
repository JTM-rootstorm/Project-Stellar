#include "stellar/server/FootstepTracker.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "stellar/core/WorldAxes.hpp"

namespace stellar::server {
namespace {

[[nodiscard]] float finite_or(float value, float fallback) noexcept {
    return std::isfinite(value) ? value : fallback;
}

[[nodiscard]] float horizontal_length(std::array<float, 3> value) noexcept {
    const float x = value[stellar::core::kHorizontalPlaneXIndex];
    const float y = value[stellar::core::kHorizontalPlaneYIndex];
    return std::sqrt(x * x + y * y);
}

[[nodiscard]] float horizontal_distance(std::array<float, 3> from,
                                        std::array<float, 3> to) noexcept {
    const float x = to[stellar::core::kHorizontalPlaneXIndex] -
                    from[stellar::core::kHorizontalPlaneXIndex];
    const float y = to[stellar::core::kHorizontalPlaneYIndex] -
                    from[stellar::core::kHorizontalPlaneYIndex];
    return std::sqrt(x * x + y * y);
}

[[nodiscard]] float sanitized_positive(float value, float fallback) noexcept {
    return std::max(0.0F, finite_or(value, fallback));
}

[[nodiscard]] std::string surface_or_generic(const FootstepSurfaceHit& surface) {
    if (!surface.valid || surface.surface_id.empty()) {
        return "generic";
    }
    return surface.surface_id;
}

} // namespace

FootstepTracker::FootstepTracker(FootstepTrackerConfig config) : config_(std::move(config)) {}

void FootstepTracker::reset(std::array<float, 3> position) noexcept {
    last_position_ = position;
    accumulated_distance_ = 0.0F;
    was_grounded_ = false;
}

std::optional<FootstepEvent> FootstepTracker::update(const FootstepTrackerInput& input) {
    const float min_speed = sanitized_positive(config_.min_horizontal_speed, 24.0F);
    const float walk_distance = std::max(1.0F, sanitized_positive(config_.walk_step_distance, 56.0F));
    const float run_distance = std::max(1.0F, sanitized_positive(config_.run_step_distance, 44.0F));
    const float speed = horizontal_length(input.current_velocity);

    if (!input.grounded) {
        accumulated_distance_ = 0.0F;
        was_grounded_ = false;
        last_position_ = input.current_position;
        return std::nullopt;
    }

    const bool landed = !was_grounded_;
    was_grounded_ = true;
    const float distance = horizontal_distance(input.previous_position, input.current_position);
    last_position_ = input.current_position;

    if (config_.emit_on_landing && landed && speed >= min_speed) {
        return FootstepEvent{.tick = input.tick,
                             .player_id = input.player_id,
                             .entity_id = input.entity_id,
                             .surface_id = surface_or_generic(input.surface),
                             .source_material_name = input.surface.source_material_name};
    }

    if (speed < min_speed || distance <= 0.0F) {
        return std::nullopt;
    }

    accumulated_distance_ += distance;
    const float step_distance = speed >= min_speed * 2.0F ? run_distance : walk_distance;
    if (accumulated_distance_ < step_distance) {
        return std::nullopt;
    }

    accumulated_distance_ = std::fmod(accumulated_distance_, step_distance);
    return FootstepEvent{.tick = input.tick,
                         .player_id = input.player_id,
                         .entity_id = input.entity_id,
                         .surface_id = surface_or_generic(input.surface),
                         .source_material_name = input.surface.source_material_name};
}

} // namespace stellar::server
