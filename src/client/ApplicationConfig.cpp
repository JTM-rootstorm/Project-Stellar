#include "stellar/client/Application.hpp"

#include "stellar/client/ScriptRegistryLoader.hpp"
#include "stellar/import/bsp/Validation.hpp"
#include "stellar/network/SocketTransport.hpp"
#include "stellar/scripting/ScriptedWorldSession.hpp"
#include "stellar/world/RuntimeWorld.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace stellar::client {

namespace {

std::expected<ApplicationValidation, stellar::platform::Error>
load_application_validation(const ApplicationConfig &config) {
  ApplicationValidation validation;

  if (config.connect_endpoint.has_value()) {
    if (config.map_path.has_value()) {
      return std::unexpected(stellar::platform::Error(
          "--map and --connect are ambiguous; remote mode uses authoritative server state"));
    }
    if (config.script_root.has_value()) {
      return std::unexpected(stellar::platform::Error(
          "--script-root applies only to local --map authoritative runtime, not --connect"));
    }
    if (auto parsed = stellar::network::parse_socket_address(*config.connect_endpoint); !parsed) {
      return std::unexpected(stellar::platform::Error(
          "Invalid --connect endpoint: " + parsed.error().message));
    }
    return validation;
  }

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

std::expected<ApplicationConfig, stellar::platform::Error>
parse_application_config(int argc, const char *const argv[]) {
  ApplicationConfig config;

  for (int i = 1; i < argc; ++i) {
    auto require_value = [&](const char* name)
        -> std::expected<std::string, stellar::platform::Error> {
      if (i + 1 >= argc) {
        return std::unexpected(stellar::platform::Error(std::string(name) + " requires a value"));
      }
      return std::string(argv[++i]);
    };

    if (std::strcmp(argv[i], "--validate-config") == 0) {
      config.validate_only = true;
      continue;
    }

    if (std::strcmp(argv[i], "--validate-map") == 0) {
      auto value = require_value("--validate-map");
      if (!value) {
        return std::unexpected(value.error());
      }
      config.validate_only = true;
      config.map_path = std::move(*value);
      continue;
    }

    if (std::strcmp(argv[i], "--validate-display") == 0) {
      config.validate_display = true;
      continue;
    }

    if (std::strcmp(argv[i], "--map") == 0) {
      auto value = require_value("--map");
      if (!value) {
        return std::unexpected(value.error());
      }
      config.map_path = std::move(*value);
      continue;
    }

    if (std::strcmp(argv[i], "--connect") == 0) {
      auto value = require_value("--connect");
      if (!value) {
        return std::unexpected(value.error());
      }
      config.connect_endpoint = std::move(*value);
      continue;
    }

    if (std::strcmp(argv[i], "--client-name") == 0) {
      auto value = require_value("--client-name");
      if (!value) {
        return std::unexpected(value.error());
      }
      config.client_name = std::move(*value);
      continue;
    }

    if (std::strcmp(argv[i], "--script-root") == 0) {
      auto value = require_value("--script-root");
      if (!value) {
        return std::unexpected(value.error());
      }
      config.script_root = std::move(*value);
      continue;
    }

    if (std::strcmp(argv[i], "--renderer") == 0 ||
        std::strcmp(argv[i], "--graphics-backend") == 0) {
      auto value = require_value("--renderer");
      if (!value) {
        return std::unexpected(value.error());
      }
      auto backend = stellar::graphics::parse_graphics_backend(*value);
      if (!backend) {
        return std::unexpected(backend.error());
      }
      config.graphics_backend = *backend;
      continue;
    }

    return std::unexpected(stellar::platform::Error("Unknown client argument: " +
                                                    std::string(argv[i])));
  }

  if (config.connect_endpoint.has_value() && config.map_path.has_value()) {
    return std::unexpected(stellar::platform::Error(
        "--map and --connect are ambiguous; remote mode uses authoritative server state"));
  }
  if (config.validate_display) {
    if (config.validate_only) {
      return std::unexpected(stellar::platform::Error(
          "--validate-display cannot be combined with --validate-config or --validate-map"));
    }
    if (config.map_path.has_value() || config.connect_endpoint.has_value() ||
        config.script_root.has_value()) {
      return std::unexpected(stellar::platform::Error(
          "--validate-display does not require --map, --connect, or --script-root"));
    }
  }
  if (config.connect_endpoint.has_value() && config.script_root.has_value()) {
    return std::unexpected(stellar::platform::Error(
        "--script-root applies only to local --map authoritative runtime, not --connect"));
  }
  if (config.connect_endpoint.has_value()) {
    if (auto parsed = stellar::network::parse_socket_address(*config.connect_endpoint); !parsed) {
      return std::unexpected(stellar::platform::Error(
          "Invalid --connect endpoint: " + parsed.error().message));
    }
  }

  return config;
}

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

  if (config.connect_endpoint.has_value()) {
    if (config.validate_only) {
      return prepared;
    }
    RemoteClientRuntimeConfig remote_config{};
    remote_config.connect_endpoint = *config.connect_endpoint;
    remote_config.client_name = config.client_name;
    auto remote = RemoteClientRuntime::connect(std::move(remote_config));
    if (!remote) {
      return std::unexpected(stellar::platform::Error(
          "Failed to connect remote client: " + remote.error().message));
    }
    prepared.remote_runtime =
        std::make_unique<RemoteClientRuntime>(std::move(*remote));
    return prepared;
  }

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

      NetworkedClientRuntimeConfig runtime_config{};
      runtime_config.bridge.map_identity =
          stellar::network::make_map_identity(*prepared.runtime_world);

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
          std::move(*scripted_session), runtime_config);
    } else {
      NetworkedClientRuntimeConfig runtime_config{};
      runtime_config.bridge.map_identity =
          stellar::network::make_map_identity(*prepared.runtime_world);
      prepared.networked_runtime =
          std::make_unique<NetworkedClientRuntime>(*prepared.runtime_world, runtime_config);
    }
  }

  return prepared;
}

} // namespace stellar::client
