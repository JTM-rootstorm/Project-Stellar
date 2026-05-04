#pragma once

#include "stellar/platform/Input.hpp"
#include "stellar/protocol/Types.hpp"

#include <SDL2/SDL.h>

namespace stellar::client {

/**
 * @brief Configurable key bindings for local movement command generation.
 */
struct MovementInputBindings {
    /** @brief Key that requests world-forward movement along positive Y. */
    SDL_Scancode forward = SDL_SCANCODE_W;

    /** @brief Key that requests world-backward movement along negative Y. */
    SDL_Scancode backward = SDL_SCANCODE_S;

    /** @brief Key that requests world-left movement along negative X. */
    SDL_Scancode left = SDL_SCANCODE_A;

    /** @brief Key that requests world-right movement along positive X. */
    SDL_Scancode right = SDL_SCANCODE_D;

    /** @brief Key that requests jump intent for server-side handling. */
    SDL_Scancode jump = SDL_SCANCODE_SPACE;
};

/**
 * @brief Options for converting client input state to server movement intent.
 */
struct MovementInputMapperConfig {
    /** @brief Platform key bindings used by the platform::Input adapter overload. */
    MovementInputBindings bindings{};

    /** @brief Normalize non-zero diagonal X/Y input to length one when true. */
    bool normalize_diagonal = true;
};

/** @brief Configurable key bindings for keyboard look and mouse capture controls. */
struct LookInputBindings {
    /** @brief Key that turns the view left on the Z-up yaw axis. */
    SDL_Scancode yaw_left = SDL_SCANCODE_LEFT;

    /** @brief Key that turns the view right on the Z-up yaw axis. */
    SDL_Scancode yaw_right = SDL_SCANCODE_RIGHT;

    /** @brief Key that pitches the view upward. */
    SDL_Scancode pitch_up = SDL_SCANCODE_UP;

    /** @brief Key that pitches the view downward. */
    SDL_Scancode pitch_down = SDL_SCANCODE_DOWN;

    /** @brief Key that toggles relative mouse capture in live play. */
    SDL_Scancode toggle_mouse_capture = SDL_SCANCODE_F1;
};

/** @brief Options for converting mouse/key deltas into client view-angle commands. */
struct LookInputMapperConfig {
    /** @brief Mouse-look sensitivity in degrees per SDL relative-motion pixel. */
    float mouse_sensitivity_degrees_per_pixel = 0.08F;

    /** @brief Keyboard yaw speed in degrees per second. */
    float keyboard_yaw_degrees_per_second = 120.0F;

    /** @brief Keyboard pitch speed in degrees per second. */
    float keyboard_pitch_degrees_per_second = 90.0F;

    /** @brief Minimum allowed pitch angle in degrees. */
    float min_pitch_degrees = -89.0F;

    /** @brief Maximum allowed pitch angle in degrees. */
    float max_pitch_degrees = 89.0F;

    /** @brief Platform input bindings for keyboard look and capture toggles. */
    LookInputBindings bindings{};
};

/** @brief Client-side accumulated view angles submitted to authoritative server simulation. */
struct ClientViewState {
    /** @brief Yaw in degrees around +Z; positive yaw turns +Y forward toward -X. */
    float yaw_degrees = 0.0F;

    /** @brief Pitch in degrees around camera right; positive pitch looks upward. */
    float pitch_degrees = 0.0F;
};

/**
 * @brief Display-free movement button state before conversion to server intent.
 */
struct MovementInputState {
    /** @brief Requests world-forward movement along positive Y. */
    bool forward = false;

    /** @brief Requests world-backward movement along negative Y. */
    bool backward = false;

    /** @brief Requests world-left movement along negative X. */
    bool left = false;

    /** @brief Requests world-right movement along positive X. */
    bool right = false;

    /** @brief Requests jump intent; the client does not apply movement locally. */
    bool jump = false;
};

/**
 * @brief Convert display-free input state into a protocol movement command intent.
 *
 * Direction convention: forward is positive Y, backward is negative Y, left is negative X,
 * right is positive X, and Z is always zero. Jump is forwarded only as command data; this
 * mapper performs no client-side movement or prediction.
 */
[[nodiscard]] stellar::protocol::MovementCommand make_movement_command(
    const MovementInputState& state,
    const MovementInputMapperConfig& config = {}) noexcept;

/**
 * @brief Convert input state into camera-relative movement for a protocol command.
 *
 * The supplied yaw rotates movement on the X/Y plane only. Positive yaw turns +Y forward toward -X,
 * matching PlayerPresentation's Z-up quaternion convention. Pitch does not affect movement.
 */
[[nodiscard]] stellar::protocol::MovementCommand make_movement_command(
    const MovementInputState& state,
    float yaw_degrees,
    const MovementInputMapperConfig& config = {}) noexcept;

/**
 * @brief Convert input state and view angles into a protocol command intent.
 *
 * Movement is camera-relative on the X/Y plane, and sanitized view angles are attached for
 * server-authoritative snapshot rotation. The client still performs no local movement or
 * prediction.
 */
[[nodiscard]] stellar::protocol::MovementCommand make_movement_command(
    const MovementInputState& state,
    ClientViewState view_state,
    const MovementInputMapperConfig& config = {}) noexcept;

/**
 * @brief Convert platform input state into a protocol movement command intent.
 *
 * Direction convention: forward is positive Y, backward is negative Y, left is negative X,
 * right is positive X, and Z is always zero. Jump is forwarded only as command data; this
 * mapper performs no client-side movement or prediction.
 */
[[nodiscard]] stellar::protocol::MovementCommand make_movement_command(
    const stellar::platform::Input& input,
    const MovementInputMapperConfig& config = {}) noexcept;

/** @brief Convert platform input and view angles into a server-owned command intent. */
[[nodiscard]] stellar::protocol::MovementCommand make_movement_command(
    const stellar::platform::Input& input,
    ClientViewState view_state,
    const MovementInputMapperConfig& config = {}) noexcept;

/**
 * @brief Accumulate mouse and keyboard look input into sanitized client view state.
 *
 * SDL mouse Y motion is inverted for FPS-style look: moving the mouse upward increases pitch.
 */
[[nodiscard]] ClientViewState update_client_view_state(
    const stellar::platform::Input& input,
    float delta_seconds,
    ClientViewState previous,
    const LookInputMapperConfig& config = {}) noexcept;

} // namespace stellar::client
