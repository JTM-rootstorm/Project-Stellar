#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <utility>

#include <SDL2/SDL.h>

namespace stellar::platform {

class Input;

/**
 * @brief Platform-agnostic error type for fallible operations.
 */
struct Error {
    std::string message;

    explicit Error(std::string msg) : message(std::move(msg)) {}
    explicit Error(const char* msg) : message(msg) {}
};

/**
 * @brief RAII wrapper for an OS window using SDL2.
 *
 * Wraps an SDL_Window and provides a clean C++ interface for the engine.
 * All fallible operations return std::expected<void, Error>.
 * No exceptions are thrown.
 */
class Window {
public:
    Window() noexcept = default;
    ~Window() noexcept;

    // Non-copyable
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Movable
    Window(Window&& other) noexcept;
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
     * @brief Get the underlying SDL window handle.
     */
    [[nodiscard]] SDL_Window* native_handle() const noexcept;

private:
    SDL_Window* window_ = nullptr;
    bool should_close_ = false;
};

} // namespace stellar::platform
