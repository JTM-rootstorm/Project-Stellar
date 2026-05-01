#include "stellar/client/ApplicationConfig.hpp"

#include "../fixtures/BspFixture.hpp"

#include <cassert>
#include <filesystem>

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
  assert(result->runtime_world_diagnostics.has_value());
  assert(result->runtime_world_diagnostics->has_collision);
  assert(result->runtime_world_diagnostics->marker_count == 5);
  assert(result->runtime_world_diagnostics->sprite_marker_count == 1);
  assert(result->runtime_world_diagnostics->object_collider_marker_count == 1);
  assert(result->runtime_world_diagnostics->has_player_spawn);

  config.map_path = (root / "unsupported.map").string();
  const auto unsupported = stellar::client::validate_application_config(config);
  assert(!unsupported.has_value());
  assert(unsupported.error().message.find("Unsupported map extension") !=
         std::string::npos);

  return 0;
}
