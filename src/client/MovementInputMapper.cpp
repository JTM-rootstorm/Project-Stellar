#include "stellar/client/MovementInputMapper.hpp"

namespace stellar::client {

stellar::server::MovementCommand make_movement_command(
    const MovementInputState& state,
    const MovementInputMapperConfig& config) noexcept {
    stellar::server::MovementCommand command{};

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

stellar::server::MovementCommand make_movement_command(
    const stellar::platform::Input& input,
    const MovementInputMapperConfig& config) noexcept {
    const MovementInputState state{
        .forward = input.is_key_pressed(config.bindings.forward),
        .backward = input.is_key_pressed(config.bindings.backward),
        .left = input.is_key_pressed(config.bindings.left),
        .right = input.is_key_pressed(config.bindings.right),
        .jump = input.is_key_pressed(config.bindings.jump),
    };
    return make_movement_command(state, config);
}

} // namespace stellar::client
