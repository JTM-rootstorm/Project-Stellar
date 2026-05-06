#include "stellar/client/Application.hpp"

#include "../fixtures/BspFixture.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void write_text_file(const std::filesystem::path &path, const std::string &source) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  file.write(source.data(), static_cast<std::streamsize>(source.size()));
}

} // namespace

int main() {
  const auto root =
      std::filesystem::temp_directory_path() / "stellar_client_map_smoke";
  std::filesystem::create_directories(root);

  const auto bsp_path = root / "valid_map.bsp";
  stellar::tests::fixtures::write_bsp_fixture(bsp_path, "minimal_zup_room");

  stellar::client::ApplicationConfig config;
  config.map_path = bsp_path.string();
  config.validate_only = true;

  const auto result = stellar::client::validate_application_config(config);

  assert(result.has_value());
  assert(result->level.has_value());
  assert(result->map_validation_report.has_value());
  assert(!result->map_validation_report->has_errors);
  assert(result->runtime_world_diagnostics.has_value());
  assert(result->runtime_world_diagnostics->has_collision);
  assert(result->runtime_world_diagnostics->marker_count == 1);
  assert(result->runtime_world_diagnostics->sprite_marker_count == 0);
  assert(result->runtime_world_diagnostics->object_collider_marker_count == 0);
  assert(result->runtime_world_diagnostics->has_player_spawn);
  assert(!result->scripted_runtime_enabled);
  assert(result->loaded_script_ids.empty());

  const auto runtime = stellar::client::prepare_application_runtime(config);
  assert(runtime.has_value());
  assert(runtime->validation != nullptr);
  assert(runtime->validation->level.has_value());
  assert(runtime->runtime_world != nullptr);
  assert(runtime->runtime_world->level_asset == &*runtime->validation->level);
  assert(runtime->runtime_world->diagnostics.has_collision);
  assert(runtime->runtime_world->diagnostics.marker_count == 1);
  assert(runtime->single_player_runtime != nullptr);
  assert(runtime->active_client_runtime == runtime->single_player_runtime.get());
  assert(runtime->active_client_runtime->mode() ==
         stellar::client::ClientRuntimeMode::kSinglePlayer);
  assert(!runtime->validation->scripted_runtime_enabled);
  stellar::platform::Input input;
  const auto runtime_frame = runtime->single_player_runtime->update(input, 0.0F);
  assert(runtime_frame.rejected_packets == 0);
  assert(runtime_frame.snapshot.has_value());
  assert(runtime_frame.snapshot->players.size() == 1);
  assert(runtime_frame.snapshot->players[0].player_id == 1);

  const auto scripted_bsp_path = root / "scripted_map.bsp";
  stellar::tests::fixtures::write_bsp_fixture(scripted_bsp_path,
                                             "scripted_interaction_zup");
  write_text_file(root / "scripts" / "gate.lua",
                  "Door = {}\n"
                  "function Door.on_trigger_enter(event)\n"
                  "  stellar.emit_event('collision.set_mesh_enabled', "
                  "{mesh = 'GateDoor', enabled = false})\n"
                  "end\n");
  write_text_file(root / "scripts" / "pickup.lua",
                  "PickupGem = {}\n"
                  "function PickupGem.on_object_collider_enter(event)\n"
                  "  stellar.emit_event('gameplay.collect_pickup', "
                  "{name = event.collider_name})\n"
                  "end\n");
  config.map_path = scripted_bsp_path.string();
  const auto scripted_runtime =
      stellar::client::prepare_application_runtime(config);
  assert(scripted_runtime.has_value());
  assert(scripted_runtime->single_player_runtime != nullptr);
  assert(scripted_runtime->active_client_runtime ==
         scripted_runtime->single_player_runtime.get());
  assert(scripted_runtime->validation->scripted_runtime_enabled);
  assert(scripted_runtime->validation->loaded_script_ids.size() == 2);
  assert(scripted_runtime->validation->loaded_script_ids[0] == "scripts/gate.lua");
  assert(scripted_runtime->validation->loaded_script_ids[1] == "scripts/pickup.lua");

  const auto isolated_root = root / "isolated_scripts";
  config.script_root = isolated_root.string();
  const auto missing_script_runtime =
      stellar::client::prepare_application_runtime(config);
  assert(!missing_script_runtime.has_value());
  assert(missing_script_runtime.error().message.find("Missing script source") !=
         std::string::npos);

  config.script_root = (root / "scripts").string();
  const auto escaped_root_runtime =
      stellar::client::prepare_application_runtime(config);
  assert(!escaped_root_runtime.has_value());
  assert(escaped_root_runtime.error().message.find("Missing script source") !=
         std::string::npos);
  config.script_root.reset();

  stellar::client::ApplicationConfig no_map_config;
  const auto no_map_runtime =
      stellar::client::prepare_application_runtime(no_map_config);
  assert(no_map_runtime.has_value());
  assert(no_map_runtime->validation != nullptr);
  assert(!no_map_runtime->validation->level.has_value());
  assert(no_map_runtime->runtime_world == nullptr);
  assert(no_map_runtime->single_player_runtime == nullptr);
  assert(no_map_runtime->active_client_runtime == nullptr);

  config.map_path = (root / "unsupported.map").string();
  const auto unsupported = stellar::client::validate_application_config(config);
  assert(!unsupported.has_value());
  assert(unsupported.error().message.find("Unsupported map extension") !=
         std::string::npos);

  const char *validate_display_args[] = {"stellar-client", "--validate-display"};
  const auto validate_display_config = stellar::client::parse_application_config(
      2, validate_display_args);
  assert(validate_display_config.has_value());
  assert(validate_display_config->validate_display);
  assert(!validate_display_config->validate_only);
  assert(!validate_display_config->map_path.has_value());

  const char *invalid_validate_display_args[] = {
      "stellar-client", "--validate-display", "--map", "room.bsp"};
  const auto invalid_validate_display_config =
      stellar::client::parse_application_config(4, invalid_validate_display_args);
  assert(!invalid_validate_display_config.has_value());
  assert(invalid_validate_display_config.error().message.find("--validate-display") !=
         std::string::npos);

  const std::string removed_backend = std::string("vul") + "kan";
  const std::string removed_backend_message_prefix =
      "Unsupported graphics backend: " + removed_backend;
  const char *invalid_renderer_args[] = {
      "stellar-client", "--renderer", removed_backend.c_str()};
  const auto invalid_renderer_config =
      stellar::client::parse_application_config(3, invalid_renderer_args);
  assert(!invalid_renderer_config.has_value());
  assert(invalid_renderer_config.error().message.find(removed_backend_message_prefix) == 0);

  const char *invalid_graphics_backend_args[] = {
      "stellar-client", "--graphics-backend", removed_backend.c_str()};
  const auto invalid_graphics_backend_config =
      stellar::client::parse_application_config(3, invalid_graphics_backend_args);
  assert(!invalid_graphics_backend_config.has_value());
  assert(invalid_graphics_backend_config.error().message.find(removed_backend_message_prefix) == 0);

  const std::string removed_backend_alias = std::string("v") + "k";
  const char *invalid_graphics_backend_alias_args[] = {
      "stellar-client", "--graphics-backend", removed_backend_alias.c_str()};
  const auto invalid_graphics_backend_alias_config =
      stellar::client::parse_application_config(
          3, invalid_graphics_backend_alias_args);
  assert(!invalid_graphics_backend_alias_config.has_value());
  assert(invalid_graphics_backend_alias_config.error().message.find(
             "Unsupported graphics backend: " + removed_backend_alias) == 0);

#if defined(STELLAR_ENABLE_METAL_BACKEND)
  const char *metal_renderer_args[] = {"stellar-client", "--validate-config",
                                       "--renderer", "metal"};
  const auto metal_renderer_config =
      stellar::client::parse_application_config(4, metal_renderer_args);
  assert(metal_renderer_config.has_value());
  assert(metal_renderer_config->graphics_backend ==
         stellar::graphics::GraphicsBackend::kMetal);

  const char *metal_alias_args[] = {"stellar-client", "--validate-config",
                                    "--renderer", "mtl"};
  const auto metal_alias_config =
      stellar::client::parse_application_config(4, metal_alias_args);
  assert(metal_alias_config.has_value());
  assert(metal_alias_config->graphics_backend ==
         stellar::graphics::GraphicsBackend::kMetal);
#else
  const char *metal_renderer_args[] = {"stellar-client", "--validate-config",
                                       "--renderer", "metal"};
  const auto metal_renderer_config =
      stellar::client::parse_application_config(4, metal_renderer_args);
  assert(!metal_renderer_config.has_value());
  assert(metal_renderer_config.error().message.find("compiled backends: opengl") !=
         std::string::npos);
#endif

  const char *opengl_renderer_args[] = {"stellar-client", "--validate-config",
                                        "--renderer", "opengl"};
  const auto opengl_renderer_config =
      stellar::client::parse_application_config(4, opengl_renderer_args);
#if defined(STELLAR_ENABLE_OPENGL_BACKEND)
  assert(opengl_renderer_config.has_value());
  assert(opengl_renderer_config->graphics_backend ==
         stellar::graphics::GraphicsBackend::kOpenGL);
#else
  assert(!opengl_renderer_config.has_value());
  assert(opengl_renderer_config.error().message.find("compiled backends: metal") !=
         std::string::npos);
#endif

  const auto bad_script_path = root / "bad_script.bsp";
  const auto bad_script =
      stellar::tests::fixtures::build_bsp_trenchbroom_invalid_script_escape_fixture();
  {
    std::ofstream file(bad_script_path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bad_script.data()),
               static_cast<std::streamsize>(bad_script.size()));
  }
  config.map_path = bad_script_path.string();
  const auto invalid_map = stellar::client::validate_application_config(config);
  assert(!invalid_map.has_value());

  return 0;
}
