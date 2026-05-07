#include "stellar/client/Application.hpp"

#include "stellar/client/GameplayPresentation.hpp"
#include "stellar/client/HudPresentation.hpp"
#include "stellar/client/PlayerPresentation.hpp"
#include "stellar/audio/AudioEventRouter.hpp"
#include "stellar/audio/MiniaudioSink.hpp"
#include "stellar/graphics/LevelRenderer.hpp"
#include "stellar/import/bsp/Loader.hpp"
#include "stellar/platform/DisplayDiagnostics.hpp"
#include "stellar/platform/Input.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stellar::client {

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kDisplayValidationWindowWidth = 320;
constexpr int kDisplayValidationWindowHeight = 240;
constexpr int kTargetFps = 60;
constexpr int kFrameDelayMs = 1000 / kTargetFps;
constexpr int kSkipExitCode = 77;

Uint32 backend_window_flags(stellar::graphics::GraphicsBackend backend) noexcept {
  switch (backend) {
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
  case stellar::graphics::GraphicsBackend::kVulkan:
    return SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI;
#endif
#if defined(STELLAR_ENABLE_METAL_BACKEND)
  case stellar::graphics::GraphicsBackend::kMetal:
    return SDL_WINDOW_METAL | SDL_WINDOW_ALLOW_HIGHDPI;
#endif
  }
  return 0;
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
  const char *value = std::getenv("STELLAR_DEBUG_CAMERA");
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  const std::string text(value);
  return text != "0" && text != "false" && text != "FALSE" &&
         text != "off" && text != "OFF";
}

bool debug_render_enabled() {
  const char *value = std::getenv("STELLAR_DEBUG_RENDER");
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  const std::string text(value);
  return text == "1" || text == "true" || text == "TRUE" || text == "on" ||
         text == "ON";
}

std::size_t debug_render_frame_limit(bool enabled) {
  if (!enabled) {
    return 0;
  }
  const char *value = std::getenv("STELLAR_DEBUG_RENDER_FRAMES");
  if (value == nullptr || value[0] == '\0') {
    return 3;
  }
  char *end = nullptr;
  const unsigned long parsed = std::strtoul(value, &end, 10);
  if (end == value) {
    return 3;
  }
  return static_cast<std::size_t>(parsed);
}

const char *diagnostic_severity_name(
    stellar::import::bsp::DiagnosticSeverity severity) noexcept {
  switch (severity) {
  case stellar::import::bsp::DiagnosticSeverity::kInfo:
    return "info";
  case stellar::import::bsp::DiagnosticSeverity::kWarning:
    return "warning";
  case stellar::import::bsp::DiagnosticSeverity::kError:
    return "error";
  }
  return "unknown";
}

void print_map_validation_diagnostics(
    const stellar::import::bsp::ImportReport &report) {
  for (const auto &diagnostic : report.diagnostics) {
    std::cout << "stellar-client: BSP diagnostic severity="
              << diagnostic_severity_name(diagnostic.severity)
              << " message=\"" << diagnostic.message << "\"\n";
  }
}

void print_audio_diagnostics(
    const std::vector<stellar::audio::AudioPresentationDiagnostic> &diagnostics) {
  for (const auto &diagnostic : diagnostics) {
    std::cerr << "stellar-client: audio diagnostic code=" << diagnostic.code
              << " sound_id=" << diagnostic.sound_id
              << " message=\"" << diagnostic.message << "\"\n";
  }
}

void log_debug_render_startup(const PreparedApplicationRuntime &runtime) {
  const auto *level =
      runtime.validation != nullptr && runtime.validation->level.has_value()
          ? &*runtime.validation->level
          : nullptr;
  std::size_t primitive_count = 0;
  std::size_t index_count = 0;
  std::size_t surface_count = 0;
  std::size_t material_count = 0;
  std::size_t image_count = 0;
  std::size_t texture_count = 0;
  std::size_t lightmap_count = 0;
  std::size_t raw_lighting_bytes = 0;
  bool visibility_available = false;
  std::size_t visibility_leaf_count = 0;
  std::size_t collision_mesh_count = 0;
  std::size_t collision_triangle_count = 0;
  if (level != nullptr) {
    const auto &geometry = level->geometry;
    for (const auto &mesh : geometry.meshes) {
      primitive_count += mesh.primitives.size();
      for (const auto &primitive : mesh.primitives) {
        index_count += primitive.indices.size();
      }
    }
    surface_count = geometry.surfaces.size();
    material_count = geometry.materials.size();
    image_count = geometry.images.size();
    texture_count = geometry.textures.size();
    lightmap_count = geometry.lightmaps.size();
    raw_lighting_bytes = geometry.raw_lighting.size();
    visibility_available = level->visibility.available;
    visibility_leaf_count = level->visibility.leaves.size();
    if (level->level_collision.has_value()) {
      collision_mesh_count = level->level_collision->meshes.size();
      for (const auto &mesh : level->level_collision->meshes) {
        collision_triangle_count += mesh.triangles.size();
      }
    }
  }

  std::cerr << "stellar-client: debug-render startup validation.level="
            << (level != nullptr ? 1 : 0) << " source="
            << (level != nullptr ? level->source_uri : std::string{})
            << " primitives=" << primitive_count << " indices=" << index_count
            << " surfaces=" << surface_count << " materials=" << material_count
            << " images=" << image_count << " textures=" << texture_count
            << " lightmaps=" << lightmap_count
            << " raw_lighting_bytes=" << raw_lighting_bytes
            << " visibility_available=" << (visibility_available ? 1 : 0)
            << " visibility_leaves=" << visibility_leaf_count
            << " level_collision_meshes=" << collision_mesh_count
            << " level_collision_triangles=" << collision_triangle_count
            << " active_client_runtime=" << (runtime.active_client_runtime != nullptr ? 1 : 0)
            << " single_player_runtime=" << (runtime.single_player_runtime ? 1 : 0)
            << " listen_host_runtime=" << (runtime.listen_host_runtime ? 1 : 0)
            << " remote_runtime=" << (runtime.remote_runtime ? 1 : 0) << '\n';
}

const char* client_runtime_mode_name(ClientRuntimeMode mode) noexcept {
  switch (mode) {
  case ClientRuntimeMode::kNone:
    return "none";
  case ClientRuntimeMode::kSinglePlayer:
    return "SinglePlayer";
  case ClientRuntimeMode::kRemoteClient:
    return "RemoteClient";
  case ClientRuntimeMode::kListenHost:
    return "ListenHost";
  }
  return "unknown";
}

void log_debug_render_runtime_frame(const char *runtime_name, bool latest_snapshot,
                                    bool player_state, std::size_t sprite_count) {
  std::cerr << "stellar-client: debug-render frame runtime=" << runtime_name
            << " latest_snapshot=" << (latest_snapshot ? 1 : 0)
            << " player_state=" << (player_state ? 1 : 0)
            << " sprites=" << sprite_count << '\n';
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

struct ReadbackHistogram {
  std::array<std::array<std::uint64_t, 256>, 4> bins{};
  std::array<std::uint64_t, 4> sums{};
  std::array<std::uint8_t, 4> minimum{255, 255, 255, 255};
  std::array<std::uint8_t, 4> maximum{0, 0, 0, 0};
};

struct MaterialSlotSummary {
  std::size_t material_count = 0;
  std::size_t base_color_texture_count = 0;
  std::size_t lightmap_texture_count = 0;
  std::size_t normal_texture_count = 0;
  std::size_t specular_texture_count = 0;
  std::size_t metallic_roughness_texture_count = 0;
  std::size_t occlusion_texture_count = 0;
  std::size_t emissive_texture_count = 0;
};

[[nodiscard]] ReadbackHistogram make_readback_histogram(
    const stellar::graphics::FrameReadback &readback) noexcept {
  ReadbackHistogram histogram;
  for (std::size_t index = 0; index + 3 < readback.rgba_pixels.size();
       index += 4) {
    for (std::size_t channel = 0; channel < 4; ++channel) {
      const std::uint8_t value = readback.rgba_pixels[index + channel];
      ++histogram.bins[channel][value];
      histogram.sums[channel] += value;
      histogram.minimum[channel] = std::min(histogram.minimum[channel], value);
      histogram.maximum[channel] = std::max(histogram.maximum[channel], value);
    }
  }
  return histogram;
}

[[nodiscard]] MaterialSlotSummary summarize_material_slots(
    const std::optional<stellar::assets::LevelAsset> &level) noexcept {
  MaterialSlotSummary summary;
  if (!level.has_value()) {
    return summary;
  }
  for (const auto &surface_material : level->geometry.materials) {
    ++summary.material_count;
    const stellar::assets::MaterialAsset *material =
        surface_material.resolved_material.has_value()
            ? &*surface_material.resolved_material
            : nullptr;
    if (material != nullptr && material->base_color_texture.has_value()) {
      ++summary.base_color_texture_count;
    } else if (surface_material.texture_index.has_value()) {
      ++summary.base_color_texture_count;
    }
    if (surface_material.lightmap_index.has_value()) {
      ++summary.lightmap_texture_count;
    }
    if (material == nullptr) {
      continue;
    }
    summary.normal_texture_count += material->normal_texture.has_value() ? 1U : 0U;
    summary.specular_texture_count +=
        material->specular_texture.has_value() ? 1U : 0U;
    summary.metallic_roughness_texture_count +=
        material->metallic_roughness_texture.has_value() ? 1U : 0U;
    summary.occlusion_texture_count +=
        material->occlusion_texture.has_value() ? 1U : 0U;
    summary.emissive_texture_count +=
        material->emissive_texture.has_value() ? 1U : 0U;
  }
  return summary;
}

void write_json_string(std::ostream &out, std::string_view value) {
  out << '"';
  for (const char ch : value) {
    switch (ch) {
    case '\\':
      out << "\\\\";
      break;
    case '"':
      out << "\\\"";
      break;
    case '\n':
      out << "\\n";
      break;
    case '\r':
      out << "\\r";
      break;
    case '\t':
      out << "\\t";
      break;
    default:
      out << ch;
      break;
    }
  }
  out << '"';
}

void write_histogram_array(std::ostream &out,
                           const std::array<std::uint64_t, 256> &bins) {
  out << '[';
  for (std::size_t index = 0; index < bins.size(); ++index) {
    if (index != 0) {
      out << ',';
    }
    out << bins[index];
  }
  out << ']';
}

std::string_view projection_convention(
    stellar::graphics::GraphicsBackend backend) noexcept {
  switch (backend) {
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
  case stellar::graphics::GraphicsBackend::kVulkan:
    return "vulkan_ndc_z_zero_to_one";
#endif
#if defined(STELLAR_ENABLE_METAL_BACKEND)
  case stellar::graphics::GraphicsBackend::kMetal:
    return "metal_ndc_z_zero_to_one";
#endif
  }
  return "unknown";
}

std::expected<void, stellar::platform::Error> write_readback_report(
    const ApplicationConfig &config,
    const std::optional<stellar::assets::LevelAsset> &rendered_level,
    const stellar::graphics::FrameReadback &readback) {
  if (!config.readback_output_path.has_value()) {
    return {};
  }
  std::ofstream out(*config.readback_output_path, std::ios::binary);
  if (!out) {
    return std::unexpected(stellar::platform::Error(
        "Failed to open readback output path: " + *config.readback_output_path));
  }

  const ReadbackHistogram histogram = make_readback_histogram(readback);
  const MaterialSlotSummary slots = summarize_material_slots(rendered_level);
  const std::uint64_t pixel_count =
      static_cast<std::uint64_t>(std::max(readback.width, 0)) *
      static_cast<std::uint64_t>(std::max(readback.height, 0));
  const std::array<const char *, 4> channel_names{"r", "g", "b", "a"};

  out << "{\n";
  out << "  \"schema\": \"stellar.frame_readback.v1\",\n";
  out << "  \"backend\": ";
  write_json_string(out,
                    stellar::graphics::graphics_backend_name(config.graphics_backend));
  out << ",\n";
  out << "  \"map\": ";
  write_json_string(out, config.map_path.value_or(std::string{}));
  out << ",\n";
  out << "  \"projection\": ";
  write_json_string(out, projection_convention(config.graphics_backend));
  out << ",\n";
  out << "  \"frame\": {\"width\": " << readback.width
      << ", \"height\": " << readback.height << ", \"format\": \"rgba8\"},\n";
  out << "  \"material_slots\": {";
  out << "\"materials\": " << slots.material_count;
  out << ", \"base_color\": " << slots.base_color_texture_count;
  out << ", \"lightmap\": " << slots.lightmap_texture_count;
  out << ", \"normal\": " << slots.normal_texture_count;
  out << ", \"specular\": " << slots.specular_texture_count;
  out << ", \"metallic_roughness\": " << slots.metallic_roughness_texture_count;
  out << ", \"occlusion\": " << slots.occlusion_texture_count;
  out << ", \"emissive\": " << slots.emissive_texture_count << "},\n";
  out << "  \"histogram\": {\n";
  for (std::size_t channel = 0; channel < channel_names.size(); ++channel) {
    out << "    \"" << channel_names[channel] << "\": {";
    out << "\"min\": " << static_cast<int>(histogram.minimum[channel]);
    out << ", \"max\": " << static_cast<int>(histogram.maximum[channel]);
    out << ", \"mean\": "
        << (pixel_count > 0
                ? static_cast<double>(histogram.sums[channel]) /
                      static_cast<double>(pixel_count)
                : 0.0);
    out << ", \"bins\": ";
    write_histogram_array(out, histogram.bins[channel]);
    out << '}';
    out << (channel + 1 < channel_names.size() ? ",\n" : "\n");
  }
  out << "  }\n";
  out << "}\n";
  if (!out) {
    return std::unexpected(stellar::platform::Error(
        "Failed to write readback output path: " + *config.readback_output_path));
  }
  return {};
}

std::expected<void, stellar::platform::Error>
run_display_validation(const ApplicationConfig &config) {
  stellar::platform::Window window;
  if (auto result = window.create(
          kDisplayValidationWindowWidth, kDisplayValidationWindowHeight,
          "Stellar Display Validation",
          SDL_WINDOW_SHOWN | backend_window_flags(config.graphics_backend));
      !result) {
    return std::unexpected(stellar::platform::Error(result.error().message,
                                                    kSkipExitCode));
  }

  std::unique_ptr<ApplicationValidation> validation =
      std::make_unique<ApplicationValidation>();
  std::optional<stellar::assets::LevelAsset> renderer_level;
  std::optional<stellar::assets::LevelAsset> readback_report_level;
  if (config.map_path.has_value()) {
    auto prepared = prepare_application_runtime(config);
    if (!prepared) {
      return std::unexpected(prepared.error());
    }
    if (prepared->validation != nullptr) {
      validation = std::move(prepared->validation);
      if (validation->level.has_value()) {
        renderer_level = *validation->level;
      }
    }
    if (config.readback_output_path.has_value()) {
      auto presentation_level =
          stellar::import::bsp::load_level(*config.map_path);
      if (!presentation_level) {
        return std::unexpected(presentation_level.error());
      }
      renderer_level = std::move(*presentation_level);
    }
    readback_report_level = renderer_level;
  }

  auto renderer = std::make_unique<stellar::graphics::LevelRenderer>(
      config.graphics_backend, std::move(renderer_level));
  if (auto result = renderer->initialize(window); !result) {
    return std::unexpected(stellar::platform::Error(
        stellar::platform::append_display_environment_diagnostics(
            "Display validation graphics initialization failed (backend=" +
            std::string(stellar::graphics::graphics_backend_name(
                config.graphics_backend)) +
            "): " + result.error().message),
        result.error().message.find("no default MTLDevice") != std::string::npos
            ? kSkipExitCode
            : 1));
  }

  if (config.readback_output_path.has_value()) {
    if (auto request = renderer->request_frame_readback(); !request) {
      return std::unexpected(request.error());
    }
    renderer->render(0.0F, 0.0F, kDisplayValidationWindowWidth,
                     kDisplayValidationWindowHeight);
    auto readback = renderer->take_frame_readback();
    if (!readback) {
      return std::unexpected(readback.error());
    }
    if (!readback->has_value()) {
      return std::unexpected(stellar::platform::Error(
          "Display validation readback did not produce a frame"));
    }
    if (auto write =
            write_readback_report(config, readback_report_level, **readback);
        !write) {
      return std::unexpected(write.error());
    }
    std::cout << "stellar-client: display readback validation succeeded (backend="
              << stellar::graphics::graphics_backend_name(config.graphics_backend)
              << ", output=" << *config.readback_output_path << ")\n";
    return {};
  }

  std::cout << "stellar-client: display validation succeeded (backend="
            << stellar::graphics::graphics_backend_name(config.graphics_backend)
            << ")\n";
  return {};
}
} // namespace

Application::Application(ApplicationConfig config) noexcept
    : config_(std::move(config)) {}

std::expected<void, stellar::platform::Error> Application::run() {
  if (config_.validate_display) {
    return run_display_validation(config_);
  }

  auto runtime = prepare_application_runtime(config_);
  if (!runtime) {
    return std::unexpected(runtime.error());
  }

  if (config_.validate_only) {
    if (config_.map_path.has_value()) {
      if (runtime->validation != nullptr &&
          runtime->validation->map_validation_report.has_value()) {
        print_map_validation_diagnostics(
            *runtime->validation->map_validation_report);
      }
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
  const bool debug_render = debug_render_enabled();
  if (debug_render) {
    log_debug_render_startup(*runtime);
  }

  stellar::platform::Input input;
  HudPresentationState hud_state;
  stellar::audio::AudioEventRouter audio_router;
  stellar::audio::RuntimeAudioRequestSink runtime_audio_sink =
      stellar::audio::make_runtime_audio_request_sink();
  print_audio_diagnostics(runtime_audio_sink.diagnostics);
  stellar::audio::AudioRequestSink& audio_sink = *runtime_audio_sink.sink;
  Uint32 previous_frame_start = SDL_GetTicks();
  IClientRuntime* active_client_runtime = runtime->active_client_runtime;
  const bool has_live_runtime = active_client_runtime != nullptr;
  bool mouse_captured = false;
  int debug_camera_frames_remaining = debug_camera_enabled() ? 5 : 0;
  std::size_t debug_render_frames_remaining =
      debug_render_frame_limit(debug_render);
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

    if (active_client_runtime != nullptr) {
      [[maybe_unused]] const ClientRuntimeFrame client_frame =
          active_client_runtime->update(input, delta_seconds);
      apply_gameplay_events(hud_state, client_frame.events);
      [[maybe_unused]] const auto audio_result =
          audio_router.route_events(client_frame.events, audio_sink);
      print_audio_diagnostics(audio_result.diagnostics);
      const char* runtime_name = client_runtime_mode_name(active_client_runtime->mode());
      if (!client_frame.snapshot.has_value()) {
        if (debug_render_frames_remaining > 0) {
          log_debug_render_runtime_frame(runtime_name, false, false, 0);
          --debug_render_frames_remaining;
        }
        renderer->clear_render_view();
        renderer->clear_presentation_state();
      } else {
        const auto player_state = make_player_presentation_state(
            *client_frame.snapshot, client_frame.local_player_id);
        if (player_state.has_value()) {
          const PlayerCameraFrame camera_frame = make_player_camera_frame(*player_state);
          if (debug_camera_frames_remaining > 0) {
            log_debug_camera_frame(runtime_name, *player_state, camera_frame);
            --debug_camera_frames_remaining;
          }
          renderer->set_render_view(make_level_render_view(camera_frame));
        } else {
          renderer->clear_render_view();
        }
        auto presentation_frame = make_gameplay_presentation_frame(*client_frame.snapshot);
        if (debug_render_frames_remaining > 0) {
          log_debug_render_runtime_frame(runtime_name, true,
                                         player_state.has_value(),
                                         presentation_frame.sprites.size());
          --debug_render_frames_remaining;
        }
        renderer->set_presentation_state(stellar::graphics::LevelPresentationState{
            .sprites = std::move(presentation_frame.sprites)});
      }
    } else {
      if (debug_render_frames_remaining > 0) {
        log_debug_render_runtime_frame("none", false, false, 0);
        --debug_render_frames_remaining;
      }
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
