#pragma once

#include <expected>
#include <optional>
#include <string>

#include "stellar/assets/LevelAsset.hpp"
#include "stellar/graphics/GraphicsBackend.hpp"
#include "stellar/import/bsp/Diagnostics.hpp"
#include "stellar/platform/Error.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::client {

/**
 * @brief Configuration supplied to the client application at startup.
 */
struct ApplicationConfig {
  /** @brief Optional BSP map path to validate and use as the playable level
   * source. */
  std::optional<std::string> map_path;

  /** @brief Graphics backend selected at startup. OpenGL remains the default.
   */
  stellar::graphics::GraphicsBackend graphics_backend =
      stellar::graphics::GraphicsBackend::kOpenGL;

  /** @brief Validate startup inputs and return before creating a window or
   * graphics context. */
  bool validate_only = false;
};

/**
 * @brief Backend-neutral result of validating client startup configuration.
 */
struct ApplicationValidation {
  /** @brief Optional source-neutral level loaded from the configured map path.
   */
  std::optional<stellar::assets::LevelAsset> level;

  /** @brief Optional display-free diagnostics for the assembled runtime world.
   */
  std::optional<stellar::world::RuntimeWorldDiagnostics>
      runtime_world_diagnostics;

  /** @brief Optional display-free BSP validation diagnostics for configured map. */
  std::optional<stellar::import::bsp::ImportReport> map_validation_report;
};

/**
 * @brief Validate client startup configuration without creating display or
 * graphics resources.
 */
[[nodiscard]] std::expected<ApplicationValidation, stellar::platform::Error>
validate_application_config(const ApplicationConfig &config);

} // namespace stellar::client
