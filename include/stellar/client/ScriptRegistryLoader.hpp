#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "stellar/platform/Error.hpp"
#include "stellar/scripting/ScriptError.hpp"
#include "stellar/scripting/ScriptRegistry.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::client {

/** @brief Configuration for loading authoritative script sources for a runtime world. */
struct ScriptRegistryLoadConfig {
    /** @brief BSP map path whose parent is the default script source root. */
    std::filesystem::path map_path;

    /** @brief Optional explicit root for asset-relative script ids. */
    std::optional<std::filesystem::path> script_root;
};

/** @brief Result of loading a script registry for script-bound world metadata. */
struct ScriptRegistryLoadResult {
    /** @brief In-memory script registry populated from referenced source files. */
    stellar::scripting::ScriptRegistry registry;

    /** @brief Deterministically ordered script ids loaded from disk. */
    std::vector<std::string> loaded_script_ids;

    /** @brief Non-fatal script diagnostics collected while loading, if any. */
    std::vector<stellar::scripting::ScriptError> script_errors;
};

/** @brief Return unique trigger/object-collider script ids referenced by a runtime world. */
[[nodiscard]] std::vector<std::string> script_ids_for_runtime_world(
    const stellar::world::RuntimeWorld& world);

/** @brief Return true when a runtime world has trigger/object-collider script bindings. */
[[nodiscard]] bool runtime_world_has_script_bindings(
    const stellar::world::RuntimeWorld& world) noexcept;

/** @brief Load authoritative script sources for script-bound runtime world metadata. */
[[nodiscard]] std::expected<ScriptRegistryLoadResult, stellar::platform::Error>
load_script_registry_for_world(const stellar::world::RuntimeWorld& world,
                               const ScriptRegistryLoadConfig& config);

} // namespace stellar::client
