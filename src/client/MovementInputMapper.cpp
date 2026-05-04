#include "stellar/client/MovementInputMapper.hpp"

#include <algorithm>
#include <cmath>

namespace stellar::client {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kDefaultMinPitchDegrees = -89.0F;
constexpr float kDefaultMaxPitchDegrees = 89.0F;

[[nodiscard]] bool finite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] float non_negative_or(float value, float fallback) noexcept {
    return finite(value) && value >= 0.0F ? value : fallback;
}

[[nodiscard]] float normalize_yaw_degrees(float yaw_degrees) noexcept {
    if (!finite(yaw_degrees)) {
        return 0.0F;
    }
    float normalized = std::fmod(yaw_degrees, 360.0F);
    if (normalized < 0.0F) {
        normalized += 360.0F;
    }
    return normalized;
}

[[nodiscard]] float sanitize_min_pitch(const LookInputMapperConfig& config) noexcept {
    return finite(config.min_pitch_degrees) ? config.min_pitch_degrees : kDefaultMinPitchDegrees;
}

[[nodiscard]] float sanitize_max_pitch(const LookInputMapperConfig& config) noexcept {
    return finite(config.max_pitch_degrees) ? config.max_pitch_degrees : kDefaultMaxPitchDegrees;
}

[[nodiscard]] float clamp_pitch_degrees(float pitch_degrees,
                                        const LookInputMapperConfig& config) noexcept {
    if (!finite(pitch_degrees)) {
        return 0.0F;
    }
    float min_pitch = sanitize_min_pitch(config);
    float max_pitch = sanitize_max_pitch(config);
    if (min_pitch > max_pitch) {
        min_pitch = kDefaultMinPitchDegrees;
        max_pitch = kDefaultMaxPitchDegrees;
    }
    return std::clamp(pitch_degrees, min_pitch, max_pitch);
}

[[nodiscard]] float clamp_default_pitch_degrees(float pitch_degrees) noexcept {
    if (!finite(pitch_degrees)) {
        return 0.0F;
    }
    return std::clamp(pitch_degrees, kDefaultMinPitchDegrees, kDefaultMaxPitchDegrees);
}

[[nodiscard]] MovementInputState movement_state_from_input(
    const stellar::platform::Input& input,
    const MovementInputMapperConfig& config) noexcept {
    return MovementInputState{.forward = input.is_key_pressed(config.bindings.forward),
                              .backward = input.is_key_pressed(config.bindings.backward),
                              .left = input.is_key_pressed(config.bindings.left),
                              .right = input.is_key_pressed(config.bindings.right),
                              .jump = input.is_key_pressed(config.bindings.jump)};
}

} // namespace

stellar::protocol::MovementCommand make_movement_command(
    const MovementInputState& state,
    const MovementInputMapperConfig& config) noexcept {
    stellar::protocol::MovementCommand command{};

    float x = 0.0F;
    float y = 0.0F;

    if (state.left) {
        x -= 1.0F;
    }
    if (state.right) {
        x += 1.0F;
    }
    if (state.forward) {
        y += 1.0F;
    }
    if (state.backward) {
        y -= 1.0F;
    }

    if (config.normalize_diagonal && x != 0.0F && y != 0.0F) {
        constexpr float kInverseSqrtTwo = 0.70710678118654752440F;
        x *= kInverseSqrtTwo;
        y *= kInverseSqrtTwo;
    }

    command.wish_direction = {x, y, 0.0F};
    command.jump = state.jump;
    return command;
}

stellar::protocol::MovementCommand make_movement_command(
    const MovementInputState& state,
    float yaw_degrees,
    const MovementInputMapperConfig& config) noexcept {
    stellar::protocol::MovementCommand command = make_movement_command(state, config);
    const float radians = normalize_yaw_degrees(yaw_degrees) * kPi / 180.0F;
    const float forward_x = -std::sin(radians);
    const float forward_y = std::cos(radians);
    const float right_x = std::cos(radians);
    const float right_y = std::sin(radians);
    const float local_x = command.wish_direction[0];
    const float local_y = command.wish_direction[1];

    command.wish_direction = {right_x * local_x + forward_x * local_y,
                              right_y * local_x + forward_y * local_y,
                              0.0F};
    return command;
}

stellar::protocol::MovementCommand make_movement_command(
    const MovementInputState& state,
    ClientViewState view_state,
    const MovementInputMapperConfig& config) noexcept {
    stellar::protocol::MovementCommand command =
        make_movement_command(state, view_state.yaw_degrees, config);
    command.view_yaw_degrees = normalize_yaw_degrees(view_state.yaw_degrees);
    command.view_pitch_degrees = clamp_default_pitch_degrees(view_state.pitch_degrees);
    command.has_view_angles = true;
    return command;
}

stellar::protocol::MovementCommand make_movement_command(
    const stellar::platform::Input& input,
    const MovementInputMapperConfig& config) noexcept {
    return make_movement_command(movement_state_from_input(input, config), config);
}

stellar::protocol::MovementCommand make_movement_command(
    const stellar::platform::Input& input,
    ClientViewState view_state,
    const MovementInputMapperConfig& config) noexcept {
    return make_movement_command(movement_state_from_input(input, config), view_state, config);
}

ClientViewState update_client_view_state(const stellar::platform::Input& input,
                                         float delta_seconds,
                                         ClientViewState previous,
                                         const LookInputMapperConfig& config) noexcept {
    ClientViewState next{.yaw_degrees = normalize_yaw_degrees(previous.yaw_degrees),
                         .pitch_degrees = clamp_pitch_degrees(previous.pitch_degrees, config)};
    const float dt = finite(delta_seconds) && delta_seconds > 0.0F ? delta_seconds : 0.0F;
    const float mouse_sensitivity = non_negative_or(
        config.mouse_sensitivity_degrees_per_pixel,
        LookInputMapperConfig{}.mouse_sensitivity_degrees_per_pixel);
    const float keyboard_yaw = non_negative_or(
        config.keyboard_yaw_degrees_per_second,
        LookInputMapperConfig{}.keyboard_yaw_degrees_per_second);
    const float keyboard_pitch = non_negative_or(
        config.keyboard_pitch_degrees_per_second,
        LookInputMapperConfig{}.keyboard_pitch_degrees_per_second);

    if (input.is_key_pressed(config.bindings.yaw_left)) {
        next.yaw_degrees += keyboard_yaw * dt;
    }
    if (input.is_key_pressed(config.bindings.yaw_right)) {
        next.yaw_degrees -= keyboard_yaw * dt;
    }
    if (input.is_key_pressed(config.bindings.pitch_up)) {
        next.pitch_degrees += keyboard_pitch * dt;
    }
    if (input.is_key_pressed(config.bindings.pitch_down)) {
        next.pitch_degrees -= keyboard_pitch * dt;
    }

    next.yaw_degrees -= static_cast<float>(input.mouse_delta_x()) * mouse_sensitivity;
    next.pitch_degrees -= static_cast<float>(input.mouse_delta_y()) * mouse_sensitivity;
    next.yaw_degrees = normalize_yaw_degrees(next.yaw_degrees);
    next.pitch_degrees = clamp_pitch_degrees(next.pitch_degrees, config);
    return next;
}

} // namespace stellar::client
