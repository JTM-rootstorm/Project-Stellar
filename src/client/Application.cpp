#include "stellar/client/Application.hpp"

#include "stellar/graphics/RendererFactory.hpp"
#include "stellar/platform/Input.hpp"

#include <SDL2/SDL.h>

#include <utility>

namespace stellar::client {

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kTargetFps = 60;
constexpr int kFrameDelayMs = 1000 / kTargetFps;
} // namespace

Application::Application(ApplicationConfig config) noexcept : config_(std::move(config)) {}

std::expected<void, stellar::platform::Error> Application::run() {
    auto validation = validate_application_config(config_);
    if (!validation) {
        return std::unexpected(validation.error());
    }

    if (config_.validate_only) {
        return {};
    }

    stellar::platform::Window window;
    const Uint32 backend_window_flags =
        config_.graphics_backend == stellar::graphics::GraphicsBackend::kVulkan
            ? SDL_WINDOW_VULKAN
            : SDL_WINDOW_OPENGL;
    if (auto result = window.create(kWindowWidth, kWindowHeight, "Stellar Engine",
                                    SDL_WINDOW_SHOWN | backend_window_flags);
        !result) {
        return result;
    }

    auto renderer = stellar::graphics::create_renderer(
        config_.graphics_backend, std::move(validation->scene),
        stellar::graphics::SceneRendererAnimationOptions{.animation_index = config_.animation_index,
                                                         .animation_name = config_.animation_name,
                                                         .loop = config_.animation_loop});
    if (auto result = renderer->initialize(window); !result) {
        return result;
    }

    stellar::platform::Input input;
    Uint32 previous_frame_start = SDL_GetTicks();

    while (!window.should_close()) {
        const Uint32 frame_start = SDL_GetTicks();

        window.process_input(input);

        const float elapsed_seconds = static_cast<float>(frame_start) / 1000.0f;
        const float delta_seconds =
            static_cast<float>(frame_start - previous_frame_start) / 1000.0f;
        previous_frame_start = frame_start;

        renderer->render(elapsed_seconds, delta_seconds, kWindowWidth, kWindowHeight);

        input.reset_frame_state();

        const Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < kFrameDelayMs) {
            SDL_Delay(kFrameDelayMs - frame_time);
        }
    }

    return {};
}

} // namespace stellar::client
