#include "stellar/client/Application.hpp"

#include "stellar/authority/AuthorityBootstrap.hpp"
#include "stellar/network/SocketTransport.hpp"

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace stellar::client {

namespace {

[[nodiscard]] stellar::authority::AuthorityLoadConfig make_authority_load_config(
    const ApplicationConfig &config) {
  stellar::authority::AuthorityLoadConfig load_config;
  load_config.map_path = *config.map_path;
  if (config.script_root.has_value()) {
    load_config.script_root = *config.script_root;
  }
  return load_config;
}

void fill_validation_from_authority(
    ApplicationValidation &validation,
    const stellar::authority::PreparedAuthority &authority) {
  validation.level = *authority.level;
  validation.map_validation_report = authority.validation.map_validation_report;
  validation.runtime_world_diagnostics = authority.validation.runtime_world_diagnostics;
  validation.loaded_script_ids = authority.scripts.loaded_script_ids;
  validation.script_errors = authority.scripts.script_errors;
  validation.scripted_runtime_enabled = authority.scripted_runtime_enabled;
}

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

  auto authority = stellar::authority::prepare_authority(make_authority_load_config(config));
  if (!authority) {
    return std::unexpected(authority.error());
  }
  fill_validation_from_authority(validation, *authority);
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
  return validation;
}

std::expected<PreparedApplicationRuntime, stellar::platform::Error>
prepare_application_runtime(const ApplicationConfig &config) {
  PreparedApplicationRuntime prepared;

  if (config.connect_endpoint.has_value()) {
    auto validation = load_application_validation(config);
    if (!validation) {
      return std::unexpected(validation.error());
    }
    prepared.validation =
        std::make_unique<ApplicationValidation>(std::move(*validation));
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

  if (!config.map_path.has_value()) {
    prepared.validation = std::make_unique<ApplicationValidation>();
    return prepared;
  }

  {
    auto authority = stellar::authority::prepare_authority(make_authority_load_config(config));
    if (!authority) {
      return std::unexpected(authority.error());
    }

    ApplicationValidation validation;
    fill_validation_from_authority(validation, *authority);
    prepared.validation = std::make_unique<ApplicationValidation>(std::move(validation));

    // Keep a presentation/runtime-world copy backed by ApplicationValidation::level so mapped
    // client callers have stable display-free world data independent of retained authority state.
    prepared.runtime_world = std::make_unique<stellar::world::RuntimeWorld>(
        stellar::authority::build_authority_runtime_world(*prepared.validation->level));

    NetworkedClientRuntimeConfig runtime_config{};
    runtime_config.server.map_identity = authority->map_identity;
    if (auto* scripted = std::get_if<stellar::scripting::ScriptedWorldSession>(
            &authority->session)) {
      prepared.networked_runtime =
          std::make_unique<NetworkedClientRuntime>(std::move(*scripted), runtime_config);
      prepared.authority = std::make_unique<stellar::authority::PreparedAuthority>(
          std::move(*authority));
    } else {
      prepared.networked_runtime =
          std::make_unique<NetworkedClientRuntime>(*prepared.runtime_world, runtime_config);
    }
  }

  return prepared;
}

} // namespace stellar::client
