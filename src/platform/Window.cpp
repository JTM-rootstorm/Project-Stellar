#include "stellar/platform/Window.hpp"
#include "stellar/platform/Input.hpp"

#include <SDL2/SDL.h>

namespace stellar::platform {

namespace {

void handle_event(const SDL_Event& event, Input* input, bool& should_close) {
    if (input) {
        input->process_event(event);
    }

    switch (event.type) {
        case SDL_QUIT:
            should_close = true;
            break;
        case SDL_KEYDOWN:
            // Exit on ESC key.
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                should_close = true;
            }
            break;
    }
}

}  // namespace

Window::~Window() noexcept {
    destroy();
}

Window::Window(Window&& other) noexcept
    : window_(std::move(other.window_)),
      should_close_(other.should_close_) {
    other.should_close_ = false;
    other.window_ = nullptr;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        destroy();
        window_ = std::move(other.window_);
        should_close_ = other.should_close_;
        other.should_close_ = false;
        other.window_ = nullptr;
    }
    return *this;
}

std::expected<void, Error>
Window::create(int width, int height, std::string_view title, Uint32 flags) {
    if (window_) {
        return std::unexpected(Error("Window already created"));
    }

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        return std::unexpected(Error("SDL initialization failed"));
    }

    // Create window
    window_ = SDL_CreateWindow(
        std::string(title).c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        flags
    );

    if (!window_) {
        return std::unexpected(Error("Failed to create SDL window"));
    }

    return {};
}

void Window::destroy() noexcept {
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
    should_close_ = false;
}

void Window::poll_events() noexcept {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        handle_event(event, nullptr, should_close_);
    }
}

void Window::process_input(Input& input) noexcept {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        handle_event(event, &input, should_close_);
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
