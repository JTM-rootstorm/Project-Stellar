#include "stellar/platform/Window.hpp"

#include <SDL2/SDL.h>
#include <utility>

namespace stellar::platform {

Window::~Window() noexcept {
    destroy();
}

Window::Window(Window&& other) noexcept
    : window_(other.window_),
      gl_context_(other.gl_context_),
      should_close_(other.should_close_) {
    other.window_ = nullptr;
    other.gl_context_ = nullptr;
    other.should_close_ = false;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        destroy();
        window_ = other.window_;
        gl_context_ = other.gl_context_;
        should_close_ = other.should_close_;
        other.window_ = nullptr;
        other.gl_context_ = nullptr;
        other.should_close_ = false;
    }
    return *this;
}

std::expected<void, Error>
Window::create(int width, int height, std::string_view title) {
    if (window_ != nullptr) {
        return std::unexpected(Error("Window already created"));
    }

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        return std::unexpected(
            Error(std::string("SDL_InitSubSystem failed: ") + SDL_GetError()));
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);

    window_ = SDL_CreateWindow(
        title.data(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    if (window_ == nullptr) {
        return std::unexpected(
            Error(std::string("SDL_CreateWindow failed: ") + SDL_GetError()));
    }

    gl_context_ = SDL_GL_CreateContext(window_);
    if (gl_context_ == nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return std::unexpected(
            Error(std::string("SDL_GL_CreateContext failed: ") +
                  SDL_GetError()));
    }

    return {};
}

void Window::destroy() noexcept {
    if (gl_context_ != nullptr) {
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    should_close_ = false;
}

void Window::swap_buffers() noexcept {
    if (window_ != nullptr) {
        SDL_GL_SwapWindow(window_);
    }
}

void Window::poll_events() noexcept {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            should_close_ = true;
        }
    }
}

bool Window::should_close() const noexcept {
    return should_close_;
}

void Window::request_close() noexcept {
    should_close_ = true;
}

std::expected<void, Error> Window::set_vsync(bool enabled) noexcept {
    if (SDL_GL_SetSwapInterval(enabled ? 1 : 0) != 0) {
        return std::unexpected(
            Error(std::string("SDL_GL_SetSwapInterval failed: ") +
                  SDL_GetError()));
    }
    return {};
}

SDL_Window* Window::native_handle() const noexcept {
    return window_;
}

} // namespace stellar::platform
