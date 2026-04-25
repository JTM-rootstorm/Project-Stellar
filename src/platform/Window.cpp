#include "stellar/platform/Window.hpp"

#include "stellar/platform/Input.hpp"
#include <utility>

namespace stellar::platform {

Window::~Window() noexcept {
    destroy();
}

Window::Window(Window&& other) noexcept
    : window_(std::move(other.window_)),
      should_close_(other.should_close_) {
    other.should_close_ = false;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        destroy();
        window_ = std::move(other.window_);
        should_close_ = other.should_close_;
        other.should_close_ = false;
    }
    return *this;
}

std::expected<void, Error>
Window::create(int width, int height, std::string_view title) {
    if (window_.isOpen()) {
        return std::unexpected(Error("Window already created"));
    }

    // Configure OpenGL context with depth buffer for 3D rendering
    sf::ContextSettings context_settings;
    context_settings.depthBits = 24;
    context_settings.majorVersion = 3;
    context_settings.minorVersion = 3;
    context_settings.attributeFlags = sf::ContextSettings::Attribute::Core;

    sf::VideoMode video_mode(sf::Vector2u(width, height));
    sf::RenderWindow window(video_mode,
                           std::string(title),
                           sf::State::Windowed,
                           context_settings);

    if (!window.isOpen()) {
        return std::unexpected(Error("Failed to create SFML window"));
    }

    window_ = std::move(window);
    return {};
}

void Window::destroy() noexcept {
    if (window_.isOpen()) {
        window_.close();
    }
    should_close_ = false;
}

void Window::swap_buffers() noexcept {
    if (window_.isOpen()) {
        window_.display();
    }
}

void Window::poll_events() noexcept {
    while (const auto event = window_.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            should_close_ = true;
        }
    }
}

void Window::process_input(Input& input) noexcept {
    while (const auto event = window_.pollEvent()) {
        input.process_event(*event);
        if (event->is<sf::Event::Closed>()) {
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
    // SFML doesn't provide direct vsync control; this is handled at display() time
    // For proper vsync, the driver settings and swap interval must be configured
    // SFML's display() respects the system's swap interval settings
    (void)enabled;
    return {};
}

sf::RenderWindow* Window::native_handle() const noexcept {
    return const_cast<sf::RenderWindow*>(&window_);
}

} // namespace stellar::platform