#include "ImporterPrivate.hpp"

#include "stellar/scene/AnimationRuntime.hpp"

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace stellar::import::gltf {
namespace {

using Mat4 = std::array<float, 16>;
using Vec3 = std::array<float, 3>;
using Quat = std::array<float, 4>;

bool starts_with(std::string_view value, std::string_view prefix) noexcept {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

void skip_ws(std::string_view json, std::size_t& index) noexcept {
    while (index < json.size() &&
           (json[index] == ' ' || json[index] == '\n' || json[index] == '\r' ||
            json[index] == '\t')) {
        ++index;
    }
}

bool append_escaped_json_char(std::string& out, std::string_view json, std::size_t& index) {
    if (index >= json.size()) {
        return false;
    }
    const char escaped = json[index++];
    switch (escaped) {
        case '"':
        case '\\':
        case '/':
            out.push_back(escaped);
            return true;
        case 'b':
            out.push_back('\b');
            return true;
        case 'f':
            out.push_back('\f');
            return true;
        case 'n':
            out.push_back('\n');
            return true;
        case 'r':
            out.push_back('\r');
            return true;
        case 't':
            out.push_back('\t');
            return true;
        case 'u':
            if (index + 4 > json.size()) {
                return false;
            }
            out.push_back('?');
            index += 4;
            return true;
        default:
            return false;
    }
}

std::optional<std::string> parse_json_string(std::string_view json, std::size_t& index) {
    skip_ws(json, index);
    if (index >= json.size() || json[index] != '"') {
        return std::nullopt;
    }
    ++index;
    std::string out;
    while (index < json.size()) {
        const char ch = json[index++];
        if (ch == '"') {
            return out;
        }
        if (ch == '\\') {
            if (!append_escaped_json_char(out, json, index)) {
                return std::nullopt;
            }
        } else {
            out.push_back(ch);
        }
    }
    return std::nullopt;
}

bool skip_json_string(std::string_view json, std::size_t& index) {
    return parse_json_string(json, index).has_value();
}

bool skip_json_value(std::string_view json, std::size_t& index);

bool skip_json_container(std::string_view json, std::size_t& index, char open, char close) {
    skip_ws(json, index);
    if (index >= json.size() || json[index] != open) {
        return false;
    }
    ++index;
    skip_ws(json, index);
    if (index < json.size() && json[index] == close) {
        ++index;
        return true;
    }
    while (index < json.size()) {
        if (open == '{') {
            if (!skip_json_string(json, index)) {
                return false;
            }
            skip_ws(json, index);
            if (index >= json.size() || json[index] != ':') {
                return false;
            }
            ++index;
        }
        if (!skip_json_value(json, index)) {
            return false;
        }
        skip_ws(json, index);
        if (index < json.size() && json[index] == ',') {
            ++index;
            continue;
        }
        if (index < json.size() && json[index] == close) {
            ++index;
            return true;
        }
        return false;
    }
    return false;
}

bool skip_json_value(std::string_view json, std::size_t& index) {
    skip_ws(json, index);
    if (index >= json.size()) {
        return false;
    }
    if (json[index] == '"') {
        return skip_json_string(json, index);
    }
    if (json[index] == '{') {
        return skip_json_container(json, index, '{', '}');
    }
    if (json[index] == '[') {
        return skip_json_container(json, index, '[', ']');
    }
    while (index < json.size() && json[index] != ',' && json[index] != '}' && json[index] != ']') {
        ++index;
    }
    return true;
}

std::optional<std::pair<std::size_t, std::size_t>> object_member_value_bounds(
    std::string_view json,
    std::string_view member_name) {
    std::size_t index = 0;
    skip_ws(json, index);
    if (index >= json.size() || json[index] != '{') {
        return std::nullopt;
    }
    ++index;
    while (index < json.size()) {
        skip_ws(json, index);
        if (index < json.size() && json[index] == '}') {
            return std::nullopt;
        }
        auto key = parse_json_string(json, index);
        if (!key) {
            return std::nullopt;
        }
        skip_ws(json, index);
        if (index >= json.size() || json[index] != ':') {
            return std::nullopt;
        }
        ++index;
        skip_ws(json, index);
        const std::size_t value_begin = index;
        if (!skip_json_value(json, index)) {
            return std::nullopt;
        }
        if (*key == member_name) {
            return std::pair{value_begin, index};
        }
        skip_ws(json, index);
        if (index < json.size() && json[index] == ',') {
            ++index;
            continue;
        }
        if (index < json.size() && json[index] == '}') {
            return std::nullopt;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<stellar::assets::WorldScriptBinding> script_binding_from_extras(
    std::string_view extras_json) {
    const auto stellar_bounds = object_member_value_bounds(extras_json, "stellar");
    if (!stellar_bounds) {
        return std::nullopt;
    }
    const std::string_view stellar_json = extras_json.substr(
        stellar_bounds->first, stellar_bounds->second - stellar_bounds->first);
    const auto script_bounds = object_member_value_bounds(stellar_json, "script");
    if (!script_bounds) {
        return std::nullopt;
    }

    std::size_t script_index = script_bounds->first;
    auto script_id = parse_json_string(stellar_json, script_index);
    if (!script_id || script_index != script_bounds->second) {
        return std::nullopt;
    }

    stellar::assets::WorldScriptBinding binding;
    binding.script_id = std::move(*script_id);

    if (const auto table_bounds = object_member_value_bounds(stellar_json, "table")) {
        std::size_t table_index = table_bounds->first;
        auto table_name = parse_json_string(stellar_json, table_index);
        if (!table_name || table_index != table_bounds->second) {
            return std::nullopt;
        }
        binding.table_name = std::move(*table_name);
    }
    return binding;
}

Mat4 multiply(const Mat4& lhs, const Mat4& rhs) noexcept {
    Mat4 out{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            float value = 0.0F;
            for (std::size_t k = 0; k < 4; ++k) {
                value += lhs[k * 4 + row] * rhs[column * 4 + k];
            }
            out[column * 4 + row] = value;
        }
    }
    return out;
}

float column_length(const Mat4& matrix, std::size_t column) noexcept {
    const std::size_t base = column * 4;
    return std::sqrt(matrix[base] * matrix[base] + matrix[base + 1] * matrix[base + 1] +
                     matrix[base + 2] * matrix[base + 2]);
}

Quat normalize_quaternion(Quat value) noexcept {
    const float length_squared = value[0] * value[0] + value[1] * value[1] +
                                 value[2] * value[2] + value[3] * value[3];
    if (!std::isfinite(length_squared) || length_squared <= 0.00000001F) {
        return {0.0F, 0.0F, 0.0F, 1.0F};
    }
    const float inv_length = 1.0F / std::sqrt(length_squared);
    return {value[0] * inv_length, value[1] * inv_length, value[2] * inv_length,
            value[3] * inv_length};
}

Quat rotation_from_matrix(const Mat4& matrix, const Vec3& scale) noexcept {
    float m00 = scale[0] > 0.00000001F ? matrix[0] / scale[0] : 1.0F;
    float m01 = scale[1] > 0.00000001F ? matrix[4] / scale[1] : 0.0F;
    float m02 = scale[2] > 0.00000001F ? matrix[8] / scale[2] : 0.0F;
    float m10 = scale[0] > 0.00000001F ? matrix[1] / scale[0] : 0.0F;
    float m11 = scale[1] > 0.00000001F ? matrix[5] / scale[1] : 1.0F;
    float m12 = scale[2] > 0.00000001F ? matrix[9] / scale[2] : 0.0F;
    float m20 = scale[0] > 0.00000001F ? matrix[2] / scale[0] : 0.0F;
    float m21 = scale[1] > 0.00000001F ? matrix[6] / scale[1] : 0.0F;
    float m22 = scale[2] > 0.00000001F ? matrix[10] / scale[2] : 1.0F;

    const float trace = m00 + m11 + m22;
    if (trace > 0.0F) {
        const float s = std::sqrt(trace + 1.0F) * 2.0F;
        return normalize_quaternion({(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s,
                                     0.25F * s});
    }
    if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0F + m00 - m11 - m22) * 2.0F;
        return normalize_quaternion({0.25F * s, (m01 + m10) / s, (m02 + m20) / s,
                                     (m21 - m12) / s});
    }
    if (m11 > m22) {
        const float s = std::sqrt(1.0F + m11 - m00 - m22) * 2.0F;
        return normalize_quaternion({(m01 + m10) / s, 0.25F * s, (m12 + m21) / s,
                                     (m02 - m20) / s});
    }

    const float s = std::sqrt(1.0F + m22 - m00 - m11) * 2.0F;
    return normalize_quaternion({(m02 + m20) / s, (m12 + m21) / s, 0.25F * s,
                                 (m10 - m01) / s});
}

std::string extras_json_for_node(const cgltf_data* data, std::size_t node_index) {
    if (!data || node_index >= data->nodes_count || !data->nodes[node_index].extras.data) {
        return {};
    }
    return data->nodes[node_index].extras.data;
}

std::optional<stellar::assets::WorldMarker> marker_from_node_name(std::string_view node_name) {
    stellar::assets::WorldMarker marker;
    if (starts_with(node_name, "SPAWN_")) {
        const std::string suffix(node_name.substr(6));
        if (suffix.empty()) {
            return std::nullopt;
        }
        marker.name = suffix;
        if (suffix == "Player") {
            marker.type = stellar::assets::WorldMarkerType::kPlayerSpawn;
        } else {
            marker.type = stellar::assets::WorldMarkerType::kEntitySpawn;
            marker.archetype = suffix;
        }
        return marker;
    }
    if (starts_with(node_name, "TRIGGER_")) {
        marker.name = std::string(node_name.substr(8));
        marker.type = stellar::assets::WorldMarkerType::kTrigger;
        return marker.name.empty() ? std::nullopt : std::optional(marker);
    }
    if (starts_with(node_name, "SPRITE_")) {
        marker.name = std::string(node_name.substr(7));
        marker.type = stellar::assets::WorldMarkerType::kSprite;
        return marker.name.empty() ? std::nullopt : std::optional(marker);
    }
    if (starts_with(node_name, "PORTAL_")) {
        marker.name = std::string(node_name.substr(7));
        marker.type = stellar::assets::WorldMarkerType::kPortal;
        return marker.name.empty() ? std::nullopt : std::optional(marker);
    }
    if (starts_with(node_name, "COLLIDER_")) {
        marker.name = std::string(node_name.substr(9));
        marker.type = stellar::assets::WorldMarkerType::kObjectCollider;
        return marker.name.empty() ? std::nullopt : std::optional(marker);
    }
    return std::nullopt;
}

std::vector<std::size_t> metadata_roots(const stellar::assets::SceneAsset& scene) {
    if (scene.default_scene_index.has_value() &&
        *scene.default_scene_index < scene.scenes.size()) {
        return scene.scenes[*scene.default_scene_index].root_nodes;
    }

    std::vector<std::size_t> roots;
    for (std::size_t i = 0; i < scene.nodes.size(); ++i) {
        if (!scene.nodes[i].parent_index.has_value()) {
            roots.push_back(i);
        }
    }
    return roots;
}

std::expected<void, stellar::platform::Error>
walk_metadata_nodes(const stellar::assets::SceneAsset& scene,
                    const cgltf_data* data,
                    stellar::assets::WorldMetadataAsset& metadata,
                    std::size_t node_index,
                    const Mat4& parent_world,
                    std::vector<bool>& visited) {
    if (node_index >= scene.nodes.size()) {
        return std::unexpected(
            stellar::platform::Error("Scene root node index exceeds node count"));
    }
    if (visited[node_index]) {
        return {};
    }
    visited[node_index] = true;

    const auto& node = scene.nodes[node_index];
    const Mat4 local = stellar::scene::compose_transform(node.local_transform);
    const Mat4 world = multiply(parent_world, local);
    if (auto marker = marker_from_node_name(node.name)) {
        marker->position = {world[12], world[13], world[14]};
        marker->scale = {std::abs(column_length(world, 0)), std::abs(column_length(world, 1)),
                         std::abs(column_length(world, 2))};
        marker->rotation = rotation_from_matrix(world, marker->scale);
        marker->extras_json = extras_json_for_node(data, node_index);
        marker->script = script_binding_from_extras(marker->extras_json);
        metadata.markers.push_back(std::move(*marker));
    }

    for (std::size_t child : node.children) {
        auto result = walk_metadata_nodes(scene, data, metadata, child, world, visited);
        if (!result) {
            return result;
        }
    }

    return {};
}

} // namespace

std::expected<stellar::assets::WorldMetadataAsset, stellar::platform::Error>
extract_world_metadata(const stellar::assets::SceneAsset& scene, const cgltf_data* data) {
    stellar::assets::WorldMetadataAsset metadata;
    std::vector<bool> visited(scene.nodes.size(), false);
    for (std::size_t root : metadata_roots(scene)) {
        auto result = walk_metadata_nodes(scene, data, metadata, root, identity_matrix(), visited);
        if (!result) {
            return std::unexpected(result.error());
        }
    }
    return metadata;
}

} // namespace stellar::import::gltf
