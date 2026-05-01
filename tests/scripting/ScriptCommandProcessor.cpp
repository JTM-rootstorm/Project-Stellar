#include "stellar/scripting/ScriptCommandProcessor.hpp"

#include "stellar/world/RuntimeWorld.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <utility>

namespace {

using Vec3 = std::array<float, 3>;

stellar::assets::CollisionTriangle triangle() {
    return stellar::assets::CollisionTriangle{.a = {0.0F, 0.0F, 0.0F},
                                              .b = {1.0F, 0.0F, 0.0F},
                                              .c = {0.0F, 0.0F, 1.0F},
                                              .normal = {0.0F, 1.0F, 0.0F}};
}

stellar::assets::CollisionMesh mesh(std::string name) {
    stellar::assets::CollisionMesh collision_mesh;
    collision_mesh.name = std::move(name);
    collision_mesh.triangles = {triangle()};
    return collision_mesh;
}

stellar::assets::WorldMarker player_spawn(Vec3 position) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kPlayerSpawn;
    marker.name = "SPAWN_Player";
    marker.position = position;
    return marker;
}

stellar::assets::LevelAsset scene_with_meshes(
    std::initializer_list<stellar::assets::CollisionMesh> meshes) {
    stellar::assets::LevelAsset scene{};
    scene.world_metadata.markers = {player_spawn({0.0F, 0.0F, 0.0F})};
    scene.level_collision = stellar::assets::LevelCollisionAsset{};
    scene.level_collision->meshes.assign(meshes.begin(), meshes.end());
    return scene;
}

stellar::scripting::ScriptField string_field(std::string key, std::string value) {
    stellar::scripting::ScriptField field{};
    field.key = std::move(key);
    field.type = stellar::scripting::ScriptValueType::kString;
    field.string_value = std::move(value);
    return field;
}

stellar::scripting::ScriptField bool_field(std::string key, bool value) {
    stellar::scripting::ScriptField field{};
    field.key = std::move(key);
    field.type = stellar::scripting::ScriptValueType::kBoolean;
    field.bool_value = value;
    return field;
}

stellar::scripting::ScriptField number_field(std::string key, double value) {
    stellar::scripting::ScriptField field{};
    field.key = std::move(key);
    field.type = stellar::scripting::ScriptValueType::kNumber;
    field.number_value = value;
    return field;
}

stellar::scripting::ScriptOutputEvent set_mesh_event(std::string mesh_name, bool enabled) {
    return stellar::scripting::ScriptOutputEvent{
        .name = "collision.set_mesh_enabled",
        .fields = {string_field("mesh", std::move(mesh_name)), bool_field("enabled", enabled)}};
}

stellar::assets::WorldMarker object_collider_marker(std::string name) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kObjectCollider;
    marker.name = std::move(name);
    marker.position = {0.0F, 0.0F, 0.0F};
    marker.scale = {1.0F, 1.0F, 1.0F};
    return marker;
}

stellar::assets::LevelAsset scene_with_object_collider() {
    stellar::assets::LevelAsset scene{};
    scene.world_metadata.markers = {player_spawn({0.0F, 0.0F, 0.0F}),
                                    object_collider_marker("Pickup")};
    return scene;
}

stellar::scripting::ScriptOutputEvent set_object_collider_event(double id, bool enabled) {
    return stellar::scripting::ScriptOutputEvent{
        .name = "object_collider.set_enabled",
        .fields = {number_field("id", id), bool_field("enabled", enabled)}};
}

void collision_set_mesh_enabled_valid_event_applies() {
    const auto scene = scene_with_meshes({mesh("DoorBlocker")});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world);

    const std::array<stellar::scripting::ScriptOutputEvent, 1> events{
        set_mesh_event("DoorBlocker", false)};
    const auto application = stellar::scripting::apply_script_commands(session, events);

    assert(application.results.size() == 1);
    assert(application.results[0].event_name == "collision.set_mesh_enabled");
    assert(application.results[0].applied);
    assert(application.results[0].code == "ok");
}

void missing_mesh_field_reports_invalid_field() {
    const auto scene = scene_with_meshes({mesh("DoorBlocker")});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world);
    const stellar::scripting::ScriptOutputEvent event{.name = "collision.set_mesh_enabled",
                                                     .fields = {bool_field("enabled", false)}};

    const auto application = stellar::scripting::apply_script_commands(session, {&event, 1});

    assert(application.results.size() == 1);
    assert(!application.results[0].applied);
    assert(application.results[0].code == "invalid_field");
    assert(application.results[0].message.find("mesh") != std::string::npos);
}

void non_boolean_enabled_reports_invalid_field() {
    const auto scene = scene_with_meshes({mesh("DoorBlocker")});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world);
    const stellar::scripting::ScriptOutputEvent event{
        .name = "collision.set_mesh_enabled",
        .fields = {string_field("mesh", "DoorBlocker"), number_field("enabled", 0.0)}};

    const auto application = stellar::scripting::apply_script_commands(session, {&event, 1});

    assert(application.results.size() == 1);
    assert(!application.results[0].applied);
    assert(application.results[0].code == "invalid_field");
    assert(application.results[0].message.find("enabled") != std::string::npos);
}

void unknown_collision_mesh_reports_not_found() {
    const auto scene = scene_with_meshes({mesh("DoorBlocker")});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world);
    const auto event = set_mesh_event("Missing", false);

    const auto application = stellar::scripting::apply_script_commands(session, {&event, 1});

    assert(application.results.size() == 1);
    assert(!application.results[0].applied);
    assert(application.results[0].code == "not_found");
}

void unsupported_event_reports_by_policy() {
    const auto scene = scene_with_meshes({mesh("DoorBlocker")});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world);
    const stellar::scripting::ScriptOutputEvent event{.name = "door_open_requested"};

    const auto application = stellar::scripting::apply_script_commands(session, {&event, 1});

    assert(application.results.size() == 1);
    assert(application.results[0].event_name == "door_open_requested");
    assert(!application.results[0].applied);
    assert(application.results[0].code == "unsupported_event");
}

void multiple_events_apply_in_deterministic_order() {
    const auto scene = scene_with_meshes({mesh("DoorBlocker")});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world);
    const std::array<stellar::scripting::ScriptOutputEvent, 3> events{
        set_mesh_event("DoorBlocker", false),
        stellar::scripting::ScriptOutputEvent{.name = "unhandled"},
        set_mesh_event("DoorBlocker", true),
    };

    const auto application = stellar::scripting::apply_script_commands(session, events);

    assert(application.results.size() == 3);
    assert(application.results[0].code == "ok");
    assert(application.results[0].applied);
    assert(application.results[1].code == "unsupported_event");
    assert(!application.results[1].applied);
    assert(application.results[2].code == "ok");
    assert(application.results[2].applied);
}

void object_collider_set_enabled_valid_event_applies() {
    const auto scene = scene_with_object_collider();
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world);

    const auto event = set_object_collider_event(2.0, false);
    const auto application = stellar::scripting::apply_script_commands(session, {&event, 1});

    assert(application.results.size() == 1);
    assert(application.results[0].event_name == "object_collider.set_enabled");
    assert(application.results[0].applied);
    assert(application.results[0].code == "applied");
}

void object_collider_set_enabled_missing_id_reports_invalid_field() {
    const auto scene = scene_with_object_collider();
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world);
    const stellar::scripting::ScriptOutputEvent event{.name = "object_collider.set_enabled",
                                                      .fields = {bool_field("enabled", false)}};

    const auto application = stellar::scripting::apply_script_commands(session, {&event, 1});

    assert(application.results.size() == 1);
    assert(!application.results[0].applied);
    assert(application.results[0].code == "invalid_field");
    assert(application.results[0].message.find("id") != std::string::npos);
}

void object_collider_set_enabled_non_integer_id_reports_invalid_field() {
    const auto scene = scene_with_object_collider();
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world);
    const auto event = set_object_collider_event(2.5, false);

    const auto application = stellar::scripting::apply_script_commands(session, {&event, 1});

    assert(application.results.size() == 1);
    assert(!application.results[0].applied);
    assert(application.results[0].code == "invalid_field");
}

void object_collider_set_enabled_non_boolean_enabled_reports_invalid_field() {
    const auto scene = scene_with_object_collider();
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world);
    const stellar::scripting::ScriptOutputEvent event{
        .name = "object_collider.set_enabled",
        .fields = {number_field("id", 2.0), number_field("enabled", 0.0)}};

    const auto application = stellar::scripting::apply_script_commands(session, {&event, 1});

    assert(application.results.size() == 1);
    assert(!application.results[0].applied);
    assert(application.results[0].code == "invalid_field");
    assert(application.results[0].message.find("enabled") != std::string::npos);
}

void object_collider_set_enabled_unknown_id_reports_not_found() {
    const auto scene = scene_with_object_collider();
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world);
    const auto event = set_object_collider_event(99.0, false);

    const auto application = stellar::scripting::apply_script_commands(session, {&event, 1});

    assert(application.results.size() == 1);
    assert(!application.results[0].applied);
    assert(application.results[0].code == "not_found");
}

void object_collider_disable_while_overlapped_surfaces_sync_exit_in_command_result() {
    const auto scene = scene_with_object_collider();
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::WorldSession session(world);
    (void)session.tick({});
    const auto event = set_object_collider_event(2.0, false);

    const auto application = stellar::scripting::apply_script_commands(session, {&event, 1});

    assert(application.results.size() == 1);
    assert(application.results[0].applied);
    assert(application.results[0].object_collider_events.size() == 1);
    assert(application.results[0].object_collider_events[0].exited);
    assert(application.results[0].object_collider_events[0].collider_id == 2);
}

} // namespace

int main() {
    collision_set_mesh_enabled_valid_event_applies();
    missing_mesh_field_reports_invalid_field();
    non_boolean_enabled_reports_invalid_field();
    unknown_collision_mesh_reports_not_found();
    unsupported_event_reports_by_policy();
    multiple_events_apply_in_deterministic_order();
    object_collider_set_enabled_valid_event_applies();
    object_collider_set_enabled_missing_id_reports_invalid_field();
    object_collider_set_enabled_non_integer_id_reports_invalid_field();
    object_collider_set_enabled_non_boolean_enabled_reports_invalid_field();
    object_collider_set_enabled_unknown_id_reports_not_found();
    object_collider_disable_while_overlapped_surfaces_sync_exit_in_command_result();
    return EXIT_SUCCESS;
}
