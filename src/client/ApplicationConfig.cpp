#include "stellar/client/ApplicationConfig.hpp"

#include "stellar/import/bsp/Loader.hpp"
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

  auto loaded_level = stellar::import::bsp::load_level(*config.map_path);
  if (!loaded_level) {
    return std::unexpected(loaded_level.error());
  }

  validation.level = std::move(*loaded_level);
  validation.runtime_world_diagnostics =
      stellar::world::build_runtime_world(*validation.level).diagnostics;
  return validation;
}

} // namespace stellar::client
