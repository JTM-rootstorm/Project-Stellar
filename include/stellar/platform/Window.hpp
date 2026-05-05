#pragma once

#include <expected>
#include <string_view>

#include <SDL2/SDL.h>

#include "stellar/platform/Error.hpp"

namespace stellar::platform {

class Input;

/**
 * @brief RAII wrapper for an OS window using SDL2.
 *
 * Wraps an SDL_Window and provides a clean C++ interface for the engine.
 * All fallible operations return std::expected<void, Error>.
 * No exceptions are thrown.
 */
class Window {
public:
    /** @brief Construct an empty window wrapper with no native SDL window. */
    Window() noexcept = default;

    /** @brief Destroy any owned native SDL window. */
    ~Window() noexcept;

    /** @brief Windows own native OS resources and cannot be copied. */
    Window(const Window&) = delete;

    /** @brief Windows own native OS resources and cannot be copy-assigned. */
    Window& operator=(const Window&) = delete;

    /** @brief Move ownership of a native SDL window from another wrapper. */
    Window(Window&& other) noexcept;

    /** @brief Move-assign ownership of a native SDL window from another wrapper. */
    Window& operator=(Window&& other) noexcept;

    /**
     * @brief Create an OS window.
     * @param width  Window width in pixels.
     * @param height Window height in pixels.
     * @param title  Window title string.
     * @param flags  SDL window flags to apply.
     * @return std::expected<void, Error> on failure.
     */
    [[nodiscard]] std::expected<void, Error>
    create(int width, int height, std::string_view title, Uint32 flags = SDL_WINDOW_SHOWN);

    /**
     * @brief Destroy the window.
     */
    void destroy() noexcept;

    /**
     * @brief Poll and process pending OS events.
     */
    void poll_events() noexcept;

    /**
     * @brief Process pending OS events through an Input object.
     * @param input The Input instance to feed events to.
     */
    void process_input(Input& input) noexcept;

    /**
     * @brief Query whether the window should close.
     */
    [[nodiscard]] bool should_close() const noexcept;

    /**
     * @brief Request that the window close on the next frame.
     */
    void request_close() noexcept;

    /**
     * @brief Enable or disable SDL relative mouse mode for camera-look capture.
     * @return std::expected<void, Error> with SDL diagnostics on failure.
     */
    [[nodiscard]] std::expected<void, Error> set_relative_mouse_mode(bool enabled) noexcept;

    /** @brief Return true when SDL relative mouse mode is currently enabled. */
    [[nodiscard]] bool relative_mouse_mode() const noexcept;

    /**
     * @brief Get the underlying SDL window handle.
     */
    [[nodiscard]] SDL_Window* native_handle() const noexcept;

private:
    SDL_Window* window_ = nullptr;
    bool should_close_ = false;
};

} // namespace stellar::platform
