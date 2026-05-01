#include "stellar/client/Application.hpp"

#include "../fixtures/BspFixture.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
  const auto root =
      std::filesystem::temp_directory_path() / "stellar_client_map_smoke";
  std::filesystem::create_directories(root);

  const auto bsp_path = root / "valid_map.bsp";
  stellar::tests::fixtures::write_bsp_fixture(bsp_path);

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
  assert(result->runtime_world_diagnostics->marker_count == 5);
  assert(result->runtime_world_diagnostics->sprite_marker_count == 1);
  assert(result->runtime_world_diagnostics->object_collider_marker_count == 1);
  assert(result->runtime_world_diagnostics->has_player_spawn);

  const auto runtime = stellar::client::prepare_application_runtime(config);
  assert(runtime.has_value());
  assert(runtime->validation != nullptr);
  assert(runtime->validation->level.has_value());
  assert(runtime->runtime_world != nullptr);
  assert(runtime->runtime_world->level_asset == &*runtime->validation->level);
  assert(runtime->runtime_world->diagnostics.has_collision);
  assert(runtime->runtime_world->diagnostics.marker_count == 5);
  assert(runtime->local_loopback_runtime != nullptr);
  assert(runtime->local_loopback_runtime->latest_snapshot().players.size() == 1);
  assert(runtime->local_loopback_runtime->latest_snapshot().players[0].player_id ==
         1);

  stellar::client::ApplicationConfig no_map_config;
  const auto no_map_runtime =
      stellar::client::prepare_application_runtime(no_map_config);
  assert(no_map_runtime.has_value());
  assert(no_map_runtime->validation != nullptr);
  assert(!no_map_runtime->validation->level.has_value());
  assert(no_map_runtime->runtime_world == nullptr);
  assert(no_map_runtime->local_loopback_runtime == nullptr);

  config.map_path = (root / "unsupported.map").string();
  const auto unsupported = stellar::client::validate_application_config(config);
  assert(!unsupported.has_value());
  assert(unsupported.error().message.find("Unsupported map extension") !=
         std::string::npos);

  const auto bad_script_path = root / "bad_script.bsp";
  const auto bad_script =
      stellar::tests::fixtures::build_bsp_invalid_script_path_fixture();
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
