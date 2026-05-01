#include "stellar/client/Application.hpp"

#include "stellar/client/ScriptRegistryLoader.hpp"
#include "stellar/import/bsp/Validation.hpp"
#include "stellar/scripting/ScriptedWorldSession.hpp"
#include "stellar/world/RuntimeWorld.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace stellar::client {

namespace {

std::expected<ApplicationValidation, stellar::platform::Error>
load_application_validation(const ApplicationConfig &config) {
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
  return validation;
}

} // namespace

std::expected<ApplicationValidation, stellar::platform::Error>
validate_application_config(const ApplicationConfig &config) {
  auto validation = load_application_validation(config);
  if (!validation) {
    return std::unexpected(validation.error());
  }

  if (validation->level.has_value()) {
    validation->runtime_world_diagnostics =
        stellar::world::build_runtime_world(*validation->level).diagnostics;
  }
  return validation;
}

std::expected<PreparedApplicationRuntime, stellar::platform::Error>
prepare_application_runtime(const ApplicationConfig &config) {
  auto validation = load_application_validation(config);
  if (!validation) {
    return std::unexpected(validation.error());
  }

  PreparedApplicationRuntime prepared;
  prepared.validation =
      std::make_unique<ApplicationValidation>(std::move(*validation));

  if (prepared.validation->level.has_value()) {
    prepared.runtime_world = std::make_unique<stellar::world::RuntimeWorld>(
        stellar::world::build_runtime_world(*prepared.validation->level));
    prepared.validation->runtime_world_diagnostics =
        prepared.runtime_world->diagnostics;

    if (runtime_world_has_script_bindings(*prepared.runtime_world)) {
      ScriptRegistryLoadConfig load_config;
      if (config.map_path.has_value()) {
        load_config.map_path = *config.map_path;
      }
      if (config.script_root.has_value()) {
        load_config.script_root = *config.script_root;
      }

      auto registry = load_script_registry_for_world(*prepared.runtime_world, load_config);
      if (!registry) {
        return std::unexpected(registry.error());
      }
      prepared.validation->loaded_script_ids = registry->loaded_script_ids;
      prepared.validation->script_errors = registry->script_errors;

      auto scripted_session = stellar::scripting::ScriptedWorldSession::create(
          *prepared.runtime_world, stellar::server::WorldSessionConfig{},
          std::move(registry->registry));
      if (!scripted_session) {
        prepared.validation->script_errors.push_back(scripted_session.error());
        return std::unexpected(stellar::platform::Error(
            "Failed to prepare scripted authoritative runtime for --map: " +
            scripted_session.error().message));
      }
      prepared.validation->scripted_runtime_enabled = true;
      prepared.networked_runtime = std::make_unique<NetworkedClientRuntime>(
          std::move(*scripted_session));
    } else {
      prepared.networked_runtime =
          std::make_unique<NetworkedClientRuntime>(*prepared.runtime_world);
    }
  }

  return prepared;
}

} // namespace stellar::client
