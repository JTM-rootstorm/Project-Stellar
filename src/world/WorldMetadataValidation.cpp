#include "stellar/world/WorldMetadataValidation.hpp"

#include "stellar/world/RuntimeWorld.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>
#include <unordered_map>

namespace stellar::world {
namespace {

constexpr float kNearZeroExtent = 1.0e-6F;

bool is_finite(float value) noexcept {
    return std::isfinite(value);
}

template <std::size_t N>
bool is_finite_array(const std::array<float, N>& value) noexcept {
    for (float element : value) {
        if (!is_finite(element)) {
            return false;
        }
    }
    return true;
}

bool trigger_has_empty_extent(const stellar::assets::WorldMarker& marker) noexcept {
    for (float extent : marker.scale) {
        if (std::fabs(extent) <= kNearZeroExtent) {
            return true;
        }
    }
    return false;
}

bool trigger_has_large_extent(const stellar::assets::WorldMarker& marker) noexcept {
    for (float extent : marker.scale) {
        if (std::fabs(extent) > kWorldMetadataValidationLargeTriggerExtentThreshold) {
            return true;
        }
    }
    return false;
}

bool is_ascii_alpha(char value) noexcept {
    return std::isalpha(static_cast<unsigned char>(value)) != 0;
}

bool path_segment_is_parent(std::string_view segment) noexcept {
    return segment == "..";
}

void add_finding(WorldMetadataValidationReport& report,
                 WorldMetadataValidationSeverity severity,
                 std::string code,
                 std::string message,
                 std::size_t marker_index);

bool script_id_has_path_escape(std::string_view script_id) noexcept {
    if (script_id.empty()) {
        return false;
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
            if (path_segment_is_parent(script_id.substr(segment_begin, index - segment_begin))) {
                return true;
            }
            segment_begin = index + 1;
        }
    }
    return false;
}

void validate_script_binding(const stellar::assets::WorldMarker& marker,
                             const WorldMetadataValidationConfig& config,
                             WorldMetadataValidationReport& report,
                             std::size_t marker_index) {
    if (!marker.script.has_value()) {
        return;
    }

    if (marker.script->script_id.empty()) {
        add_finding(report,
                    WorldMetadataValidationSeverity::kError,
                    "script_binding_empty_script_id",
                    "World metadata script binding has an empty script id",
                    marker_index);
    } else if (script_id_has_path_escape(marker.script->script_id)) {
        add_finding(report,
                    WorldMetadataValidationSeverity::kError,
                    "script_binding_path_traversal",
                    "World metadata script binding uses an absolute path or parent path escape",
                    marker_index);
    }

    if (marker.script->table_name.empty()) {
        add_finding(report,
                    WorldMetadataValidationSeverity::kWarning,
                    "script_binding_empty_table_name",
                    "World metadata script binding has an empty Lua table name",
                    marker_index);
    }

    if (config.warn_script_binding_unsupported_marker_types &&
        marker.type != stellar::assets::WorldMarkerType::kTrigger) {
        add_finding(report,
                    WorldMetadataValidationSeverity::kWarning,
                    "script_binding_unsupported_marker_type",
                    "World metadata script binding is attached to a marker type without runtime "
                    "script invocation support",
                    marker_index);
    }
}

void add_finding(WorldMetadataValidationReport& report,
                 WorldMetadataValidationSeverity severity,
                 std::string code,
                 std::string message,
                 std::size_t marker_index) {
    report.findings.push_back(WorldMetadataValidationFinding{.severity = severity,
                                                             .code = std::move(code),
                                                             .message = std::move(message),
                                                             .marker_index = marker_index});
    report.has_errors = report.has_errors || severity == WorldMetadataValidationSeverity::kError;
}

bool finding_less(const WorldMetadataValidationFinding& lhs,
                  const WorldMetadataValidationFinding& rhs) noexcept {
    if (lhs.marker_index != rhs.marker_index) {
        return lhs.marker_index < rhs.marker_index;
    }
    return lhs.code < rhs.code;
}

} // namespace

WorldMetadataValidationReport validate_world_metadata(
    const stellar::assets::WorldMetadataAsset& metadata,
    const WorldMetadataValidationConfig& config) {
    WorldMetadataValidationReport report;
    std::size_t player_spawn_count = 0;
    std::unordered_map<std::string, std::size_t> first_trigger_name_indices;
    std::unordered_map<std::string, std::size_t> first_sprite_name_indices;

    for (std::size_t marker_index = 0; marker_index < metadata.markers.size(); ++marker_index) {
        const auto& marker = metadata.markers[marker_index];

        if (!is_finite_array(marker.position)) {
            add_finding(report,
                        WorldMetadataValidationSeverity::kError,
                        "non_finite_position",
                        "World metadata marker position contains a non-finite value",
                        marker_index);
        }
        if (!is_finite_array(marker.rotation)) {
            add_finding(report,
                        WorldMetadataValidationSeverity::kError,
                        "non_finite_rotation",
                        "World metadata marker rotation contains a non-finite value",
                        marker_index);
        }
        if (!is_finite_array(marker.scale)) {
            add_finding(report,
                        WorldMetadataValidationSeverity::kError,
                        "non_finite_scale",
                        "World metadata marker scale contains a non-finite value",
                        marker_index);
        }

        if (!marker.extras_json.empty()) {
            add_finding(report,
                        WorldMetadataValidationSeverity::kWarning,
                        "raw_extras_json_unparsed",
                        "World metadata marker contains raw extras_json that runtime validation does "
                        "not parse",
                        marker_index);
        }

        validate_script_binding(marker, config, report, marker_index);

        switch (marker.type) {
            case stellar::assets::WorldMarkerType::kPlayerSpawn:
                ++player_spawn_count;
                if (config.warn_multiple_player_spawns && player_spawn_count > 1) {
                    add_finding(report,
                                WorldMetadataValidationSeverity::kWarning,
                                "multiple_player_spawns",
                                "World metadata contains more than one player spawn marker",
                                marker_index);
                }
                break;
            case stellar::assets::WorldMarkerType::kEntitySpawn:
                if (config.warn_empty_entity_archetypes && marker.archetype.empty()) {
                    add_finding(report,
                                WorldMetadataValidationSeverity::kError,
                                "empty_entity_archetype",
                                "Entity spawn marker has an empty archetype",
                                marker_index);
                }
                break;
            case stellar::assets::WorldMarkerType::kTrigger:
                if (marker.name.empty()) {
                    add_finding(report,
                                WorldMetadataValidationSeverity::kWarning,
                                "empty_trigger_name",
                                "Trigger marker has an empty name",
                                marker_index);
                } else {
                    const auto [it, inserted] =
                        first_trigger_name_indices.emplace(marker.name, marker_index);
                    if (!inserted) {
                        add_finding(report,
                                    WorldMetadataValidationSeverity::kWarning,
                                    "duplicate_trigger_name",
                                    "Trigger marker name duplicates an earlier trigger: " + marker.name,
                                    marker_index);
                        (void)it;
                    }
                }
                if (config.warn_empty_trigger_extents && trigger_has_empty_extent(marker)) {
                    add_finding(report,
                                WorldMetadataValidationSeverity::kWarning,
                                "empty_trigger_extents",
                                "Trigger marker has zero or empty extents",
                                marker_index);
                }
                if (trigger_has_large_extent(marker)) {
                    add_finding(report,
                                WorldMetadataValidationSeverity::kWarning,
                                "large_trigger_extents",
                                "Trigger marker extents exceed the documented 100000 world-unit "
                                "threshold",
                                marker_index);
                }
                break;
            case stellar::assets::WorldMarkerType::kSprite:
                if (config.warn_empty_sprite_names && marker.name.empty()) {
                    add_finding(report,
                                WorldMetadataValidationSeverity::kWarning,
                                "empty_sprite_name",
                                "Sprite marker has an empty name",
                                marker_index);
                } else if (!marker.name.empty()) {
                    const auto [it, inserted] =
                        first_sprite_name_indices.emplace(marker.name, marker_index);
                    if (!inserted) {
                        add_finding(report,
                                    WorldMetadataValidationSeverity::kWarning,
                                    "duplicate_sprite_name",
                                    "Sprite marker name duplicates an earlier sprite: " + marker.name,
                                    marker_index);
                        (void)it;
                    }
                }
                break;
            case stellar::assets::WorldMarkerType::kPortal:
                if (marker.name.empty()) {
                    add_finding(report,
                                WorldMetadataValidationSeverity::kWarning,
                                "empty_portal_name",
                                "Portal marker has an empty name",
                                marker_index);
                }
                add_finding(report,
                            WorldMetadataValidationSeverity::kWarning,
                            "portal_runtime_deferred",
                            "Portal marker is present, but portal runtime behavior is deferred",
                            marker_index);
                break;
        }
    }

    if (config.require_player_spawn && player_spawn_count == 0) {
        add_finding(report,
                    WorldMetadataValidationSeverity::kError,
                    "missing_player_spawn",
                    "World metadata contains no player spawn marker",
                    kWorldMetadataValidationInvalidIndex);
    }

    std::sort(report.findings.begin(), report.findings.end(), finding_less);
    return report;
}

WorldMetadataValidationReport validate_world_metadata(
    const RuntimeWorld& world,
    const WorldMetadataValidationConfig& config) {
    return validate_world_metadata(world.world_metadata, config);
}

} // namespace stellar::world
