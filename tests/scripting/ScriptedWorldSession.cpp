#include "stellar/scripting/ScriptedWorldSession.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace {

using Vec3 = std::array<float, 3>;

stellar::assets::WorldMarker player_spawn(Vec3 position) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kPlayerSpawn;
    marker.name = "SPAWN_Player";
    marker.position = position;
    return marker;
}

stellar::assets::WorldMarker trigger_marker(std::string name,
                                            Vec3 position,
                                            Vec3 scale,
                                            std::string script_id,
                                            std::string table_name) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kTrigger;
    marker.name = std::move(name);
    marker.position = position;
    marker.scale = scale;
    marker.script =
        stellar::assets::WorldScriptBinding{std::move(script_id), std::move(table_name)};
    return marker;
}

stellar::assets::WorldMarker object_collider_marker(std::string name,
                                                    Vec3 position,
                                                    Vec3 scale,
                                                    std::string archetype,
                                                    std::string script_id,
                                                    std::string table_name) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kObjectCollider;
    marker.name = std::move(name);
    marker.position = position;
    marker.scale = scale;
    marker.archetype = std::move(archetype);
    marker.script =
        stellar::assets::WorldScriptBinding{std::move(script_id), std::move(table_name)};
    return marker;
}

stellar::assets::CollisionTriangle triangle(Vec3 a, Vec3 b, Vec3 c, Vec3 normal) {
    return stellar::assets::CollisionTriangle{.a = a, .b = b, .c = c, .normal = normal};
}

stellar::assets::CollisionTriangle wall_x_triangle_a(float x = 0.8F) {
    return triangle({x, -2.0F, -4.0F}, {x, 4.0F, 4.0F}, {x, -2.0F, 4.0F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::CollisionTriangle wall_x_triangle_b(float x = 0.8F) {
    return triangle({x, -2.0F, -4.0F}, {x, 4.0F, -4.0F}, {x, 4.0F, 4.0F},
                    {-1.0F, 0.0F, 0.0F});
}

stellar::assets::LevelAsset scene_with_markers(
    std::initializer_list<stellar::assets::WorldMarker> markers) {
    stellar::assets::LevelAsset scene{};
    scene.world_metadata.markers.assign(markers.begin(), markers.end());
    return scene;
}

stellar::assets::LevelAsset scene_with_trigger_and_wall(std::string wall_name) {
    auto scene = scene_with_markers({
        player_spawn({0.0F, 0.5F, 0.0F}),
        trigger_marker("DoorOpen", {0.0F, 0.5F, 0.0F}, {0.5F, 0.5F, 0.5F}, "door", "Door"),
    });
    stellar::assets::CollisionMesh wall;
    wall.name = std::move(wall_name);
    wall.triangles = {wall_x_triangle_a(), wall_x_triangle_b()};
    scene.level_collision = stellar::assets::LevelCollisionAsset{.meshes = {wall}};
    return scene;
}

stellar::server::WorldSessionConfig test_session_config() {
    stellar::server::WorldSessionConfig config{};
    config.local_player_id = 7;
    config.movement.max_speed = 10.0F;
    config.movement.acceleration = 100.0F;
    config.movement.gravity = 0.0F;
    config.movement.terminal_fall_speed = 50.0F;
    config.movement.fixed_dt = 0.1F;
    config.movement.character.radius = 0.25F;
    config.movement.character.height = 1.0F;
    config.movement.character.skin_width = 0.0F;
    config.movement.character.ground_snap_distance = 0.0F;
    config.movement.character.max_slide_iterations = 4;
    return config;
}

stellar::scripting::ScriptRegistry registry_with(std::string script_id, std::string source) {
    stellar::scripting::ScriptRegistry registry{};
    registry.set_script(std::move(script_id), std::move(source));
    return registry;
}

stellar::scripting::ScriptRegistry registry_with(std::string first_id,
                                                 std::string first_source,
                                                 std::string second_id,
                                                 std::string second_source) {
    stellar::scripting::ScriptRegistry registry{};
    registry.set_script(std::move(first_id), std::move(first_source));
    registry.set_script(std::move(second_id), std::move(second_source));
    return registry;
}

const stellar::scripting::ScriptField* find_field(
    const stellar::scripting::ScriptOutputEvent& event,
    const std::string& key) {
    for (const stellar::scripting::ScriptField& field : event.fields) {
        if (field.key == key) {
            return &field;
        }
    }
    return nullptr;
}

double number_field(const stellar::scripting::ScriptOutputEvent& event, const std::string& key) {
    const stellar::scripting::ScriptField* field = find_field(event, key);
    assert(field != nullptr);
    assert(field->type == stellar::scripting::ScriptValueType::kNumber);
    return field->number_value;
}

std::string string_field(const stellar::scripting::ScriptOutputEvent& event,
                         const std::string& key) {
    const stellar::scripting::ScriptField* field = find_field(event, key);
    assert(field != nullptr);
    assert(field->type == stellar::scripting::ScriptValueType::kString);
    return field->string_value;
}

void assert_same_event(const stellar::scripting::ScriptOutputEvent& a,
                       const stellar::scripting::ScriptOutputEvent& b) {
    assert(a.name == b.name);
    assert(a.fields.size() == b.fields.size());
    for (std::size_t i = 0; i < a.fields.size(); ++i) {
        assert(a.fields[i].key == b.fields[i].key);
        assert(a.fields[i].type == b.fields[i].type);
        assert(a.fields[i].string_value == b.fields[i].string_value);
        assert(a.fields[i].number_value == b.fields[i].number_value);
        assert(a.fields[i].bool_value == b.fields[i].bool_value);
    }
}

void assert_same_command_result(const stellar::scripting::ScriptCommandResult& a,
                                const stellar::scripting::ScriptCommandResult& b) {
    assert(a.event_name == b.event_name);
    assert(a.applied == b.applied);
    assert(a.code == b.code);
    assert(a.message == b.message);
}

void assert_same_frame(const stellar::scripting::ScriptedWorldFrame& a,
                       const stellar::scripting::ScriptedWorldFrame& b) {
    assert(a.snapshot.tick == b.snapshot.tick);
    assert(a.snapshot.players.size() == b.snapshot.players.size());
    assert(a.snapshot.trigger_events.size() == b.snapshot.trigger_events.size());
    assert(a.snapshot.object_collider_events.size() == b.snapshot.object_collider_events.size());
    assert(a.script_errors.size() == b.script_errors.size());
    assert(a.script_events.size() == b.script_events.size());
    assert(a.command_results.size() == b.command_results.size());
    for (std::size_t i = 0; i < a.script_events.size(); ++i) {
        assert_same_event(a.script_events[i], b.script_events[i]);
    }
    for (std::size_t i = 0; i < a.command_results.size(); ++i) {
        assert_same_command_result(a.command_results[i], b.command_results[i]);
    }
}

void create_loads_each_required_script_once() {
    const auto scene = scene_with_markers({
        player_spawn({0.0F, 0.0F, 0.0F}),
        trigger_marker("A", {0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}, "shared", "A"),
        trigger_marker("B", {0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}, "shared", "B"),
    });
    const auto world = stellar::world::build_runtime_world(scene);
    auto registry = registry_with(
        "shared",
        "load_count = (load_count or 0) + 1\n"
        "A = {}; B = {}\n"
        "function A.on_trigger_enter(event) stellar.emit_event('A', {loads = load_count}) end\n"
        "function B.on_trigger_enter(event) stellar.emit_event('B', {loads = load_count}) end\n");

    auto session = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                    std::move(registry));
    assert(session.has_value());
    const auto frame = session->tick({});

    assert(frame.script_errors.empty());
    assert(frame.script_events.size() == 2);
    assert(number_field(frame.script_events[0], "loads") == 1.0);
    assert(number_field(frame.script_events[1], "loads") == 1.0);
}

void missing_script_id_returns_deterministic_error() {
    const auto scene = scene_with_markers({
        player_spawn({0.0F, 0.0F, 0.0F}),
        trigger_marker("Missing", {0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}, "missing", "Missing"),
    });
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::scripting::ScriptRegistry registry{};

    auto session = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                    std::move(registry));

    assert(!session.has_value());
    assert(session.error().script_id == "missing");
    assert(session.error().operation == "load_script");
    assert(session.error().message == "Missing script source for id: missing");
}

void tick_emits_native_snapshot_and_script_events() {
    const auto scene = scene_with_markers({
        player_spawn({0.0F, 0.0F, 0.0F}),
        trigger_marker("Gate", {0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}, "gate", "Gate"),
    });
    const auto world = stellar::world::build_runtime_world(scene);
    auto registry = registry_with(
        "gate",
        "Gate = {}\n"
        "function Gate.on_trigger_enter(event)\n"
        "  stellar.emit_event('gate_entered', {trigger = event.trigger_name, tick = event.tick})\n"
        "end\n");
    auto session = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                    std::move(registry));
    assert(session.has_value());

    const auto frame = session->tick({});

    assert(frame.snapshot.tick == 1);
    assert(frame.snapshot.players.size() == 1);
    assert(frame.snapshot.trigger_events.size() == 1);
    assert(frame.snapshot.trigger_events[0].trigger_name == "Gate");
    assert(frame.snapshot.trigger_events[0].entered);
    assert(frame.script_errors.empty());
    assert(frame.script_events.size() == 1);
    assert(frame.script_events[0].name == "gate_entered");
    assert(string_field(frame.script_events[0], "trigger") == "Gate");
    assert(number_field(frame.script_events[0], "tick") == 1.0);
}

void repeated_scripted_path_is_deterministic() {
    const auto scene = scene_with_markers({
        player_spawn({0.0F, 0.0F, 0.0F}),
        trigger_marker("Gate", {0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}, "gate", "Gate"),
    });
    const auto world = stellar::world::build_runtime_world(scene);
    const char* source =
        "Gate = {}\n"
        "function Gate.on_trigger_enter(event)\n"
        "  stellar.emit_event('enter', {tick = event.tick})\n"
        "end\n"
        "function Gate.on_trigger_stay(event)\n"
        "  stellar.emit_event('stay', {tick = event.tick})\n"
        "end\n";
    auto first_registry = registry_with("gate", source);
    auto second_registry = registry_with("gate", source);
    auto first = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                  std::move(first_registry));
    auto second = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                   std::move(second_registry));
    assert(first.has_value());
    assert(second.has_value());

    assert_same_frame(first->tick({}), second->tick({}));
    assert_same_frame(first->tick({}), second->tick({}));
}

void script_errors_are_reported_without_crashing_session() {
    const auto scene = scene_with_markers({
        player_spawn({0.0F, 0.0F, 0.0F}),
        trigger_marker("Bad", {0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}, "bad", "Bad"),
    });
    const auto world = stellar::world::build_runtime_world(scene);
    auto registry = registry_with(
        "bad",
        "Bad = {}\n"
        "function Bad.on_trigger_enter(event) error('expected scripted failure') end\n"
        "function Bad.on_trigger_stay(event) stellar.emit_event('still_alive', {}) end\n");
    auto session = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                    std::move(registry));
    assert(session.has_value());

    const auto first = session->tick({});
    const auto second = session->tick({});

    assert(first.script_errors.size() == 1);
    assert(first.script_errors[0].script_id == "bad");
    assert(first.script_errors[0].operation == "on_trigger_enter");
    assert(first.script_errors[0].message.find("expected scripted failure") != std::string::npos);
    assert(second.script_errors.empty());
    assert(second.script_events.size() == 1);
    assert(second.script_events[0].name == "still_alive");
}

void latest_snapshot_does_not_replay_script_events() {
    const auto scene = scene_with_markers({
        player_spawn({0.0F, 0.0F, 0.0F}),
        trigger_marker("Gate", {0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}, "gate", "Gate"),
    });
    const auto world = stellar::world::build_runtime_world(scene);
    auto registry = registry_with(
        "gate",
        "count = 0\n"
        "Gate = {}\n"
        "function Gate.on_trigger_enter(event)\n"
        "  count = count + 1\n"
        "  stellar.emit_event('counted', {count = count})\n"
        "end\n");
    auto session = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                    std::move(registry));
    assert(session.has_value());

    const auto frame = session->tick({});
    const auto& latest_a = session->latest_snapshot();
    const auto& latest_b = session->latest_snapshot();

    assert(frame.script_events.size() == 1);
    assert(number_field(frame.script_events[0], "count") == 1.0);
    assert(latest_a.tick == frame.snapshot.tick);
    assert(latest_b.tick == frame.snapshot.tick);
    assert(latest_a.players.size() == frame.snapshot.players.size());
    assert(latest_b.trigger_events.size() == frame.snapshot.trigger_events.size());
}

void scripted_trigger_can_disable_named_collision_mesh() {
    const auto scene = scene_with_trigger_and_wall("DoorBlocker");
    const auto world = stellar::world::build_runtime_world(scene);
    auto registry = registry_with(
        "door",
        "Door = {}\n"
        "function Door.on_trigger_enter(event)\n"
        "  stellar.emit_event('collision.set_mesh_enabled', "
        "{mesh = 'DoorBlocker', enabled = false})\n"
        "end\n");
    auto session = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                    std::move(registry));
    assert(session.has_value());

    const auto frame = session->tick({});

    assert(frame.script_errors.empty());
    assert(frame.script_events.size() == 1);
    assert(frame.script_events[0].name == "collision.set_mesh_enabled");
    assert(frame.command_results.size() == 1);
    assert(frame.command_results[0].applied);
    assert(frame.command_results[0].code == "ok");
    bool found_open_gate = false;
    for (const stellar::server::GameplayEntity& entity : frame.snapshot.gameplay_world.entities) {
        if (entity.kind == stellar::server::EntityKind::kDoor && entity.metadata.name == "DoorBlocker") {
            found_open_gate = entity.open && !entity.active;
        }
    }
    assert(found_open_gate);
}

void collision_disable_affects_next_tick_not_current_tick() {
    const auto scene = scene_with_trigger_and_wall("DoorBlocker");
    const auto world = stellar::world::build_runtime_world(scene);
    auto registry = registry_with(
        "door",
        "Door = {}\n"
        "function Door.on_trigger_enter(event)\n"
        "  stellar.emit_event('collision.set_mesh_enabled', "
        "{mesh = 'DoorBlocker', enabled = false})\n"
        "end\n");
    auto session = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                    std::move(registry));
    assert(session.has_value());
    const std::vector<stellar::server::PlayerCommand> move_right{{
        .player_id = 7,
        .movement = {.wish_direction = {1.0F, 0.0F, 0.0F}},
    }};

    const auto first = session->tick(move_right);
    const auto second = session->tick(move_right);

    assert(first.command_results.size() == 1);
    assert(first.command_results[0].applied);
    assert(first.snapshot.players[0].position[0] < 0.56F);
    assert(second.snapshot.players[0].position[0] > 0.9F);
}

void scripted_invalid_collision_command_does_not_crash() {
    const auto scene = scene_with_trigger_and_wall("DoorBlocker");
    const auto world = stellar::world::build_runtime_world(scene);
    auto registry = registry_with(
        "door",
        "Door = {}\n"
        "function Door.on_trigger_enter(event)\n"
        "  stellar.emit_event('collision.set_mesh_enabled', {mesh = 'DoorBlocker'})\n"
        "end\n");
    auto session = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                    std::move(registry));
    assert(session.has_value());

    const auto frame = session->tick({});

    assert(frame.script_errors.empty());
    assert(frame.script_events.size() == 1);
    assert(frame.command_results.size() == 1);
    assert(!frame.command_results[0].applied);
    assert(frame.command_results[0].code == "invalid_field");
}

void latest_snapshot_does_not_reapply_commands() {
    const auto scene = scene_with_trigger_and_wall("DoorBlocker");
    const auto world = stellar::world::build_runtime_world(scene);
    auto registry = registry_with(
        "door",
        "count = 0\n"
        "Door = {}\n"
        "function Door.on_trigger_enter(event)\n"
        "  count = count + 1\n"
        "  stellar.emit_event('collision.set_mesh_enabled', "
        "{mesh = 'DoorBlocker', enabled = false, count = count})\n"
        "end\n");
    auto session = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                    std::move(registry));
    assert(session.has_value());
    const auto frame = session->tick({});
    const auto& latest_a = session->latest_snapshot();
    const auto& latest_b = session->latest_snapshot();

    assert(frame.command_results.size() == 1);
    assert(number_field(frame.script_events[0], "count") == 1.0);
    assert(latest_a.tick == frame.snapshot.tick);
    assert(latest_b.tick == frame.snapshot.tick);
}

void repeat_run_produces_same_script_events_and_command_results() {
    const auto scene = scene_with_trigger_and_wall("DoorBlocker");
    const auto world = stellar::world::build_runtime_world(scene);
    const char* source =
        "Door = {}\n"
        "function Door.on_trigger_enter(event)\n"
        "  stellar.emit_event('collision.set_mesh_enabled', "
        "{mesh = 'DoorBlocker', enabled = false, tick = event.tick})\n"
        "end\n";
    auto first_registry = registry_with("door", source);
    auto second_registry = registry_with("door", source);
    auto first = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                  std::move(first_registry));
    auto second = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                   std::move(second_registry));
    assert(first.has_value());
    assert(second.has_value());

    assert_same_frame(first->tick({}), second->tick({}));
    assert_same_frame(first->tick({}), second->tick({}));
}

void scripted_object_collider_enter_can_disable_own_collider() {
    const auto scene = scene_with_markers({
        player_spawn({0.0F, 0.0F, 0.0F}),
        object_collider_marker("Gem", {0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}, "sensor",
                               "gem", "Gem"),
    });
    const auto world = stellar::world::build_runtime_world(scene);
    auto registry = registry_with(
        "gem",
        "Gem = {}\n"
        "function Gem.on_object_collider_enter(event)\n"
        "  stellar.emit_event('object_collider.set_enabled', "
        "{id = event.collider_id, enabled = false, name = event.collider_name})\n"
        "end\n");
    auto session = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                    std::move(registry));
    assert(session.has_value());

    const auto frame = session->tick({});

    assert(frame.snapshot.object_collider_events.size() == 1);
    assert(frame.snapshot.object_collider_events[0].entered);
    assert(frame.script_errors.empty());
    assert(frame.script_events.size() == 1);
    assert(frame.script_events[0].name == "object_collider.set_enabled");
    assert(frame.command_results.size() == 1);
    assert(frame.command_results[0].applied);
    assert(frame.command_results[0].code == "applied");
    assert(frame.command_results[0].object_collider_events.size() == 1);
    assert(frame.command_results[0].object_collider_events[0].exited);

    const auto next = session->tick({});
    assert(next.snapshot.object_collider_events.empty());
    assert(next.script_events.empty());
}

void pickup_enter_collects_once_and_deactivates_gameplay_entity() {
    stellar::assets::WorldMarker pickup{};
    pickup.type = stellar::assets::WorldMarkerType::kObjectCollider;
    pickup.name = "Gem";
    pickup.position = {0.0F, 0.0F, 0.0F};
    pickup.scale = {1.0F, 1.0F, 1.0F};
    pickup.archetype = "pickup";
    const auto scene = scene_with_markers({player_spawn({0.0F, 0.0F, 0.0F}), pickup});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::scripting::ScriptRegistry registry{};
    auto session = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                    std::move(registry));
    assert(session.has_value());

    const auto first = session->tick({});
    const auto second = session->tick({});

    assert(first.snapshot.object_collider_events.size() == 1);
    assert(first.snapshot.object_collider_events[0].entered);
    assert(first.script_events.size() == 1);
    assert(first.script_events[0].name == "gameplay.collect_pickup");
    assert(first.command_results.size() == 1);
    assert(first.command_results[0].applied);
    assert(first.command_results[0].code == "collected");
    assert(first.command_results[0].object_collider_events.size() == 1);
    assert(first.command_results[0].object_collider_events[0].exited);

    const auto& pickup_entity = first.snapshot.gameplay_world.entities[1];
    assert(pickup_entity.kind == stellar::server::EntityKind::kPickup);
    assert(!pickup_entity.active);
    assert(second.snapshot.object_collider_events.empty());
    assert(second.script_events.empty());
    assert(second.command_results.empty());
}

void latest_snapshot_does_not_replay_object_collider_scripts() {
    const auto scene = scene_with_markers({
        player_spawn({0.0F, 0.0F, 0.0F}),
        object_collider_marker("Gem", {0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}, "sensor",
                               "gem", "Gem"),
    });
    const auto world = stellar::world::build_runtime_world(scene);
    auto registry = registry_with(
        "gem",
        "count = 0\n"
        "Gem = {}\n"
        "function Gem.on_object_collider_enter(event)\n"
        "  count = count + 1\n"
        "  stellar.emit_event('object_counted', {count = count})\n"
        "end\n");
    auto session = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                    std::move(registry));
    assert(session.has_value());

    const auto frame = session->tick({});
    const auto& latest_a = session->latest_snapshot();
    const auto& latest_b = session->latest_snapshot();

    assert(frame.script_events.size() == 1);
    assert(number_field(frame.script_events[0], "count") == 1.0);
    assert(latest_a.tick == frame.snapshot.tick);
    assert(latest_b.object_collider_events.size() == frame.snapshot.object_collider_events.size());
}

void repeat_run_produces_same_object_script_events_and_command_results() {
    const auto scene = scene_with_markers({
        player_spawn({0.0F, 0.0F, 0.0F}),
        object_collider_marker("Gem", {0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}, "sensor",
                               "gem", "Gem"),
    });
    const auto world = stellar::world::build_runtime_world(scene);
    const char* source =
        "Gem = {}\n"
        "function Gem.on_object_collider_enter(event)\n"
        "  stellar.emit_event('object_collider.set_enabled', "
        "{id = event.collider_id, enabled = false, tick = event.tick})\n"
        "end\n";
    auto first = stellar::scripting::ScriptedWorldSession::create(
        world, test_session_config(), registry_with("gem", source));
    auto second = stellar::scripting::ScriptedWorldSession::create(
        world, test_session_config(), registry_with("gem", source));
    assert(first.has_value());
    assert(second.has_value());

    assert_same_frame(first->tick({}), second->tick({}));
    assert_same_frame(first->tick({}), second->tick({}));
}

void trigger_scripts_and_object_collider_scripts_have_documented_order() {
    const auto scene = scene_with_markers({
        player_spawn({0.0F, 0.0F, 0.0F}),
        trigger_marker("Gate", {0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F}, "gate", "Gate"),
        object_collider_marker("Gem", {0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}, "sensor",
                               "gem", "Gem"),
    });
    const auto world = stellar::world::build_runtime_world(scene);
    auto registry = registry_with(
        "gate",
        "Gate = {}\n"
        "function Gate.on_trigger_enter(event) stellar.emit_event('trigger_first', {}) end\n",
        "gem",
        "Gem = {}\n"
        "function Gem.on_object_collider_enter(event) stellar.emit_event('object_second', {}) end\n");
    auto session = stellar::scripting::ScriptedWorldSession::create(world, test_session_config(),
                                                                    std::move(registry));
    assert(session.has_value());

    const auto frame = session->tick({});

    assert(frame.snapshot.trigger_events.size() == 1);
    assert(frame.snapshot.object_collider_events.size() == 1);
    assert(frame.script_events.size() == 2);
    assert(frame.script_events[0].name == "trigger_first");
    assert(frame.script_events[1].name == "object_second");
}

} // namespace

int main() {
    create_loads_each_required_script_once();
    missing_script_id_returns_deterministic_error();
    tick_emits_native_snapshot_and_script_events();
    repeated_scripted_path_is_deterministic();
    script_errors_are_reported_without_crashing_session();
    latest_snapshot_does_not_replay_script_events();
    scripted_trigger_can_disable_named_collision_mesh();
    collision_disable_affects_next_tick_not_current_tick();
    scripted_invalid_collision_command_does_not_crash();
    latest_snapshot_does_not_reapply_commands();
    repeat_run_produces_same_script_events_and_command_results();
    scripted_object_collider_enter_can_disable_own_collider();
    pickup_enter_collects_once_and_deactivates_gameplay_entity();
    latest_snapshot_does_not_replay_object_collider_scripts();
    repeat_run_produces_same_object_script_events_and_command_results();
    trigger_scripts_and_object_collider_scripts_have_documented_order();
    return EXIT_SUCCESS;
}
