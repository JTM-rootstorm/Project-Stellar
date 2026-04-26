#include "stellar/client/Application.hpp"

#include "stellar/graphics/RendererFactory.hpp"
#include "stellar/platform/Input.hpp"

#include <SDL2/SDL.h>

namespace stellar::client {

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kTargetFps = 60;
constexpr int kFrameDelayMs = 1000 / kTargetFps;
constexpr float kRotationSpeed = 45.0f;

} // namespace

std::expected<void, stellar::platform::Error> Application::run() {
    stellar::platform::Window window;
    if (auto result = window.create(kWindowWidth, kWindowHeight, "Stellar Engine");
        !result) {
        return result;
    }

    auto renderer = stellar::graphics::create_renderer();
    if (auto result = renderer->initialize(window); !result) {
        return result;
    }

    stellar::platform::Input input;

    while (!window.should_close()) {
        const Uint32 frame_start = SDL_GetTicks();

        window.process_input(input);

        const float elapsed_seconds = static_cast<float>(frame_start) / 1000.0f;
        const float rotation_angle = elapsed_seconds * kRotationSpeed;

        renderer->render(rotation_angle, kWindowWidth, kWindowHeight);

        input.reset_frame_state();

        const Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < kFrameDelayMs) {
            SDL_Delay(kFrameDelayMs - frame_time);
        }
    }

    return {};
}

} // namespace stellar::client
