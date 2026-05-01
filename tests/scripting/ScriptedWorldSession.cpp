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

stellar::assets::SceneAsset scene_with_markers(
    std::initializer_list<stellar::assets::WorldMarker> markers) {
    stellar::assets::SceneAsset scene{};
    scene.world_metadata.markers.assign(markers.begin(), markers.end());
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

void assert_same_frame(const stellar::scripting::ScriptedWorldFrame& a,
                       const stellar::scripting::ScriptedWorldFrame& b) {
    assert(a.snapshot.tick == b.snapshot.tick);
    assert(a.snapshot.players.size() == b.snapshot.players.size());
    assert(a.snapshot.trigger_events.size() == b.snapshot.trigger_events.size());
    assert(a.script_errors.size() == b.script_errors.size());
    assert(a.script_events.size() == b.script_events.size());
    for (std::size_t i = 0; i < a.script_events.size(); ++i) {
        assert_same_event(a.script_events[i], b.script_events[i]);
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

} // namespace

int main() {
    create_loads_each_required_script_once();
    missing_script_id_returns_deterministic_error();
    tick_emits_native_snapshot_and_script_events();
    repeated_scripted_path_is_deterministic();
    script_errors_are_reported_without_crashing_session();
    latest_snapshot_does_not_replay_script_events();
    return EXIT_SUCCESS;
}
