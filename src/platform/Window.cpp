#include "stellar/platform/Window.hpp"
#include "stellar/platform/Input.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

namespace stellar::platform {

Window::~Window() noexcept {
    destroy();
}

Window::Window(Window&& other) noexcept
    : window_(std::move(other.window_)),
      context_(std::move(other.context_)),
      should_close_(other.should_close_) {
    other.should_close_ = false;
    other.window_ = nullptr;
    other.context_ = nullptr;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        destroy();
        window_ = std::move(other.window_);
        context_ = std::move(other.context_);
        should_close_ = other.should_close_;
        other.should_close_ = false;
        other.window_ = nullptr;
        other.context_ = nullptr;
    }
    return *this;
}

std::expected<void, Error>
Window::create(int width, int height, std::string_view title) {
    if (window_) {
        return std::unexpected(Error("Window already created"));
    }

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        return std::unexpected(Error("SDL initialization failed"));
    }

    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Create window
    window_ = SDL_CreateWindow(
        std::string(title).c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!window_) {
        return std::unexpected(Error("Failed to create SDL window"));
    }

    // Create OpenGL context
    context_ = SDL_GL_CreateContext(window_);
    if (!context_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return std::unexpected(Error("Failed to create OpenGL context"));
    }

    return {};
}

void Window::destroy() noexcept {
    if (context_) {
        SDL_GL_DeleteContext(context_);
        context_ = nullptr;
    }

    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
    should_close_ = false;
}

void Window::swap_buffers() noexcept {
    if (window_) {
        SDL_GL_SwapWindow(window_);
    }
}

void Window::poll_events() noexcept {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                should_close_ = true;
                break;
            case SDL_KEYDOWN:
                // Exit on ESC key
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    should_close_ = true;
                }
                break;
        }
    }
}

void Window::process_input(Input& input) noexcept {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        input.process_event(event);
        switch (event.type) {
            case SDL_QUIT:
                should_close_ = true;
                break;
            case SDL_KEYDOWN:
                // Exit on ESC key
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    should_close_ = true;
                }
                break;
        }
    }
}

bool Window::should_close() const noexcept {
    return should_close_;
}

void Window::request_close() noexcept {
    should_close_ = true;
}

SDL_Window* Window::native_handle() const noexcept {
    return window_;
}

} // namespace stellar::platform