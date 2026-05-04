#include "stellar/client/ApplicationConfig.hpp"
#include "stellar/import/bsp/Diagnostics.hpp"

#include "../fixtures/BspFixture.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

namespace {

bool has_code(const stellar::import::bsp::ImportReport &report,
              stellar::import::bsp::DiagnosticCode code) {
  for (const auto &diagnostic : report.diagnostics) {
    if (diagnostic.code == code) {
      return true;
    }
  }
  return false;
}

} // namespace

int main() {
  const auto root =
      std::filesystem::temp_directory_path() / "stellar_bsp_authoring_smoke";
  std::filesystem::create_directories(root);

  const auto valid_path = root / "authored_valid.bsp";
  stellar::tests::fixtures::write_bsp_fixture(valid_path, "gameplay_room");

  stellar::client::ApplicationConfig config;
  config.validate_only = true;
  config.map_path = valid_path.string();

  const auto valid = stellar::client::validate_application_config(config);
  assert(valid.has_value());
  assert(valid->level.has_value());
  assert(valid->map_validation_report.has_value());
  assert(!valid->map_validation_report->has_errors);
  assert(valid->runtime_world_diagnostics.has_value());
  assert(valid->runtime_world_diagnostics->has_player_spawn);
  assert(valid->runtime_world_diagnostics->has_collision);

  const auto missing_spawn_path = root / "missing_spawn.bsp";
  const auto missing_spawn =
      stellar::tests::fixtures::build_bsp_missing_player_spawn_fixture();
  {
    std::ofstream file(missing_spawn_path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(missing_spawn.data()),
               static_cast<std::streamsize>(missing_spawn.size()));
  }
  config.map_path = missing_spawn_path.string();
  const auto warning_only = stellar::client::validate_application_config(config);
  assert(warning_only.has_value());
  assert(warning_only->map_validation_report.has_value());
  assert(has_code(*warning_only->map_validation_report,
                  stellar::import::bsp::DiagnosticCode::kMissingPlayerSpawn));

  const auto bad_script_path = root / "bad_script.bsp";
  const auto bad_script =
      stellar::tests::fixtures::build_bsp_invalid_script_path_fixture();
  {
    std::ofstream file(bad_script_path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bad_script.data()),
               static_cast<std::streamsize>(bad_script.size()));
  }
  config.map_path = bad_script_path.string();
  const auto invalid = stellar::client::validate_application_config(config);
  assert(!invalid.has_value());

  return 0;
}
