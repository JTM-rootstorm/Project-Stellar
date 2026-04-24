#include "stellar/graphics/GraphicsDevice.hpp"
#include "stellar/graphics/Renderer.hpp"
#include "stellar/platform/Window.hpp"
#include "stellar/platform/Input.hpp"

#include <SFML/System.hpp>
#include <SFML/OpenGL.hpp>
#include <cstdio>
#include <cstdlib>

using stellar::platform::Error;
using stellar::platform::Input;
using stellar::platform::Window;
using stellar::graphics::Renderer;

namespace {

constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;
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

    auto device = stellar::graphics::create_opengl_device();
    if (auto result = device->initialize(window.native_handle()); !result) {
        std::fprintf(stderr, "GraphicsDevice initialization failed: %s\n",
                     result.error().message.c_str());
        window.destroy();
        return EXIT_FAILURE;
    }

    Renderer renderer;
    if (auto result = renderer.initialize(); !result) {
        std::fprintf(stderr, "Renderer initialization failed: %s\n",
                     result.error().message.c_str());
        device->shutdown();
        window.destroy();
        return EXIT_FAILURE;
    }

    Input input;
    sf::Clock clock;

    while (!window.should_close()) {
        const auto frame_start = clock.getElapsedTime().asMilliseconds();

        window.poll_events();
        window.process_input(input);

        if (input.is_key_pressed(sf::Keyboard::Key::Escape)) {
            window.request_close();
        }

        device->begin_frame();
        device->clear(0.0f, 0.0f, 0.0f, 1.0f);
        renderer.render_text("working", 316.0f, 288.0f, 3.0f, 0xFFFFFFFF);
        device->end_frame();
        window.swap_buffers();

        input.reset_frame_state();

        const auto frame_time = clock.getElapsedTime().asMilliseconds() - frame_start;
        if (frame_time < kFrameDelayMs) {
            sf::sleep(sf::milliseconds(kFrameDelayMs - frame_time));
        }
    }

    // Renderer destructor runs automatically here.
    device->shutdown();
    window.destroy();
    return EXIT_SUCCESS;
}
