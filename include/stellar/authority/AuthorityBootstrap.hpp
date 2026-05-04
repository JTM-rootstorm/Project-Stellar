#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "stellar/assets/LevelAsset.hpp"
#include "stellar/import/bsp/Diagnostics.hpp"
#include "stellar/network/Messages.hpp"
#include "stellar/network/Session.hpp"
#include "stellar/platform/Error.hpp"
#include "stellar/scripting/ScriptError.hpp"
#include "stellar/scripting/ScriptRegistry.hpp"
#include "stellar/scripting/ScriptedWorldSession.hpp"
#include "stellar/server/WorldSession.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::authority {

/** @brief Configuration for loading and preparing an authoritative BSP runtime. */
struct AuthorityLoadConfig {
    /** @brief BSP map path loaded by the authoritative runtime. */
    std::filesystem::path map_path;

    /** @brief Optional sandbox root used for asset-relative authoritative script ids. */
    std::optional<std::filesystem::path> script_root;

    /** @brief World session rules used when constructing authority session state. */
    stellar::server::WorldSessionConfig session_config{};
};

/** @brief Display-free map/runtime validation data produced by authority bootstrap. */
struct AuthorityValidationResult {
    /** @brief BSP validation diagnostics for the configured map. */
    std::optional<stellar::import::bsp::ImportReport> map_validation_report;

    /** @brief Diagnostics from converting imported level data into runtime world data. */
    std::optional<stellar::world::RuntimeWorldDiagnostics> runtime_world_diagnostics;
};

/** @brief Result of sandboxed authoritative script source discovery and loading. */
struct AuthorityScriptLoadResult {
    /** @brief In-memory script registry populated from referenced source files. */
    stellar::scripting::ScriptRegistry registry;

    /** @brief Deterministically ordered script ids discovered in the runtime world. */
    std::vector<std::string> discovered_script_ids;

    /** @brief Deterministically ordered script ids loaded from disk. */
    std::vector<std::string> loaded_script_ids;

    /** @brief Non-fatal script diagnostics observed during bootstrap. */
    std::vector<stellar::scripting::ScriptError> script_errors;
};

/** @brief Fully prepared authoritative map, runtime world, map id, and session state. */
struct PreparedAuthority {
    /** @brief Loaded BSP level asset that owns source map data for the runtime world. */
    std::unique_ptr<stellar::assets::LevelAsset> level;

    /** @brief Runtime world assembled from the loaded BSP level. */
    std::unique_ptr<stellar::world::RuntimeWorld> world;

    /** @brief Stable protocol map identity derived from the runtime world. */
    stellar::network::MapIdentity map_identity;

    /** @brief Authority session, scripted when the map references Lua scripts. */
    std::variant<stellar::server::WorldSession, stellar::scripting::ScriptedWorldSession> session;

    /** @brief Validation diagnostics collected during bootstrap. */
    AuthorityValidationResult validation;

    /** @brief Script loading diagnostics and loaded script ids. */
    AuthorityScriptLoadResult scripts;

    /** @brief True when the prepared authority uses ScriptedWorldSession. */
    bool scripted_runtime_enabled = false;
};

/** @brief Return unique trigger/object-collider script ids referenced by a runtime world. */
[[nodiscard]] std::vector<std::string> script_ids_for_runtime_world(
    const stellar::world::RuntimeWorld& world);

/** @brief Return true when a runtime world has trigger/object-collider script bindings. */
[[nodiscard]] bool runtime_world_has_script_bindings(
    const stellar::world::RuntimeWorld& world) noexcept;

/** @brief Build authority-compatible runtime world data for a caller-owned level asset. */
[[nodiscard]] stellar::world::RuntimeWorld build_authority_runtime_world(
    const stellar::assets::LevelAsset& level);

/** @brief Load authoritative script sources with script-root sandbox enforcement. */
[[nodiscard]] std::expected<AuthorityScriptLoadResult, stellar::platform::Error>
load_authority_scripts(const stellar::world::RuntimeWorld& world,
                       const AuthorityLoadConfig& config);

/** @brief Validate a BSP map, build runtime state, load scripts, and construct authority session. */
[[nodiscard]] std::expected<PreparedAuthority, stellar::platform::Error> prepare_authority(
    const AuthorityLoadConfig& config);

} // namespace stellar::authority
