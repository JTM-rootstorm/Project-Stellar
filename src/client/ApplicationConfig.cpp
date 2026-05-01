#include "stellar/client/ApplicationConfig.hpp"

#include "stellar/import/bsp/Validation.hpp"
#include "stellar/world/RuntimeWorld.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <utility>

namespace stellar::client {
std::expected<ApplicationValidation, stellar::platform::Error>
validate_application_config(const ApplicationConfig &config) {
  ApplicationValidation validation;

  if (!config.map_path.has_value()) {
    return validation;
  }

  std::string extension =
      std::filesystem::path(*config.map_path).extension().string();
  std::ranges::transform(extension, extension.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (extension != ".bsp") {
    return std::unexpected(stellar::platform::Error(
        "Unsupported map extension for --map: " + extension +
        " (expected .bsp)"));
  }

  auto map_validation = stellar::import::bsp::validate_level(*config.map_path);
  if (!map_validation) {
    return std::unexpected(map_validation.error());
  }
  validation.map_validation_report = map_validation->report;
  if (!map_validation->valid || !map_validation->loaded_level.has_value()) {
    for (const auto &diagnostic : map_validation->report.diagnostics) {
      if (diagnostic.severity ==
          stellar::import::bsp::DiagnosticSeverity::kError) {
        return std::unexpected(stellar::platform::Error(diagnostic.message));
      }
    }
    return std::unexpected(stellar::platform::Error(
        "BSP map validation failed for --map: " + *config.map_path));
  }

  validation.level = std::move(map_validation->loaded_level->asset);
  validation.runtime_world_diagnostics =
      stellar::world::build_runtime_world(*validation.level).diagnostics;
  return validation;
}

} // namespace stellar::client
