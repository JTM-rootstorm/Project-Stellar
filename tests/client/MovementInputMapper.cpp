#include "stellar/client/MovementInputMapper.hpp"

#include <SDL2/SDL.h>

#include <cassert>
#include <cmath>

namespace {

constexpr float kEpsilon = 0.00001F;

void assert_near(float actual, float expected) {
    assert(std::fabs(actual - expected) <= kEpsilon);
}

void assert_direction(const stellar::protocol::MovementCommand& command,
                      float expected_x,
                      float expected_y,
                      float expected_z) {
    assert_near(command.wish_direction[0], expected_x);
    assert_near(command.wish_direction[1], expected_y);
    assert_near(command.wish_direction[2], expected_z);
}

void synthesize_key(stellar::platform::Input& input, SDL_Scancode scancode, Uint32 type) {
    SDL_Event event{};
    event.type = type;
    event.key.type = type;
    event.key.keysym.scancode = scancode;
    input.process_event(event);
}

void synthesize_mouse_motion(stellar::platform::Input& input, int xrel, int yrel) {
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.type = SDL_MOUSEMOTION;
    event.motion.xrel = xrel;
    event.motion.yrel = yrel;
    input.process_event(event);
}

void no_keys_produces_zero_command() {
    const auto command = stellar::client::make_movement_command(
        stellar::client::MovementInputState{});

    assert_direction(command, 0.0F, 0.0F, 0.0F);
    assert(!command.jump);
}

void forward_maps_to_world_forward() {
    const auto command = stellar::client::make_movement_command(
        stellar::client::MovementInputState{.forward = true});

    assert_direction(command, 0.0F, 1.0F, 0.0F);
    assert(!command.jump);
}

void backward_maps_to_world_backward() {
    const auto command = stellar::client::make_movement_command(
        stellar::client::MovementInputState{.backward = true});

    assert_direction(command, 0.0F, -1.0F, 0.0F);
    assert(!command.jump);
}

void left_and_right_cancel() {
    const auto command = stellar::client::make_movement_command(
        stellar::client::MovementInputState{.left = true, .right = true});

    assert_direction(command, 0.0F, 0.0F, 0.0F);
    assert(!command.jump);
}

void forward_and_backward_cancel() {
    const auto command = stellar::client::make_movement_command(
        stellar::client::MovementInputState{.forward = true, .backward = true});

    assert_direction(command, 0.0F, 0.0F, 0.0F);
    assert(!command.jump);
}

void diagonal_input_is_normalized_when_enabled() {
    const auto command = stellar::client::make_movement_command(
        stellar::client::MovementInputState{.forward = true, .right = true});

    constexpr float kExpected = 0.70710678118654752440F;
    assert_direction(command, kExpected, kExpected, 0.0F);
    assert_near((command.wish_direction[0] * command.wish_direction[0]) +
                    (command.wish_direction[1] * command.wish_direction[1]),
                1.0F);
    assert(!command.jump);
}

void diagonal_input_not_normalized_when_disabled() {
    stellar::client::MovementInputMapperConfig config{};
    config.normalize_diagonal = false;

    const auto command = stellar::client::make_movement_command(
        stellar::client::MovementInputState{.forward = true, .right = true}, config);

    assert_direction(command, 1.0F, 1.0F, 0.0F);
    assert(!command.jump);
}

void camera_relative_forward_uses_positive_yaw_convention() {
    const auto command = stellar::client::make_movement_command(
        stellar::client::MovementInputState{.forward = true}, 90.0F);

    assert_direction(command, -1.0F, 0.0F, 0.0F);
}

void pitch_does_not_change_camera_relative_movement() {
    const stellar::client::ClientViewState yaw_only{.yaw_degrees = 270.0F,
                                                    .pitch_degrees = 0.0F};
    const stellar::client::ClientViewState with_pitch{.yaw_degrees = 270.0F,
                                                      .pitch_degrees = 75.0F};

    const auto a = stellar::client::make_movement_command(
        stellar::client::MovementInputState{.forward = true}, yaw_only);
    const auto b = stellar::client::make_movement_command(
        stellar::client::MovementInputState{.forward = true}, with_pitch);

    assert_direction(a, 1.0F, 0.0F, 0.0F);
    assert_direction(b, a.wish_direction[0], a.wish_direction[1], a.wish_direction[2]);
}

void camera_relative_diagonal_remains_normalized() {
    const auto command = stellar::client::make_movement_command(
        stellar::client::MovementInputState{.forward = true, .right = true}, 90.0F);

    assert_near((command.wish_direction[0] * command.wish_direction[0]) +
                    (command.wish_direction[1] * command.wish_direction[1]),
                1.0F);
}

void command_includes_sanitized_view_angles() {
    const auto command = stellar::client::make_movement_command(
        stellar::client::MovementInputState{.forward = true},
        stellar::client::ClientViewState{.yaw_degrees = -90.0F, .pitch_degrees = 180.0F});

    assert(command.has_view_angles);
    assert_near(command.view_yaw_degrees, 270.0F);
    assert_near(command.view_pitch_degrees, 89.0F);
}

void mouse_and_keyboard_look_update_view_state() {
    stellar::platform::Input input;
    synthesize_key(input, SDL_SCANCODE_LEFT, SDL_KEYDOWN);
    synthesize_key(input, SDL_SCANCODE_UP, SDL_KEYDOWN);
    synthesize_mouse_motion(input, 4, -6);
    stellar::client::LookInputMapperConfig config;
    config.mouse_sensitivity_degrees_per_pixel = 0.5F;
    config.keyboard_yaw_degrees_per_second = 120.0F;
    config.keyboard_pitch_degrees_per_second = 100.0F;
    config.min_pitch_degrees = -45.0F;
    config.max_pitch_degrees = 45.0F;

    const auto view = stellar::client::update_client_view_state(input, 0.5F, {}, config);

    assert_near(view.yaw_degrees, 58.0F);
    assert_near(view.pitch_degrees, 45.0F);
}

void jump_key_sets_jump_intent() {
    const auto command = stellar::client::make_movement_command(
        stellar::client::MovementInputState{.jump = true});

    assert_direction(command, 0.0F, 0.0F, 0.0F);
    assert(command.jump);
}

void plain_state_and_platform_input_adapter_match() {
    stellar::platform::Input input;
    synthesize_key(input, SDL_SCANCODE_W, SDL_KEYDOWN);
    synthesize_key(input, SDL_SCANCODE_D, SDL_KEYDOWN);
    synthesize_key(input, SDL_SCANCODE_SPACE, SDL_KEYDOWN);
    synthesize_key(input, SDL_SCANCODE_A, SDL_KEYDOWN);
    synthesize_key(input, SDL_SCANCODE_A, SDL_KEYUP);

    const stellar::client::MovementInputState state{
        .forward = true,
        .right = true,
        .jump = true,
    };

    const auto from_state = stellar::client::make_movement_command(state);
    const auto from_platform = stellar::client::make_movement_command(input);

    assert_direction(from_platform,
                     from_state.wish_direction[0],
                     from_state.wish_direction[1],
                     from_state.wish_direction[2]);
    assert(from_platform.jump == from_state.jump);
}

void platform_input_adapter_uses_view_state_for_camera_relative_command() {
    stellar::platform::Input input;
    synthesize_key(input, SDL_SCANCODE_W, SDL_KEYDOWN);

    const auto command = stellar::client::make_movement_command(
        input, stellar::client::ClientViewState{.yaw_degrees = 90.0F});

    assert_direction(command, -1.0F, 0.0F, 0.0F);
    assert(command.has_view_angles);
}

} // namespace

int main() {
    no_keys_produces_zero_command();
    forward_maps_to_world_forward();
    backward_maps_to_world_backward();
    left_and_right_cancel();
    forward_and_backward_cancel();
    diagonal_input_is_normalized_when_enabled();
    diagonal_input_not_normalized_when_disabled();
    camera_relative_forward_uses_positive_yaw_convention();
    pitch_does_not_change_camera_relative_movement();
    camera_relative_diagonal_remains_normalized();
    command_includes_sanitized_view_angles();
    mouse_and_keyboard_look_update_view_state();
    jump_key_sets_jump_intent();
    plain_state_and_platform_input_adapter_match();
    platform_input_adapter_uses_view_state_for_camera_relative_command();

    return 0;
}
