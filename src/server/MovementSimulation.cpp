#include "stellar/server/MovementSimulation.hpp"

#include <algorithm>
#include <cmath>

namespace stellar::server {
namespace {

constexpr float kMinFixedDt = 1.0e-5F;
constexpr float kMaxFixedDt = 0.25F;
constexpr float kMaxReasonableSpeed = 200.0F;
constexpr float kMaxReasonableAcceleration = 1000.0F;
constexpr float kMaxReasonableGravity = 1000.0F;
constexpr float kMaxReasonableTerminalFallSpeed = 1000.0F;
constexpr float kMinRadius = 0.001F;
constexpr int kMinSlideIterations = 1;
constexpr int kMaxSlideIterations = 16;

using Vec3 = std::array<float, 3>;

[[nodiscard]] bool is_finite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool is_finite(Vec3 value) noexcept {
    return is_finite(value[0]) && is_finite(value[1]) && is_finite(value[2]);
}

[[nodiscard]] float sanitize_non_negative(float value,
                                          float fallback,
                                          float maximum,
                                          bool& sanitized) noexcept {
    if (!is_finite(value) || value < 0.0F) {
        sanitized = true;
        return fallback;
    }
    if (value > maximum) {
        sanitized = true;
        return maximum;
    }
    return value;
}

[[nodiscard]] float sanitize_positive(float value,
                                      float fallback,
                                      float minimum,
                                      float maximum,
                                      bool& sanitized) noexcept {
    if (!is_finite(value) || value < minimum) {
        sanitized = true;
        return fallback;
    }
    if (value > maximum) {
        sanitized = true;
        return maximum;
    }
    return value;
}

[[nodiscard]] Vec3 sanitize_vec3(Vec3 value, bool& sanitized) noexcept {
    for (float& component : value) {
        if (!is_finite(component)) {
            component = 0.0F;
            sanitized = true;
        }
    }
    return value;
}

[[nodiscard]] float length_squared(Vec3 value) noexcept {
    return value[0] * value[0] + value[1] * value[1] + value[2] * value[2];
}

[[nodiscard]] Vec3 clamp_length(Vec3 value, float max_length, bool& sanitized) noexcept {
    const float len_sq = length_squared(value);
    const float max_len_sq = max_length * max_length;
    if (len_sq > max_len_sq && len_sq > 0.0F) {
        const float scale = max_length / std::sqrt(len_sq);
        value[0] *= scale;
        value[1] *= scale;
        value[2] *= scale;
        sanitized = true;
    }
    return value;
}

[[nodiscard]] Vec3 add(Vec3 lhs, Vec3 rhs) noexcept {
    return {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
}

[[nodiscard]] Vec3 sub(Vec3 lhs, Vec3 rhs) noexcept {
    return {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2]};
}

[[nodiscard]] Vec3 mul(Vec3 value, float scale) noexcept {
    return {value[0] * scale, value[1] * scale, value[2] * scale};
}

[[nodiscard]] float approach(float current, float target, float max_delta) noexcept {
    if (current < target) {
        return std::min(current + max_delta, target);
    }
    return std::max(current - max_delta, target);
}

[[nodiscard]] SanitizedMovementSimulationConfig sanitize_config(
    MovementSimulationConfig config) noexcept {
    bool sanitized = false;
    config.max_speed = sanitize_non_negative(config.max_speed, 6.0F, kMaxReasonableSpeed, sanitized);
    config.acceleration =
        sanitize_non_negative(config.acceleration, 40.0F, kMaxReasonableAcceleration, sanitized);
    config.gravity = sanitize_non_negative(config.gravity, 24.0F, kMaxReasonableGravity, sanitized);
    config.terminal_fall_speed = sanitize_non_negative(
        config.terminal_fall_speed, 50.0F, kMaxReasonableTerminalFallSpeed, sanitized);
    config.fixed_dt = sanitize_positive(config.fixed_dt, 1.0F / 60.0F, kMinFixedDt, kMaxFixedDt,
                                        sanitized);

    config.character.radius = sanitize_positive(config.character.radius, 0.35F, kMinRadius,
                                                kMaxReasonableSpeed, sanitized);
    config.character.height = sanitize_positive(config.character.height, 1.8F,
                                                2.0F * config.character.radius,
                                                kMaxReasonableSpeed, sanitized);
    config.character.skin_width = sanitize_non_negative(config.character.skin_width, 0.03F, 1.0F,
                                                        sanitized);
    config.character.max_slope_degrees = sanitize_positive(config.character.max_slope_degrees, 50.0F,
                                                           0.01F, 89.9F, sanitized);
    config.character.step_height = sanitize_non_negative(config.character.step_height, 0.35F, 5.0F,
                                                         sanitized);
    config.character.ground_snap_distance = sanitize_non_negative(
        config.character.ground_snap_distance, 0.12F, 5.0F, sanitized);
    if (config.character.max_slide_iterations < kMinSlideIterations) {
        config.character.max_slide_iterations = kMinSlideIterations;
        sanitized = true;
    } else if (config.character.max_slide_iterations > kMaxSlideIterations) {
        config.character.max_slide_iterations = kMaxSlideIterations;
        sanitized = true;
    }

    return {.value = config, .sanitized = sanitized};
}

[[nodiscard]] MovementState sanitize_state(MovementState state, bool& sanitized) noexcept {
    state.position = sanitize_vec3(state.position, sanitized);
    state.velocity = sanitize_vec3(state.velocity, sanitized);
    return state;
}

[[nodiscard]] MovementCommand sanitize_command(MovementCommand command, bool& sanitized) noexcept {
    command.wish_direction = sanitize_vec3(command.wish_direction, sanitized);
    if (command.wish_direction[1] != 0.0F) {
        sanitized = true;
    }
    command.wish_direction[1] = 0.0F;
    command.wish_direction = clamp_length(command.wish_direction, 1.0F, sanitized);
    return command;
}

} // namespace

MovementState make_spawn_movement_state(const stellar::world::RuntimeWorld& world) {
    MovementState state;
    if (const auto* spawn = stellar::world::find_player_spawn(world); spawn != nullptr) {
        state.position = spawn->position;
    }
    return state;
}

SanitizedMovementSimulationConfig sanitize_movement_simulation_config(
    MovementSimulationConfig config) noexcept {
    return sanitize_config(config);
}

MovementTickResult simulate_movement_tick(const stellar::world::RuntimeWorld& world,
                                          const MovementState& previous,
                                          const MovementCommand& command,
                                          const MovementSimulationConfig& config) noexcept {
    return simulate_movement_tick(world, previous, command, config, nullptr);
}

MovementTickResult simulate_movement_tick(
    const stellar::world::RuntimeWorld& world,
    const MovementState& previous,
    const MovementCommand& command,
    const MovementSimulationConfig& config,
    const stellar::world::RuntimeCollisionState* collision_state) noexcept {
    bool sanitized = false;
    const SanitizedMovementSimulationConfig sanitized_config = sanitize_config(config);
    sanitized = sanitized || sanitized_config.sanitized;
    MovementState state = sanitize_state(previous, sanitized);
    const MovementCommand clean_command = sanitize_command(command, sanitized);
    const auto& cfg = sanitized_config.value;

    const Vec3 desired_horizontal = mul(clean_command.wish_direction, cfg.max_speed);
    const float max_delta = cfg.acceleration * cfg.fixed_dt;
    state.velocity[0] = approach(state.velocity[0], desired_horizontal[0], max_delta);
    state.velocity[2] = approach(state.velocity[2], desired_horizontal[2], max_delta);

    if (state.grounded) {
        state.velocity[1] = std::min(state.velocity[1], 0.0F);
    } else {
        state.velocity[1] -= cfg.gravity * cfg.fixed_dt;
        state.velocity[1] = std::max(state.velocity[1], -cfg.terminal_fall_speed);
    }

    const Vec3 start_position = state.position;
    const Vec3 displacement = mul(state.velocity, cfg.fixed_dt);

    MovementTickResult result;
    result.command_was_sanitized = sanitized;

    if (world.collision_world.has_value()) {
        const stellar::physics::CharacterController controller(*world.collision_world);
        const stellar::physics::CharacterMoveInput input{.position = start_position,
                                                          .displacement = displacement,
                                                          .up = {0.0F, 1.0F, 0.0F}};
        if (collision_state != nullptr) {
            const stellar::physics::CollisionQueryFilter filter{
                .enabled_meshes = &collision_state->enabled_meshes()};
            result.collision = controller.move(input, cfg.character, filter);
        } else {
            result.collision = controller.move(input, cfg.character);
        }
        state.position = result.collision.position;
        state.grounded = result.collision.grounded;
        state.velocity = mul(sub(state.position, start_position), 1.0F / cfg.fixed_dt);
        if (state.grounded && state.velocity[1] < 0.0F) {
            state.velocity[1] = 0.0F;
        }
    } else {
        state.position = add(start_position, displacement);
        state.grounded = false;
        result.collision.position = state.position;
        result.collision.remaining_displacement = {};
        result.collision.ground_normal = {0.0F, 1.0F, 0.0F};
        result.collision.hit = false;
        result.collision.grounded = false;
        result.collision.stepped = false;
        result.collision.started_overlapping = false;
        result.collision.iterations = 0;
    }

    if (!is_finite(state.position) || !is_finite(state.velocity)) {
        state.position = start_position;
        state.velocity = {};
        state.grounded = false;
        result.command_was_sanitized = true;
        result.collision.position = state.position;
        result.collision.remaining_displacement = {};
    }

    result.state = state;
    return result;
}

} // namespace stellar::server
