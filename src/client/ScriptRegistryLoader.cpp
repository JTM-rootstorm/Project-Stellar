#include "stellar/client/ScriptRegistryLoader.hpp"

#include "stellar/assets/WorldMetadataAsset.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>

namespace stellar::client {
namespace {

[[nodiscard]] bool contains_script_id(const std::vector<std::string>& script_ids,
                                      const std::string& script_id) noexcept {
    return std::ranges::find(script_ids, script_id) != script_ids.end();
}

[[nodiscard]] bool is_ascii_alpha(char value) noexcept {
    return std::isalpha(static_cast<unsigned char>(value)) != 0;
}

[[nodiscard]] bool script_id_has_path_escape(std::string_view script_id) noexcept {
    if (script_id.empty()) {
        return true;
    }
    if (script_id.front() == '/' || script_id.front() == '\\') {
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

[[nodiscard]] std::filesystem::path normalized_absolute_root(
    const ScriptRegistryLoadConfig& config) {
    const std::filesystem::path root = config.script_root.has_value()
                                      ? *config.script_root
                                      : config.map_path.parent_path();
    return std::filesystem::absolute(root.empty() ? std::filesystem::path{"."} : root)
        .lexically_normal();
}

[[nodiscard]] bool relative_path_stays_within_root(const std::filesystem::path& relative) {
    for (const auto& component : relative.lexically_normal()) {
        if (component == "..") {
            return false;
        }
    }
    return !relative.is_absolute();
}

[[nodiscard]] std::expected<std::string, stellar::platform::Error> read_text_file(
    const std::filesystem::path& path,
    const std::string& script_id) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::unexpected(stellar::platform::Error(
            "Missing script source for id: " + script_id + " at " + path.string()));
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    if (!file.good() && !file.eof()) {
        return std::unexpected(stellar::platform::Error(
            "Failed to read script source for id: " + script_id + " at " + path.string()));
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

std::expected<ScriptRegistryLoadResult, stellar::platform::Error> load_script_registry_for_world(
    const stellar::world::RuntimeWorld& world,
    const ScriptRegistryLoadConfig& config) {
    ScriptRegistryLoadResult result;
    const std::vector<std::string> script_ids = script_ids_for_runtime_world(world);
    if (script_ids.empty()) {
        return result;
    }

    const std::filesystem::path root = normalized_absolute_root(config);
    for (const std::string& script_id : script_ids) {
        if (script_id_has_path_escape(script_id)) {
            return std::unexpected(stellar::platform::Error(
                "Script id escapes configured script root: " + script_id));
        }

        const std::filesystem::path relative = std::filesystem::path(script_id).lexically_normal();
        if (!relative_path_stays_within_root(relative)) {
            return std::unexpected(stellar::platform::Error(
                "Script id escapes configured script root after normalization: " + script_id));
        }

        const std::filesystem::path script_path = (root / relative).lexically_normal();
        const std::filesystem::path relative_to_root = script_path.lexically_relative(root);
        if (relative_to_root.empty() || !relative_path_stays_within_root(relative_to_root)) {
            return std::unexpected(stellar::platform::Error(
                "Script source path escapes configured script root: " + script_id));
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

} // namespace stellar::client
