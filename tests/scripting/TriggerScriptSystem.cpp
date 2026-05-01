#include "stellar/scripting/TriggerScriptSystem.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using stellar::assets::WorldMarker;
using stellar::assets::WorldMarkerType;
using stellar::assets::WorldScriptBinding;
using stellar::scripting::LuaRuntime;
using stellar::scripting::ScriptOutputEvent;
using stellar::scripting::ScriptValueType;
using stellar::scripting::TriggerScriptSystem;
using stellar::server::MovementTriggerEvent;
using stellar::server::PlayerSnapshot;
using stellar::server::WorldSnapshot;
using stellar::world::RuntimeWorld;

namespace {

void require_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

WorldMarker trigger_marker(std::string name, std::string script_id, std::string table_name) {
    WorldMarker marker{};
    marker.type = WorldMarkerType::kTrigger;
    marker.name = std::move(name);
    marker.script = WorldScriptBinding{std::move(script_id), std::move(table_name)};
    return marker;
}

WorldSnapshot snapshot_with_event(MovementTriggerEvent event) {
    WorldSnapshot snapshot{};
    snapshot.tick = 7;
    PlayerSnapshot player{};
    player.player_id = 1;
    player.position = {2.0F, 3.0F, 4.0F};
    snapshot.players.push_back(player);
    snapshot.trigger_events.push_back(std::move(event));
    return snapshot;
}

double number_field(const ScriptOutputEvent& event, const std::string& key) {
    for (const auto& field : event.fields) {
        if (field.key == key) {
            require_true(field.type == ScriptValueType::kNumber, "field should be numeric");
            return field.number_value;
        }
    }
    require_true(false, "numeric field should exist");
    return 0.0;
}

std::string string_field(const ScriptOutputEvent& event, const std::string& key) {
    for (const auto& field : event.fields) {
        if (field.key == key) {
            require_true(field.type == ScriptValueType::kString, "field should be a string");
            return field.string_value;
        }
    }
    require_true(false, "string field should exist");
    return {};
}

void enter_stay_exit_callbacks_receive_event_fields() {
    RuntimeWorld world{};
    world.world_metadata.markers.push_back(trigger_marker("Door", "door", "Door"));
    TriggerScriptSystem system{world};

    LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "door",
        "Door = {}\n"
        "function Door.on_trigger_enter(event)\n"
        "  stellar.emit_event('enter', {trigger = event.trigger_name, source = event.source, "
        "phase = event.phase, player = event.player_id, x = event.player_position_x, "
        "y = event.player_position_y, z = event.player_position_z, tick = event.tick})\n"
        "end\n"
        "function Door.on_trigger_stay(event) stellar.emit_event('stay', {phase = event.phase}) end\n"
        "function Door.on_trigger_exit(event) stellar.emit_event('exit', {phase = event.phase}) end\n");
    require_true(loaded.has_value(), "door script should load");

    auto enter = system.process_trigger_events(
        runtime, snapshot_with_event(MovementTriggerEvent{.trigger_name = "Door", .entered = true}));
    require_true(enter.errors.empty(), "enter should not error");
    require_true(enter.output_events.size() == 1, "enter should emit one event");
    require_true(enter.output_events[0].name == "enter", "enter callback should run");
    require_true(string_field(enter.output_events[0], "trigger") == "Door", "trigger_name should pass");
    require_true(string_field(enter.output_events[0], "source") == "Door", "source should pass");
    require_true(string_field(enter.output_events[0], "phase") == "enter", "enter phase should pass");
    require_true(number_field(enter.output_events[0], "player") == 1.0, "player id should pass");
    require_true(number_field(enter.output_events[0], "x") == 2.0, "player x should pass");
    require_true(number_field(enter.output_events[0], "y") == 3.0, "player y should pass");
    require_true(number_field(enter.output_events[0], "z") == 4.0, "player z should pass");

    auto stay = system.process_trigger_events(
        runtime, snapshot_with_event(MovementTriggerEvent{.trigger_name = "Door", .stayed = true}));
    require_true(stay.errors.empty(), "stay should not error");
    require_true(stay.output_events.size() == 1, "stay should emit one event");
    require_true(stay.output_events[0].name == "stay", "stay callback should run");
    require_true(string_field(stay.output_events[0], "phase") == "stay", "stay phase should pass");

    auto exit = system.process_trigger_events(
        runtime, snapshot_with_event(MovementTriggerEvent{.trigger_name = "Door", .exited = true}));
    require_true(exit.errors.empty(), "exit should not error");
    require_true(exit.output_events.size() == 1, "exit should emit one event");
    require_true(exit.output_events[0].name == "exit", "exit callback should run");
    require_true(string_field(exit.output_events[0], "phase") == "exit", "exit phase should pass");
}

void phase_order_is_stable_when_multiple_flags_exist() {
    RuntimeWorld world{};
    world.world_metadata.markers.push_back(trigger_marker("Odd", "odd", "Odd"));
    TriggerScriptSystem system{world};
    LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "odd",
        "Odd = {}\n"
        "function Odd.on_trigger_enter(event) stellar.emit_event('enter', {}) end\n"
        "function Odd.on_trigger_stay(event) stellar.emit_event('stay', {}) end\n"
        "function Odd.on_trigger_exit(event) stellar.emit_event('exit', {}) end\n");
    require_true(loaded.has_value(), "odd script should load");

    auto result = system.process_trigger_events(
        runtime, snapshot_with_event(MovementTriggerEvent{.trigger_name = "Odd",
                                                          .entered = true,
                                                          .stayed = true,
                                                          .exited = true}));
    require_true(result.errors.empty(), "multi-phase event should not error");
    require_true(result.output_events.size() == 3, "multi-phase event should emit three events");
    require_true(result.output_events[0].name == "enter", "enter should run first");
    require_true(result.output_events[1].name == "stay", "stay should run second");
    require_true(result.output_events[2].name == "exit", "exit should run third");
}

void duplicate_bindings_preserve_metadata_order_and_events_preserve_snapshot_order() {
    RuntimeWorld world{};
    world.world_metadata.markers.push_back(trigger_marker("Shared", "a", "A"));
    world.world_metadata.markers.push_back(trigger_marker("Other", "other", "Other"));
    world.world_metadata.markers.push_back(trigger_marker("Shared", "b", "B"));
    TriggerScriptSystem system{world};
    LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "all",
        "A = {}; B = {}; Other = {}\n"
        "function A.on_trigger_enter(event) stellar.emit_event('A', {}) end\n"
        "function B.on_trigger_enter(event) stellar.emit_event('B', {}) end\n"
        "function Other.on_trigger_enter(event) stellar.emit_event('Other', {}) end\n");
    require_true(loaded.has_value(), "ordering script should load");

    WorldSnapshot snapshot{};
    snapshot.trigger_events.push_back(MovementTriggerEvent{.trigger_name = "Shared", .entered = true});
    snapshot.trigger_events.push_back(MovementTriggerEvent{.trigger_name = "Other", .entered = true});
    auto result = system.process_trigger_events(runtime, snapshot);
    require_true(result.errors.empty(), "ordered callbacks should not error");
    require_true(result.output_events.size() == 3, "three ordered callbacks should emit");
    require_true(result.output_events[0].name == "A", "first duplicate binding should run first");
    require_true(result.output_events[1].name == "B", "second duplicate binding should run second");
    require_true(result.output_events[2].name == "Other", "second snapshot event should run later");
}

void missing_callback_is_noop_and_unbound_markers_are_ignored() {
    RuntimeWorld world{};
    world.world_metadata.markers.push_back(trigger_marker("Quiet", "quiet", "Quiet"));
    WorldMarker unbound{};
    unbound.type = WorldMarkerType::kTrigger;
    unbound.name = "Unbound";
    world.world_metadata.markers.push_back(unbound);
    TriggerScriptSystem system{world};
    LuaRuntime runtime{};
    auto loaded = runtime.load_script("quiet", "Quiet = {}");
    require_true(loaded.has_value(), "quiet script should load");

    WorldSnapshot snapshot{};
    snapshot.trigger_events.push_back(MovementTriggerEvent{.trigger_name = "Quiet", .entered = true});
    snapshot.trigger_events.push_back(MovementTriggerEvent{.trigger_name = "Unbound", .entered = true});
    auto result = system.process_trigger_events(runtime, snapshot);
    require_true(result.errors.empty(), "missing callback and unbound marker should not error");
    require_true(result.output_events.empty(), "missing callback and unbound marker should emit nothing");
}

void missing_table_reports_deterministic_error() {
    RuntimeWorld world{};
    world.world_metadata.markers.push_back(trigger_marker("Missing", "missing", "MissingTable"));
    TriggerScriptSystem system{world};
    LuaRuntime runtime{};
    auto result = system.process_trigger_events(
        runtime, snapshot_with_event(MovementTriggerEvent{.trigger_name = "Missing", .entered = true}));
    require_true(result.output_events.empty(), "missing table should emit nothing");
    require_true(result.errors.size() == 1, "missing table should report one error");
    require_true(result.errors[0].script_id == "missing", "missing table error should retain script id");
    require_true(result.errors[0].operation == "on_trigger_enter",
                 "missing table error should retain callback");
    require_true(result.errors[0].message.find("MissingTable") != std::string::npos,
                 "missing table error should name the table");
}

void runtime_error_continues_to_later_callbacks() {
    RuntimeWorld world{};
    world.world_metadata.markers.push_back(trigger_marker("Bad", "bad", "Bad"));
    world.world_metadata.markers.push_back(trigger_marker("Good", "good", "Good"));
    TriggerScriptSystem system{world};
    LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "mixed",
        "Bad = {}; Good = {}\n"
        "function Bad.on_trigger_enter(event) error('expected failure') end\n"
        "function Good.on_trigger_enter(event) stellar.emit_event('good_after_bad', {}) end\n");
    require_true(loaded.has_value(), "mixed script should load");

    WorldSnapshot snapshot{};
    snapshot.trigger_events.push_back(MovementTriggerEvent{.trigger_name = "Bad", .entered = true});
    snapshot.trigger_events.push_back(MovementTriggerEvent{.trigger_name = "Good", .entered = true});
    auto result = system.process_trigger_events(runtime, snapshot);
    require_true(result.errors.size() == 1, "runtime error should be reported once");
    require_true(result.errors[0].message.find("expected failure") != std::string::npos,
                 "runtime error should include Lua diagnostic");
    require_true(result.output_events.size() == 1, "later callback should still emit");
    require_true(result.output_events[0].name == "good_after_bad", "later callback should continue");
}

} // namespace

int main() {
    enter_stay_exit_callbacks_receive_event_fields();
    phase_order_is_stable_when_multiple_flags_exist();
    duplicate_bindings_preserve_metadata_order_and_events_preserve_snapshot_order();
    missing_callback_is_noop_and_unbound_markers_are_ignored();
    missing_table_reports_deterministic_error();
    runtime_error_continues_to_later_callbacks();
    return EXIT_SUCCESS;
}
