#pragma once

#include <expected>
#include <string>
#include <string_view>

struct SDL_Window;
using SDL_GLContext = void*;

namespace stellar::platform {

/**
 * @brief Platform-agnostic error type for fallible operations.
 */
struct Error {
    std::string message;

    explicit Error(std::string msg) : message(std::move(msg)) {}
    explicit Error(const char* msg) : message(msg) {}
};

/**
 * @brief RAII wrapper for an OS window with an OpenGL context.
 *
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
     * @brief Create an OS window and an associated OpenGL context.
     * @param width  Window width in pixels.
     * @param height Window height in pixels.
     * @param title  Window title string.
     * @return std::expected<void, Error> on failure.
     */
    [[nodiscard]] std::expected<void, Error>
    create(int width, int height, std::string_view title);

    /**
     * @brief Destroy the window and OpenGL context.
     */
    void destroy() noexcept;

    /**
     * @brief Swap the front/back buffers.
     */
    void swap_buffers() noexcept;

    /**
     * @brief Poll and process pending OS events.
     */
    void poll_events() noexcept;

    /**
     * @brief Query whether the window should close.
     */
    [[nodiscard]] bool should_close() const noexcept;

    /**
     * @brief Request that the window close on the next frame.
     */
    void request_close() noexcept;

    /**
     * @brief Enable or disable vertical synchronization.
     * @param enabled true to enable VSync, false to disable.
     * @return std::expected<void, Error> on failure.
     */
    [[nodiscard]] std::expected<void, Error> set_vsync(bool enabled) noexcept;

    /**
     * @brief Get the underlying SDL window handle.
     */
    [[nodiscard]] SDL_Window* native_handle() const noexcept;

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    bool should_close_ = false;
};

} // namespace stellar::platform
