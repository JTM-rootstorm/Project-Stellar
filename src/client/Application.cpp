#include "stellar/client/Application.hpp"

#include "stellar/client/GameplayPresentation.hpp"
#include "stellar/client/HudPresentation.hpp"
#include "stellar/client/PlayerPresentation.hpp"
#include "stellar/audio/AudioEventRouter.hpp"
#include "stellar/graphics/LevelRenderer.hpp"
#include "stellar/platform/DisplayDiagnostics.hpp"
#include "stellar/platform/Input.hpp"

#include <SDL2/SDL.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace stellar::client {

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kDisplayValidationWindowWidth = 320;
constexpr int kDisplayValidationWindowHeight = 240;
constexpr int kTargetFps = 60;
constexpr int kFrameDelayMs = 1000 / kTargetFps;

Uint32 backend_window_flags(stellar::graphics::GraphicsBackend backend) noexcept {
  return backend == stellar::graphics::GraphicsBackend::kVulkan ? SDL_WINDOW_VULKAN
                                                                : SDL_WINDOW_OPENGL;
}

stellar::graphics::LevelRenderView
make_level_render_view(const PlayerCameraFrame &camera) noexcept {
  stellar::graphics::LevelRenderView view;
  view.eye = camera.eye;
  view.target = camera.target;
  view.up = camera.up;
  view.near_plane = camera.near_plane;
  view.far_plane = camera.far_plane;
  view.visibility_culling = true;
  return view;
}

bool debug_camera_enabled() {
  const char* value = std::getenv("STELLAR_DEBUG_CAMERA");
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  const std::string text(value);
  return text != "0" && text != "false" && text != "FALSE" && text != "off" && text != "OFF";
}

void set_mouse_capture(stellar::platform::Window& window,
                       bool& mouse_captured,
                       bool enabled) {
  if (auto result = window.set_relative_mouse_mode(enabled); !result) {
    std::cerr << "stellar-client: relative mouse capture unavailable: "
              << result.error().message << '\n';
    mouse_captured = window.relative_mouse_mode();
    return;
  }
  mouse_captured = enabled;
}

void handle_mouse_capture_shortcuts(stellar::platform::Window& window,
                                    const stellar::platform::Input& input,
                                    bool& mouse_captured) {
  if (input.was_key_pressed(SDL_SCANCODE_ESCAPE)) {
    if (mouse_captured) {
      set_mouse_capture(window, mouse_captured, false);
    } else {
      window.request_close();
    }
    return;
  }

  if (input.was_key_pressed(SDL_SCANCODE_F1)) {
    set_mouse_capture(window, mouse_captured, !mouse_captured);
    return;
  }

  if (!mouse_captured && input.was_mouse_button_pressed(SDL_BUTTON_LEFT)) {
    set_mouse_capture(window, mouse_captured, true);
  }
}

void log_debug_camera_frame(const char* runtime_name,
                            const PlayerPresentationState& player,
                            const PlayerCameraFrame& camera) {
  std::cerr << "stellar-client: debug-camera runtime=" << runtime_name
            << " player_position=(" << player.position[0] << ", " << player.position[1]
            << ", " << player.position[2] << ") player_rotation=(" << player.rotation[0]
            << ", " << player.rotation[1] << ", " << player.rotation[2] << ", "
            << player.rotation[3] << ") eye=(" << camera.eye[0] << ", " << camera.eye[1]
            << ", " << camera.eye[2] << ") target=(" << camera.target[0] << ", "
            << camera.target[1] << ", " << camera.target[2] << ") near="
            << camera.near_plane << " far=" << camera.far_plane << '\n';
}

std::expected<void, stellar::platform::Error>
run_display_validation(stellar::graphics::GraphicsBackend backend) {
  stellar::platform::Window window;
  if (auto result = window.create(
          kDisplayValidationWindowWidth, kDisplayValidationWindowHeight,
          "Stellar Display Validation", SDL_WINDOW_SHOWN | backend_window_flags(backend));
      !result) {
    return result;
  }

  auto renderer = std::make_unique<stellar::graphics::LevelRenderer>(backend, std::nullopt);
  if (auto result = renderer->initialize(window); !result) {
    return std::unexpected(stellar::platform::Error(
        stellar::platform::append_display_environment_diagnostics(
            "Display validation graphics initialization failed (backend=" +
            std::string(stellar::graphics::graphics_backend_name(backend)) +
            "): " + result.error().message)));
  }

  std::cout << "stellar-client: display validation succeeded (backend="
            << stellar::graphics::graphics_backend_name(backend) << ")\n";
  return {};
}
} // namespace

Application::Application(ApplicationConfig config) noexcept
    : config_(std::move(config)) {}

std::expected<void, stellar::platform::Error> Application::run() {
  if (config_.validate_display) {
    return run_display_validation(config_.graphics_backend);
  }

  auto runtime = prepare_application_runtime(config_);
  if (!runtime) {
    return std::unexpected(runtime.error());
  }

  if (config_.validate_only) {
    if (config_.map_path.has_value()) {
      std::cout << "stellar-client: BSP map validation succeeded (BSP30 TrenchBroom target; "
                   "legacy BSP29 supported)\n";
    }
    return {};
  }

  stellar::platform::Window window;
  if (auto result = window.create(kWindowWidth, kWindowHeight, "Stellar Engine",
                                  SDL_WINDOW_SHOWN |
                                      backend_window_flags(config_.graphics_backend));
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
  HudPresentationState hud_state;
  stellar::audio::AudioEventRouter audio_router;
  stellar::audio::NoOpAudioRequestSink audio_sink;
  Uint32 previous_frame_start = SDL_GetTicks();
  const bool has_live_runtime = runtime->networked_runtime || runtime->remote_runtime;
  bool mouse_captured = false;
  int debug_camera_frames_remaining = debug_camera_enabled() ? 5 : 0;
  if (has_live_runtime) {
    set_mouse_capture(window, mouse_captured, true);
  }

  while (!window.should_close()) {
    const Uint32 frame_start = SDL_GetTicks();

    window.process_input(input);
    if (has_live_runtime) {
      handle_mouse_capture_shortcuts(window, input, mouse_captured);
    } else if (input.was_key_pressed(SDL_SCANCODE_ESCAPE)) {
      window.request_close();
    }

    const float elapsed_seconds = static_cast<float>(frame_start) / 1000.0f;
    const float delta_seconds =
        static_cast<float>(frame_start - previous_frame_start) / 1000.0f;
    previous_frame_start = frame_start;

    if (runtime->networked_runtime) {
      [[maybe_unused]] const NetworkedClientFrameResult networked_frame =
          runtime->networked_runtime->update(input, delta_seconds);
      apply_gameplay_events(hud_state, networked_frame.events);
      [[maybe_unused]] const auto audio_result =
          audio_router.route_events(networked_frame.events, audio_sink);
      const auto& snapshot = runtime->networked_runtime->latest_snapshot();
      if (!snapshot.has_value()) {
        renderer->clear_render_view();
        renderer->clear_presentation_state();
      } else {
        const auto player_state = make_player_presentation_state(
            *snapshot, runtime->networked_runtime->local_player_id());
        if (player_state.has_value()) {
          const PlayerCameraFrame camera_frame = make_player_camera_frame(*player_state);
          if (debug_camera_frames_remaining > 0) {
            log_debug_camera_frame("NetworkedClientRuntime", *player_state, camera_frame);
            --debug_camera_frames_remaining;
          }
          renderer->set_render_view(make_level_render_view(camera_frame));
        } else {
          renderer->clear_render_view();
        }
        renderer->set_presentation_state(stellar::graphics::LevelPresentationState{
            .sprites = make_gameplay_presentation_frame(*snapshot).sprites});
      }
    } else if (runtime->remote_runtime) {
      [[maybe_unused]] const NetworkedClientFrameResult remote_frame =
          runtime->remote_runtime->update(input, delta_seconds);
      apply_gameplay_events(hud_state, remote_frame.events);
      [[maybe_unused]] const auto audio_result =
          audio_router.route_events(remote_frame.events, audio_sink);
      const auto& snapshot = runtime->remote_runtime->latest_snapshot();
      if (!snapshot.has_value()) {
        renderer->clear_render_view();
        renderer->clear_presentation_state();
      } else {
        const auto player_state = make_player_presentation_state(
            *snapshot, runtime->remote_runtime->local_player_id());
        if (player_state.has_value()) {
          const PlayerCameraFrame camera_frame = make_player_camera_frame(*player_state);
          if (debug_camera_frames_remaining > 0) {
            log_debug_camera_frame("RemoteClientRuntime", *player_state, camera_frame);
            --debug_camera_frames_remaining;
          }
          renderer->set_render_view(make_level_render_view(camera_frame));
        } else {
          renderer->clear_render_view();
        }
        renderer->set_presentation_state(stellar::graphics::LevelPresentationState{
            .sprites = make_gameplay_presentation_frame(*snapshot).sprites});
      }
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
