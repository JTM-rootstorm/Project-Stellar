#include "stellar/client/Application.hpp"

#include "stellar/client/GameplayPresentation.hpp"
#include "stellar/client/PlayerPresentation.hpp"
#include "stellar/graphics/LevelRenderer.hpp"
#include "stellar/platform/Input.hpp"

#include <SDL2/SDL.h>

#include <memory>
#include <optional>
#include <utility>

namespace stellar::client {

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kTargetFps = 60;
constexpr int kFrameDelayMs = 1000 / kTargetFps;

stellar::graphics::LevelRenderView
make_level_render_view(const PlayerCameraFrame &camera) noexcept {
  stellar::graphics::LevelRenderView view;
  view.eye = camera.eye;
  view.target = camera.target;
  view.near_plane = camera.near_plane;
  view.far_plane = camera.far_plane;
  view.visibility_culling = true;
  return view;
}
} // namespace

Application::Application(ApplicationConfig config) noexcept
    : config_(std::move(config)) {}

std::expected<void, stellar::platform::Error> Application::run() {
  auto runtime = prepare_application_runtime(config_);
  if (!runtime) {
    return std::unexpected(runtime.error());
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

  std::optional<stellar::assets::LevelAsset> renderer_level;
  if (runtime->validation->level.has_value()) {
    renderer_level = *runtime->validation->level;
  }
  auto renderer = std::make_unique<stellar::graphics::LevelRenderer>(
      config_.graphics_backend, std::move(renderer_level));
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

    if (runtime->local_loopback_runtime) {
      [[maybe_unused]] const LocalLoopbackFrameResult loopback_frame =
          runtime->local_loopback_runtime->update(input, delta_seconds);
      const auto& snapshot = runtime->local_loopback_runtime->latest_snapshot();
      const auto player_state = make_player_presentation_state(
          snapshot, stellar::server::WorldSessionConfig{}.local_player_id);
      if (player_state.has_value()) {
        renderer->set_render_view(
            make_level_render_view(make_player_camera_frame(*player_state)));
      } else {
        renderer->clear_render_view();
      }
      renderer->set_presentation_state(stellar::graphics::LevelPresentationState{
          .sprites = make_gameplay_presentation_frame(snapshot).sprites});
    } else {
      renderer->clear_render_view();
      renderer->clear_presentation_state();
    }

    renderer->render(elapsed_seconds, delta_seconds, kWindowWidth,
                     kWindowHeight);

    input.reset_frame_state();

    const Uint32 frame_time = SDL_GetTicks() - frame_start;
    if (frame_time < kFrameDelayMs) {
      SDL_Delay(kFrameDelayMs - frame_time);
    }
  }

  return {};
}

} // namespace stellar::client
