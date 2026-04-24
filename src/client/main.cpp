#include "stellar/platform/Window.hpp"
#include "stellar/platform/Input.hpp"

#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>
#include <cstdio>
#include <cstdlib>

using stellar::platform::Error;
using stellar::platform::Input;
using stellar::platform::Window;

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kTargetFps = 60;
constexpr int kFrameDelayMs = 1000 / kTargetFps;

} // anonymous namespace

int main(int /*argc*/, char* /*argv*/[]) {
    Window window;
    if (auto result = window.create(kWindowWidth, kWindowHeight,
                                    "Stellar Engine");
        !result) {
        std::fprintf(stderr, "Window creation failed: %s\n",
                     result.error().message.c_str());
        return EXIT_FAILURE;
    }

    if (auto result = window.set_vsync(true); !result) {
        std::fprintf(stderr, "Failed to set VSync: %s\n",
                     result.error().message.c_str());
        // Non-fatal; continue without VSync.
    }

    // Load font for text rendering
    sf::Font font;
    if (!font.openFromFile("/usr/share/fonts/urw-fonts/URWGothic-Book.ttf")) {
        std::fprintf(stderr, "Failed to load font: /usr/share/fonts/urw-fonts/URWGothic-Book.ttf\n");
        std::fprintf(stderr, "Please install the font or update the path in main.cpp\n");
        window.destroy();
        return EXIT_FAILURE;
    }

    sf::Text debug_text(font);
    debug_text.setString("working");
    debug_text.setCharacterSize(24);
    debug_text.setFillColor(sf::Color::White);

    Input input;
    sf::Clock clock;

    while (!window.should_close()) {
        const auto frame_start = clock.getElapsedTime().asMilliseconds();

        window.process_input(input);

        if (input.is_key_pressed(sf::Keyboard::Key::Escape)) {
            window.request_close();
        }

        // Use RenderTarget interface for SFML drawing
        auto* render_target = static_cast<sf::RenderTarget*>(window.native_handle());
        render_target->pushGLStates();
        // Center text on screen
        sf::FloatRect bounds = debug_text.getLocalBounds();
        debug_text.setPosition({(kWindowWidth - bounds.size.x) / 2.0f,
                               (kWindowHeight - bounds.size.y) / 2.0f});
        render_target->draw(debug_text);
        render_target->popGLStates();

        window.swap_buffers();

        input.reset_frame_state();

        const auto frame_time = clock.getElapsedTime().asMilliseconds() - frame_start;
        if (frame_time < kFrameDelayMs) {
            sf::sleep(sf::milliseconds(kFrameDelayMs - frame_time));
        }
    }

    window.destroy();
    return EXIT_SUCCESS;
}