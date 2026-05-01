#pragma once

#include "stellar/platform/Input.hpp"
#include "stellar/server/MovementSimulation.hpp"

#include <SDL2/SDL.h>

namespace stellar::client {

/**
 * @brief Configurable key bindings for local movement command generation.
 */
struct MovementInputBindings {
    /** @brief Key that requests world-forward movement along negative Z. */
    SDL_Scancode forward = SDL_SCANCODE_W;

    /** @brief Key that requests world-backward movement along positive Z. */
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

    /** @brief Normalize non-zero diagonal X/Z input to length one when true. */
    bool normalize_diagonal = true;
};

/**
 * @brief Display-free movement button state before conversion to server intent.
 */
struct MovementInputState {
    /** @brief Requests world-forward movement along negative Z. */
    bool forward = false;

    /** @brief Requests world-backward movement along positive Z. */
    bool backward = false;

    /** @brief Requests world-left movement along negative X. */
    bool left = false;

    /** @brief Requests world-right movement along positive X. */
    bool right = false;

    /** @brief Requests jump intent; the client does not apply movement locally. */
    bool jump = false;
};

/**
 * @brief Convert display-free input state into a server-owned movement command intent.
 *
 * Direction convention: forward is negative Z, backward is positive Z, left is negative X,
 * right is positive X, and Y is always zero. Jump is forwarded only as command data; this
 * mapper performs no client-side movement or prediction.
 */
[[nodiscard]] stellar::server::MovementCommand make_movement_command(
    const MovementInputState& state,
    const MovementInputMapperConfig& config = {}) noexcept;

/**
 * @brief Convert platform input state into a server-owned movement command intent.
 *
 * Direction convention: forward is negative Z, backward is positive Z, left is negative X,
 * right is positive X, and Y is always zero. Jump is forwarded only as command data; this
 * mapper performs no client-side movement or prediction.
 */
[[nodiscard]] stellar::server::MovementCommand make_movement_command(
    const stellar::platform::Input& input,
    const MovementInputMapperConfig& config = {}) noexcept;

} // namespace stellar::client
