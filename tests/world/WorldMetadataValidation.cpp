#include "stellar/world/WorldMetadataValidation.hpp"

#include "stellar/world/RuntimeWorld.hpp"

#include <cassert>
#include <limits>
#include <string>
#include <string_view>

namespace {

stellar::assets::WorldMarker make_marker(stellar::assets::WorldMarkerType type,
                                         std::string_view name = {}) {
    stellar::assets::WorldMarker marker;
    marker.type = type;
    marker.name = std::string{name};
    if (type == stellar::assets::WorldMarkerType::kPlayerSpawn && marker.name.empty()) {
        marker.name = "Player";
    }
    if (type == stellar::assets::WorldMarkerType::kEntitySpawn && !marker.name.empty()) {
        marker.archetype = marker.name;
    }
    return marker;
}

bool has_code(const stellar::world::WorldMetadataValidationReport& report,
              std::string_view code) {
    for (const auto& finding : report.findings) {
        if (finding.code == code) {
            return true;
        }
    }
    return false;
}

const stellar::world::WorldMetadataValidationFinding* find_code(
    const stellar::world::WorldMetadataValidationReport& report,
    std::string_view code) {
    for (const auto& finding : report.findings) {
        if (finding.code == code) {
            return &finding;
        }
    }
    return nullptr;
}

stellar::assets::WorldMetadataAsset metadata_with_player_spawn() {
    stellar::assets::WorldMetadataAsset metadata;
    metadata.markers.push_back(make_marker(stellar::assets::WorldMarkerType::kPlayerSpawn));
    return metadata;
}

void empty_metadata_requires_player_spawn_by_default() {
    const stellar::assets::WorldMetadataAsset metadata;
    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(report.has_errors);
    assert(report.findings.size() == 1);
    assert(report.findings[0].code == "missing_player_spawn");
    assert(report.findings[0].marker_index ==
           stellar::world::kWorldMetadataValidationInvalidIndex);
}

void missing_player_spawn_can_be_warning_free_when_policy_disabled() {
    stellar::world::WorldMetadataValidationConfig config;
    config.require_player_spawn = false;

    const stellar::assets::WorldMetadataAsset metadata;
    const auto report = stellar::world::validate_world_metadata(metadata, config);

    assert(!report.has_errors);
    assert(report.findings.empty());
}

void single_player_spawn_passes() {
    const auto report = stellar::world::validate_world_metadata(metadata_with_player_spawn());

    assert(!report.has_errors);
    assert(report.findings.empty());
}

void multiple_player_spawns_warn() {
    auto metadata = metadata_with_player_spawn();
    metadata.markers.push_back(make_marker(stellar::assets::WorldMarkerType::kPlayerSpawn));

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(!report.has_errors);
    assert(report.findings.size() == 1);
    assert(report.findings[0].severity ==
           stellar::world::WorldMetadataValidationSeverity::kWarning);
    assert(report.findings[0].code == "multiple_player_spawns");
    assert(report.findings[0].marker_index == 1);
}

void entity_spawn_empty_archetype_reports_finding() {
    auto metadata = metadata_with_player_spawn();
    metadata.markers.push_back(make_marker(stellar::assets::WorldMarkerType::kEntitySpawn));

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(report.has_errors);
    assert(has_code(report, "empty_entity_archetype"));
    assert(find_code(report, "empty_entity_archetype")->marker_index == 1);
}

void trigger_empty_name_reports_finding() {
    auto metadata = metadata_with_player_spawn();
    metadata.markers.push_back(make_marker(stellar::assets::WorldMarkerType::kTrigger));

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(!report.has_errors);
    assert(has_code(report, "empty_trigger_name"));
}

void trigger_duplicate_names_warn() {
    auto metadata = metadata_with_player_spawn();
    metadata.markers.push_back(make_marker(stellar::assets::WorldMarkerType::kTrigger, "Door"));
    metadata.markers.push_back(make_marker(stellar::assets::WorldMarkerType::kTrigger, "Door"));

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(!report.has_errors);
    assert(has_code(report, "duplicate_trigger_name"));
    assert(find_code(report, "duplicate_trigger_name")->marker_index == 2);
}

void trigger_zero_extents_warn_under_policy() {
    auto metadata = metadata_with_player_spawn();
    auto trigger = make_marker(stellar::assets::WorldMarkerType::kTrigger, "Flat");
    trigger.scale = {1.0F, 0.0F, 1.0F};
    metadata.markers.push_back(trigger);

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(!report.has_errors);
    assert(has_code(report, "empty_trigger_extents"));

    stellar::world::WorldMetadataValidationConfig config;
    config.warn_empty_trigger_extents = false;
    const auto disabled_report = stellar::world::validate_world_metadata(metadata, config);
    assert(!has_code(disabled_report, "empty_trigger_extents"));
}

void sprite_empty_name_reports_finding() {
    auto metadata = metadata_with_player_spawn();
    metadata.markers.push_back(make_marker(stellar::assets::WorldMarkerType::kSprite));

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(!report.has_errors);
    assert(has_code(report, "empty_sprite_name"));
}

void portal_marker_warns_runtime_deferred() {
    auto metadata = metadata_with_player_spawn();
    metadata.markers.push_back(make_marker(stellar::assets::WorldMarkerType::kPortal, "North"));

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(!report.has_errors);
    assert(has_code(report, "portal_runtime_deferred"));
}

void object_collider_authoring_findings_are_reported() {
    auto metadata = metadata_with_player_spawn();
    auto unnamed = make_marker(stellar::assets::WorldMarkerType::kObjectCollider);
    unnamed.scale = {1.0F, 0.0F, 1.0F};
    auto first = make_marker(stellar::assets::WorldMarkerType::kObjectCollider, "Pickup");
    first.scale = {1.0F, 2.0F, 3.0F};
    auto duplicate = make_marker(stellar::assets::WorldMarkerType::kObjectCollider, "Pickup");
    duplicate.scale = {100001.0F, 1.0F, 1.0F};
    duplicate.script = stellar::assets::WorldScriptBinding{.script_id = "scripts/pickup.lua",
                                                           .table_name = "Pickup"};
    metadata.markers.push_back(unnamed);
    metadata.markers.push_back(first);
    metadata.markers.push_back(duplicate);

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(!report.has_errors);
    assert(has_code(report, "empty_object_collider_name"));
    assert(has_code(report, "empty_object_collider_extents"));
    assert(has_code(report, "duplicate_object_collider_name"));
    assert(has_code(report, "large_object_collider_extents"));
    assert(!has_code(report, "script_binding_unsupported_marker_type"));
    assert(find_code(report, "duplicate_object_collider_name")->marker_index == 3);
}

void object_collider_script_binding_is_supported_after_phase_12d() {
    auto metadata = metadata_with_player_spawn();
    auto collider = make_marker(stellar::assets::WorldMarkerType::kObjectCollider, "Pickup");
    collider.script = stellar::assets::WorldScriptBinding{.script_id = "scripts/pickup.lua",
                                                          .table_name = "Pickup"};
    metadata.markers.push_back(collider);

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(!report.has_errors);
    assert(!has_code(report, "script_binding_unsupported_marker_type"));
}

void sprite_or_portal_script_binding_still_warns_unsupported() {
    auto metadata = metadata_with_player_spawn();
    auto sprite = make_marker(stellar::assets::WorldMarkerType::kSprite, "Sprite");
    sprite.script = stellar::assets::WorldScriptBinding{.script_id = "scripts/sprite.lua",
                                                        .table_name = "Sprite"};
    auto portal = make_marker(stellar::assets::WorldMarkerType::kPortal, "Portal");
    portal.script = stellar::assets::WorldScriptBinding{.script_id = "scripts/portal.lua",
                                                        .table_name = "Portal"};
    metadata.markers.push_back(sprite);
    metadata.markers.push_back(portal);

    const auto report = stellar::world::validate_world_metadata(metadata);

    std::size_t unsupported_count = 0;
    for (const auto& finding : report.findings) {
        if (finding.code == "script_binding_unsupported_marker_type") {
            ++unsupported_count;
        }
    }
    assert(unsupported_count == 2);
}

void script_binding_reports_empty_script_id_as_error() {
    auto metadata = metadata_with_player_spawn();
    auto trigger = make_marker(stellar::assets::WorldMarkerType::kTrigger, "Door");
    trigger.script = stellar::assets::WorldScriptBinding{.script_id = "", .table_name = "Door"};
    metadata.markers.push_back(trigger);

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(report.has_errors);
    assert(has_code(report, "script_binding_empty_script_id"));
    assert(find_code(report, "script_binding_empty_script_id")->marker_index == 1);
}

void script_binding_reports_empty_table_name_as_warning() {
    auto metadata = metadata_with_player_spawn();
    auto trigger = make_marker(stellar::assets::WorldMarkerType::kTrigger, "Door");
    trigger.script = stellar::assets::WorldScriptBinding{.script_id = "scripts/door.lua"};
    metadata.markers.push_back(trigger);

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(!report.has_errors);
    assert(has_code(report, "script_binding_empty_table_name"));
    assert(find_code(report, "script_binding_empty_table_name")->severity ==
           stellar::world::WorldMetadataValidationSeverity::kWarning);
}

void script_binding_reports_path_escape_as_error() {
    auto metadata = metadata_with_player_spawn();
    auto trigger = make_marker(stellar::assets::WorldMarkerType::kTrigger, "Door");
    trigger.script = stellar::assets::WorldScriptBinding{.script_id = "../door.lua",
                                                         .table_name = "Door"};
    metadata.markers.push_back(trigger);

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(report.has_errors);
    assert(has_code(report, "script_binding_path_traversal"));
}

void script_binding_warns_for_marker_types_without_invocation_support() {
    auto metadata = metadata_with_player_spawn();
    metadata.markers[0].script =
        stellar::assets::WorldScriptBinding{.script_id = "scripts/player.lua",
                                            .table_name = "Player"};

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(!report.has_errors);
    assert(has_code(report, "script_binding_unsupported_marker_type"));
}

void non_finite_position_rotation_scale_report_errors() {
    auto metadata = metadata_with_player_spawn();
    metadata.markers[0].position[0] = std::numeric_limits<float>::infinity();
    metadata.markers[0].rotation[1] = std::numeric_limits<float>::quiet_NaN();
    metadata.markers[0].scale[2] = std::numeric_limits<float>::infinity();

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(report.has_errors);
    assert(has_code(report, "non_finite_position"));
    assert(has_code(report, "non_finite_rotation"));
    assert(has_code(report, "non_finite_scale"));
}

void findings_are_deterministically_ordered() {
    stellar::assets::WorldMetadataAsset metadata;
    auto late = make_marker(stellar::assets::WorldMarkerType::kSprite);
    late.position[0] = std::numeric_limits<float>::quiet_NaN();
    metadata.markers.push_back(make_marker(stellar::assets::WorldMarkerType::kPlayerSpawn));
    metadata.markers.push_back(late);
    metadata.markers.push_back(make_marker(stellar::assets::WorldMarkerType::kPortal));

    const auto report = stellar::world::validate_world_metadata(metadata);

    assert(!report.findings.empty());
    for (std::size_t i = 1; i < report.findings.size(); ++i) {
        const auto& previous = report.findings[i - 1];
        const auto& current = report.findings[i];
        assert(previous.marker_index < current.marker_index ||
               (previous.marker_index == current.marker_index &&
                previous.code <= current.code));
    }
}

void runtime_world_overload_validates_copied_metadata() {
    stellar::world::RuntimeWorld world;
    world.world_metadata = metadata_with_player_spawn();

    const auto report = stellar::world::validate_world_metadata(world);

    assert(!report.has_errors);
    assert(report.findings.empty());
}

void runtime_world_validation_uses_copied_script_binding() {
    stellar::world::RuntimeWorld world;
    world.world_metadata = metadata_with_player_spawn();
    auto trigger = make_marker(stellar::assets::WorldMarkerType::kTrigger, "Door");
    trigger.script = stellar::assets::WorldScriptBinding{.script_id = "/tmp/door.lua",
                                                         .table_name = "Door"};
    world.world_metadata.markers.push_back(trigger);

    const auto report = stellar::world::validate_world_metadata(world);

    assert(report.has_errors);
    assert(has_code(report, "script_binding_path_traversal"));
}

} // namespace

int main() {
    empty_metadata_requires_player_spawn_by_default();
    missing_player_spawn_can_be_warning_free_when_policy_disabled();
    single_player_spawn_passes();
    multiple_player_spawns_warn();
    entity_spawn_empty_archetype_reports_finding();
    trigger_empty_name_reports_finding();
    trigger_duplicate_names_warn();
    trigger_zero_extents_warn_under_policy();
    sprite_empty_name_reports_finding();
    portal_marker_warns_runtime_deferred();
    object_collider_authoring_findings_are_reported();
    object_collider_script_binding_is_supported_after_phase_12d();
    sprite_or_portal_script_binding_still_warns_unsupported();
    script_binding_reports_empty_script_id_as_error();
    script_binding_reports_empty_table_name_as_warning();
    script_binding_reports_path_escape_as_error();
    script_binding_warns_for_marker_types_without_invocation_support();
    non_finite_position_rotation_scale_report_errors();
    findings_are_deterministically_ordered();
    runtime_world_overload_validates_copied_metadata();
    runtime_world_validation_uses_copied_script_binding();
    return 0;
}
