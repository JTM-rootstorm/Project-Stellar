#include "stellar/authority/AuthorityBootstrap.hpp"

#include "stellar/assets/WorldMetadataAsset.hpp"
#include "stellar/authority/NetworkConversion.hpp"
#include "stellar/import/bsp/Validation.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>
#include <utility>

namespace stellar::authority {
namespace {

[[nodiscard]] stellar::platform::Error error(std::string message) {
    return stellar::platform::Error(std::move(message));
}

[[nodiscard]] std::string lowercase_extension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension;
}

[[nodiscard]] bool contains_script_id(const std::vector<std::string>& script_ids,
                                      const std::string& script_id) noexcept {
    return std::ranges::find(script_ids, script_id) != script_ids.end();
}

[[nodiscard]] bool is_ascii_alpha(char value) noexcept {
    return std::isalpha(static_cast<unsigned char>(value)) != 0;
}

[[nodiscard]] bool script_id_has_path_escape(std::string_view script_id) noexcept {
    if (script_id.empty() || script_id.front() == '/' || script_id.front() == '\\') {
        return true;
    }
    if (script_id.size() >= 2 && is_ascii_alpha(script_id[0]) && script_id[1] == ':') {
        return true;
    }
    std::size_t segment_begin = 0;
    for (std::size_t index = 0; index <= script_id.size(); ++index) {
        if (index == script_id.size() || script_id[index] == '/' || script_id[index] == '\\') {
            if (script_id.substr(segment_begin, index - segment_begin) == "..") {
                return true;
            }
            segment_begin = index + 1;
        }
    }
    return false;
}

[[nodiscard]] std::filesystem::path normalized_absolute_root(const AuthorityLoadConfig& config) {
    const std::filesystem::path root = config.script_root.has_value()
                                           ? *config.script_root
                                           : config.map_path.parent_path();
    return std::filesystem::absolute(root.empty() ? std::filesystem::path{"."} : root)
        .lexically_normal();
}

[[nodiscard]] bool relative_path_stays_within_root(const std::filesystem::path& relative) {
    if (relative.is_absolute()) {
        return false;
    }
    for (const auto& component : relative.lexically_normal()) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::expected<std::string, stellar::platform::Error> read_text_file(
    const std::filesystem::path& path,
    const std::string& script_id) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::unexpected(
            error("Missing script source for id: " + script_id + " at " + path.string()));
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    if (!file.good() && !file.eof()) {
        return std::unexpected(
            error("Failed to read script source for id: " + script_id + " at " + path.string()));
    }
    return stream.str();
}

} // namespace

std::vector<std::string> script_ids_for_runtime_world(const stellar::world::RuntimeWorld& world) {
    std::vector<std::string> script_ids;
    for (const stellar::assets::WorldMarker& marker : world.world_metadata.markers) {
        if ((marker.type != stellar::assets::WorldMarkerType::kTrigger &&
             marker.type != stellar::assets::WorldMarkerType::kObjectCollider) ||
            !marker.script.has_value()) {
            continue;
        }
        if (!contains_script_id(script_ids, marker.script->script_id)) {
            script_ids.push_back(marker.script->script_id);
        }
    }
    return script_ids;
}

bool runtime_world_has_script_bindings(const stellar::world::RuntimeWorld& world) noexcept {
    for (const stellar::assets::WorldMarker& marker : world.world_metadata.markers) {
        if ((marker.type == stellar::assets::WorldMarkerType::kTrigger ||
             marker.type == stellar::assets::WorldMarkerType::kObjectCollider) &&
            marker.script.has_value()) {
            return true;
        }
    }
    return false;
}

stellar::world::RuntimeWorld build_authority_runtime_world(
    const stellar::assets::LevelAsset& level) {
    return stellar::world::build_runtime_world(level);
}

std::expected<AuthorityScriptLoadResult, stellar::platform::Error> load_authority_scripts(
    const stellar::world::RuntimeWorld& world,
    const AuthorityLoadConfig& config) {
    AuthorityScriptLoadResult result;
    result.discovered_script_ids = script_ids_for_runtime_world(world);
    if (result.discovered_script_ids.empty()) {
        return result;
    }

    const std::filesystem::path root = normalized_absolute_root(config);
    for (const std::string& script_id : result.discovered_script_ids) {
        if (script_id_has_path_escape(script_id)) {
            return std::unexpected(error("Script id escapes configured script root: " + script_id));
        }
        const std::filesystem::path relative = std::filesystem::path(script_id).lexically_normal();
        if (!relative_path_stays_within_root(relative)) {
            return std::unexpected(
                error("Script id escapes configured script root after normalization: " + script_id));
        }
        const std::filesystem::path script_path = (root / relative).lexically_normal();
        const std::filesystem::path relative_to_root = script_path.lexically_relative(root);
        if (relative_to_root.empty() || !relative_path_stays_within_root(relative_to_root)) {
            return std::unexpected(
                error("Script source path escapes configured script root: " + script_id));
        }
        auto source = read_text_file(script_path, script_id);
        if (!source) {
            return std::unexpected(source.error());
        }
        result.registry.set_script(script_id, std::move(*source));
        result.loaded_script_ids.push_back(script_id);
    }
    return result;
}

std::expected<PreparedAuthority, stellar::platform::Error> prepare_authority(
    const AuthorityLoadConfig& config) {
    if (lowercase_extension(config.map_path) != ".bsp") {
        return std::unexpected(error("Unsupported map extension for --map (expected .bsp)"));
    }

    auto validation = stellar::import::bsp::validate_level(config.map_path.string());
    if (!validation) {
        return std::unexpected(validation.error());
    }
    AuthorityValidationResult validation_result{.map_validation_report = validation->report};
    if (!validation->valid || !validation->loaded_level.has_value()) {
        for (const auto& diagnostic : validation->report.diagnostics) {
            if (diagnostic.severity == stellar::import::bsp::DiagnosticSeverity::kError) {
                return std::unexpected(error(diagnostic.message));
            }
        }
        return std::unexpected(
            error("BSP map validation failed for --map: " + config.map_path.string()));
    }

    auto level = std::make_unique<stellar::assets::LevelAsset>(
        std::move(validation->loaded_level->asset));
    auto world = std::make_unique<stellar::world::RuntimeWorld>(
        build_authority_runtime_world(*level));
    validation_result.runtime_world_diagnostics = world->diagnostics;
    stellar::network::MapIdentity identity = make_map_identity(*world);

    auto scripts = load_authority_scripts(*world, config);
    if (!scripts) {
        return std::unexpected(scripts.error());
    }

    if (!scripts->discovered_script_ids.empty()) {
        auto scripted = stellar::scripting::ScriptedWorldSession::create(
            *world, config.session_config, std::move(scripts->registry));
        if (!scripted) {
            scripts->script_errors.push_back(scripted.error());
            return std::unexpected(error("Failed to load authoritative scripts for --map: " +
                                         scripted.error().message));
        }
        return PreparedAuthority{
            .level = std::move(level),
            .world = std::move(world),
            .map_identity = std::move(identity),
            .session = std::variant<stellar::server::WorldSession,
                                    stellar::scripting::ScriptedWorldSession>(
                std::in_place_type<stellar::scripting::ScriptedWorldSession>, std::move(*scripted)),
            .validation = std::move(validation_result),
            .scripts = std::move(*scripts),
            .scripted_runtime_enabled = true};
    }

    std::variant<stellar::server::WorldSession, stellar::scripting::ScriptedWorldSession> session(
        std::in_place_type<stellar::server::WorldSession>, *world, config.session_config);
    return PreparedAuthority{.level = std::move(level),
                             .world = std::move(world),
                             .map_identity = std::move(identity),
                             .session = std::move(session),
                             .validation = std::move(validation_result),
                             .scripts = std::move(*scripts),
                             .scripted_runtime_enabled = false};
}

} // namespace stellar::authority
