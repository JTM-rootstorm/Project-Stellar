#include "stellar/client/MovementInputMapper.hpp"

#include <SDL2/SDL.h>

#include <cassert>
#include <cmath>

namespace {

constexpr float kEpsilon = 0.00001F;

void assert_near(float actual, float expected) {
    assert(std::fabs(actual - expected) <= kEpsilon);
}

void assert_direction(const stellar::server::MovementCommand& command,
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

} // namespace

int main() {
    no_keys_produces_zero_command();
    forward_maps_to_world_forward();
    backward_maps_to_world_backward();
    left_and_right_cancel();
    forward_and_backward_cancel();
    diagonal_input_is_normalized_when_enabled();
    diagonal_input_not_normalized_when_disabled();
    jump_key_sets_jump_intent();
    plain_state_and_platform_input_adapter_match();

    return 0;
}
