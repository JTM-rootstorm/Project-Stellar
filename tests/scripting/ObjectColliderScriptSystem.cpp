#include "stellar/scripting/ObjectColliderScriptSystem.hpp"

#include <cassert>
#include <cstdlib>
#include <string>

namespace {

stellar::assets::WorldMarker collider_marker(std::string name,
                                             std::string archetype,
                                             std::string script_id,
                                             std::string table_name) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kObjectCollider;
    marker.name = std::move(name);
    marker.archetype = std::move(archetype);
    marker.script = stellar::assets::WorldScriptBinding{std::move(script_id), std::move(table_name)};
    return marker;
}

stellar::server::WorldSnapshot snapshot_with_event(stellar::server::ObjectColliderEvent event) {
    stellar::server::WorldSnapshot snapshot{};
    snapshot.tick = 11;
    snapshot.object_collider_events.push_back(std::move(event));
    return snapshot;
}

const stellar::scripting::ScriptField* find_field(const stellar::scripting::ScriptOutputEvent& event,
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

std::string string_field(const stellar::scripting::ScriptOutputEvent& event, const std::string& key) {
    const stellar::scripting::ScriptField* field = find_field(event, key);
    assert(field != nullptr);
    assert(field->type == stellar::scripting::ScriptValueType::kString);
    return field->string_value;
}

void object_collider_enter_invokes_bound_script() {
    stellar::world::RuntimeWorld world{};
    world.world_metadata.markers.push_back(collider_marker("Gem", "sensor", "gem", "Gem"));
    stellar::scripting::ObjectColliderScriptSystem system{world};
    stellar::scripting::LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "gem",
        "Gem = {}\n"
        "function Gem.on_object_collider_enter(event)\n"
        "  stellar.emit_event('gem_enter', {tick = event.tick, player = event.player_id, "
        "id = event.collider_id, name = event.collider_name, archetype = event.archetype})\n"
        "end\n");
    assert(loaded.has_value());

    auto result = system.process_object_collider_events(
        runtime,
        snapshot_with_event({.player_id = 7,
                             .collider_id = 1,
                             .name = "Gem",
                             .archetype = "sensor",
                             .entered = true}));

    assert(result.errors.empty());
    assert(result.output_events.size() == 1);
    assert(result.output_events[0].name == "gem_enter");
    assert(number_field(result.output_events[0], "tick") == 11.0);
    assert(number_field(result.output_events[0], "player") == 7.0);
    assert(number_field(result.output_events[0], "id") == 1.0);
    assert(string_field(result.output_events[0], "name") == "Gem");
    assert(string_field(result.output_events[0], "archetype") == "sensor");
}

void pickup_enter_emits_native_collect_command_without_lua_binding() {
    stellar::world::RuntimeWorld world{};
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kObjectCollider;
    marker.name = "Gem";
    marker.archetype = "pickup";
    world.world_metadata.markers.push_back(std::move(marker));
    stellar::scripting::ObjectColliderScriptSystem system{world};
    stellar::scripting::LuaRuntime runtime{};

    auto result = system.process_object_collider_events(
        runtime,
        snapshot_with_event({.player_id = 7,
                             .collider_id = 1,
                             .name = "Gem",
                             .archetype = "pickup",
                             .entered = true}));

    assert(result.errors.empty());
    assert(result.output_events.size() == 1);
    assert(result.output_events[0].name == "gameplay.collect_pickup");
    assert(number_field(result.output_events[0], "id") == 1.0);
    assert(number_field(result.output_events[0], "player_id") == 7.0);
    assert(string_field(result.output_events[0], "name") == "Gem");
}

void object_collider_stay_and_exit_invoke_hooks() {
    stellar::world::RuntimeWorld world{};
    world.world_metadata.markers.push_back(collider_marker("Pad", "sensor", "pad", "Pad"));
    stellar::scripting::ObjectColliderScriptSystem system{world};
    stellar::scripting::LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "pad",
        "Pad = {}\n"
        "function Pad.on_object_collider_stay(event) stellar.emit_event('stay', {}) end\n"
        "function Pad.on_object_collider_exit(event) stellar.emit_event('exit', {}) end\n");
    assert(loaded.has_value());

    auto stay = system.process_object_collider_events(
        runtime, snapshot_with_event({.player_id = 1, .collider_id = 1, .name = "Pad", .stayed = true}));
    auto exit = system.process_object_collider_events(
        runtime, snapshot_with_event({.player_id = 1, .collider_id = 1, .name = "Pad", .exited = true}));

    assert(stay.errors.empty());
    assert(exit.errors.empty());
    assert(stay.output_events.size() == 1);
    assert(exit.output_events.size() == 1);
    assert(stay.output_events[0].name == "stay");
    assert(exit.output_events[0].name == "exit");
}

void unbound_object_collider_event_is_ignored() {
    stellar::world::RuntimeWorld world{};
    world.world_metadata.markers.push_back(collider_marker("Bound", "", "bound", "Bound"));
    stellar::scripting::ObjectColliderScriptSystem system{world};
    stellar::scripting::LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "bound",
        "Bound = {}\n"
        "function Bound.on_object_collider_enter(event) stellar.emit_event('bound', {}) end\n");
    assert(loaded.has_value());

    auto result = system.process_object_collider_events(
        runtime,
        snapshot_with_event({.player_id = 1, .collider_id = 2, .name = "Unbound", .entered = true}));

    assert(result.errors.empty());
    assert(result.output_events.empty());
}

void script_errors_are_reported_without_crash() {
    stellar::world::RuntimeWorld world{};
    world.world_metadata.markers.push_back(collider_marker("Bad", "", "bad", "Bad"));
    world.world_metadata.markers.push_back(collider_marker("Good", "", "good", "Good"));
    stellar::scripting::ObjectColliderScriptSystem system{world};
    stellar::scripting::LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "both",
        "Bad = {}; Good = {}\n"
        "function Bad.on_object_collider_enter(event) error('expected object failure') end\n"
        "function Good.on_object_collider_enter(event) stellar.emit_event('good', {}) end\n");
    assert(loaded.has_value());

    stellar::server::WorldSnapshot snapshot{};
    snapshot.object_collider_events.push_back(
        {.player_id = 1, .collider_id = 1, .name = "Bad", .entered = true});
    snapshot.object_collider_events.push_back(
        {.player_id = 1, .collider_id = 2, .name = "Good", .entered = true});
    auto result = system.process_object_collider_events(runtime, snapshot);

    assert(result.errors.size() == 1);
    assert(result.errors[0].operation == "on_object_collider_enter");
    assert(result.errors[0].message.find("expected object failure") != std::string::npos);
    assert(result.output_events.size() == 1);
    assert(result.output_events[0].name == "good");
}

void event_order_is_deterministic() {
    stellar::world::RuntimeWorld world{};
    world.world_metadata.markers.push_back(collider_marker("A", "", "all", "A"));
    world.world_metadata.markers.push_back(collider_marker("B", "", "all", "B"));
    stellar::scripting::ObjectColliderScriptSystem system{world};
    stellar::scripting::LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "all",
        "A = {}; B = {}\n"
        "function A.on_object_collider_enter(event) stellar.emit_event('A_enter', {}) end\n"
        "function A.on_object_collider_stay(event) stellar.emit_event('A_stay', {}) end\n"
        "function B.on_object_collider_enter(event) stellar.emit_event('B_enter', {}) end\n");
    assert(loaded.has_value());

    stellar::server::WorldSnapshot snapshot{};
    snapshot.object_collider_events.push_back(
        {.player_id = 1, .collider_id = 1, .name = "A", .entered = true, .stayed = true});
    snapshot.object_collider_events.push_back(
        {.player_id = 1, .collider_id = 2, .name = "B", .entered = true});
    auto result = system.process_object_collider_events(runtime, snapshot);

    assert(result.errors.empty());
    assert(result.output_events.size() == 3);
    assert(result.output_events[0].name == "A_enter");
    assert(result.output_events[1].name == "A_stay");
    assert(result.output_events[2].name == "B_enter");
}

} // namespace

int main() {
    object_collider_enter_invokes_bound_script();
    pickup_enter_emits_native_collect_command_without_lua_binding();
    object_collider_stay_and_exit_invoke_hooks();
    unbound_object_collider_event_is_ignored();
    script_errors_are_reported_without_crash();
    event_order_is_deterministic();
    return EXIT_SUCCESS;
}
