#include "stellar/client/Application.hpp"

#include "stellar/graphics/RendererFactory.hpp"
#include "stellar/platform/Input.hpp"

#if defined(STELLAR_ENABLE_GLTF)
#include "stellar/import/gltf/Loader.hpp"
#endif

#include <SDL2/SDL.h>

#include <optional>
#include <utility>

namespace stellar::client {

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kTargetFps = 60;
constexpr int kFrameDelayMs = 1000 / kTargetFps;
constexpr float kRotationSpeed = 45.0f;

} // namespace

Application::Application(ApplicationConfig config) noexcept : config_(std::move(config)) {}

std::expected<void, stellar::platform::Error> Application::run() {
    std::optional<stellar::assets::SceneAsset> scene;
    if (config_.asset_path.has_value()) {
#if defined(STELLAR_ENABLE_GLTF)
        auto loaded_scene = stellar::import::gltf::load_scene(*config_.asset_path);
        if (!loaded_scene) {
            return std::unexpected(loaded_scene.error());
        }
        scene = std::move(*loaded_scene);
#else
        return std::unexpected(stellar::platform::Error(
            "--asset requires a build configured with STELLAR_ENABLE_GLTF=ON"));
#endif
    }

    stellar::platform::Window window;
    if (auto result = window.create(kWindowWidth, kWindowHeight, "Stellar Engine",
                                    SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
        !result) {
        return result;
    }

    auto renderer = stellar::graphics::create_renderer(std::move(scene));
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
