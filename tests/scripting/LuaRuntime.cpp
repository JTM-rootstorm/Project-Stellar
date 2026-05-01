#include "stellar/scripting/LuaRuntime.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using stellar::scripting::LuaRuntime;
using stellar::scripting::LuaRuntimeConfig;
using stellar::scripting::ScriptCallContext;
using stellar::scripting::ScriptField;
using stellar::scripting::ScriptValueType;

namespace {

void require_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

ScriptField string_field(std::string key, std::string value) {
    ScriptField field{};
    field.key = std::move(key);
    field.type = ScriptValueType::kString;
    field.string_value = std::move(value);
    return field;
}

ScriptField number_field(std::string key, double value) {
    ScriptField field{};
    field.key = std::move(key);
    field.type = ScriptValueType::kNumber;
    field.number_value = value;
    return field;
}

void constructs_and_loads_script() {
    LuaRuntime runtime{};
    auto loaded = runtime.load_script("simple", "Simple = {}\nfunction Simple.run(event) end");
    require_true(loaded.has_value(), "simple script should load");
}

void table_function_call_emits_event() {
    LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "caller",
        "Caller = {}\n"
        "function Caller.run(event)\n"
        "  stellar.emit_event('called', {source = event.source_name, tick = event.tick, "
        "kind = event.kind})\n"
        "end\n");
    require_true(loaded.has_value(), "caller script should load");

    ScriptCallContext context{};
    context.tick = 42;
    context.source_name = "trigger_a";
    context.fields.push_back(string_field("kind", "enter"));
    auto called = runtime.call_table_function("caller", "Caller", "run", context);
    require_true(called.has_value(), "table function call should succeed");

    auto events = runtime.drain_output_events();
    require_true(events.size() == 1, "function should emit one event");
    require_true(events[0].name == "called", "event name should match");
    require_true(events[0].fields.size() == 3, "event should include three fields");
    require_true(events[0].fields[0].key == "kind", "fields should be sorted by key");
    require_true(events[0].fields[1].key == "source", "fields should be sorted by key");
    require_true(events[0].fields[2].key == "tick", "fields should be sorted by key");
    require_true(events[0].fields[2].number_value == 42.0, "tick should round-trip");
}

void missing_function_is_noop_success() {
    LuaRuntime runtime{};
    auto loaded = runtime.load_script("missing", "Missing = {}");
    require_true(loaded.has_value(), "missing script should load");
    auto called = runtime.call_table_function("missing", "Missing", "not_present", {});
    require_true(called.has_value(), "missing function should be a no-op success");
    require_true(runtime.drain_output_events().empty(), "missing function should emit no events");

    auto missing_table = runtime.call_table_function("missing", "NoSuchTable", "run", {});
    require_true(missing_table.has_value(), "missing table should be a no-op success in Phase 10A");
}

void syntax_and_runtime_errors_are_reported() {
    LuaRuntime runtime{};
    auto syntax = runtime.load_script("bad_syntax", "function broken(");
    require_true(!syntax.has_value(), "syntax error should be reported");
    require_true(syntax.error().script_id == "bad_syntax", "syntax error should retain script id");

    auto loaded = runtime.load_script(
        "bad_runtime",
        "Bad = {}\nfunction Bad.run(event) error('expected runtime failure') end\n");
    require_true(loaded.has_value(), "runtime-error script should load");
    auto called = runtime.call_table_function("bad_runtime", "Bad", "run", {});
    require_true(!called.has_value(), "runtime error should be reported");
    require_true(called.error().message.find("expected runtime failure") != std::string::npos,
                 "runtime error should include Lua message");
}

void emitted_events_preserve_call_order() {
    LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "order",
        "Order = {}\n"
        "function Order.run(event)\n"
        "  stellar.emit_event('first', {b = 2, a = 1})\n"
        "  stellar.emit_event('second', {name = 'after'})\n"
        "end\n");
    require_true(loaded.has_value(), "order script should load");
    auto called = runtime.call_table_function("order", "Order", "run", {});
    require_true(called.has_value(), "order call should succeed");
    auto events = runtime.drain_output_events();
    require_true(events.size() == 2, "two events should be emitted");
    require_true(events[0].name == "first", "first event should retain order");
    require_true(events[1].name == "second", "second event should retain order");
    require_true(events[0].fields[0].key == "a", "field order should be deterministic");
    require_true(events[0].fields[1].key == "b", "field order should be deterministic");
}

void sandbox_restrictions_are_installed() {
    LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "sandbox",
        "Sandbox = {}\n"
        "function Sandbox.run(event)\n"
        "  stellar.emit_event('sandbox', {io_missing = io == nil, os_missing = os == nil, "
        "package_missing = package == nil, debug_missing = debug == nil, "
        "dofile_missing = dofile == nil, loadfile_missing = loadfile == nil, "
        "require_missing = require == nil, load_missing = load == nil, "
        "math_ok = math.floor(1.8), string_ok = string.upper('a'), "
        "table_ok = table.concat({'o', 'k'})})\n"
        "end\n");
    require_true(loaded.has_value(), "sandbox script should load");
    auto called = runtime.call_table_function("sandbox", "Sandbox", "run", {});
    require_true(called.has_value(), "sandbox call should succeed");
    auto events = runtime.drain_output_events();
    require_true(events.size() == 1, "sandbox test should emit one event");

    bool saw_io = false;
    bool saw_os = false;
    bool saw_package = false;
    bool saw_debug = false;
    bool saw_require = false;
    for (const auto& field : events[0].fields) {
        if (field.key == "io_missing") {
            saw_io = field.bool_value;
        } else if (field.key == "os_missing") {
            saw_os = field.bool_value;
        } else if (field.key == "package_missing") {
            saw_package = field.bool_value;
        } else if (field.key == "debug_missing") {
            saw_debug = field.bool_value;
        } else if (field.key == "require_missing") {
            saw_require = field.bool_value;
        }
    }
    require_true(saw_io && saw_os && saw_package && saw_debug && saw_require,
                 "restricted libraries should be unavailable");
}

void emit_event_rejects_nested_fields() {
    LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "nested",
        "Nested = {}\nfunction Nested.run(event) stellar.emit_event('bad', {nested = {x = 1}}) end\n");
    require_true(loaded.has_value(), "nested script should load");
    auto called = runtime.call_table_function("nested", "Nested", "run", {});
    require_true(!called.has_value(), "nested event field should fail protected call");
    require_true(runtime.drain_output_events().empty(), "failed event should not be buffered");
}

void instruction_budget_rejects_infinite_loop() {
    LuaRuntime runtime{LuaRuntimeConfig{.instruction_budget = 100, .restricted_sandbox = true}};
    auto loaded = runtime.load_script(
        "loop",
        "Loop = {}\nfunction Loop.run(event) while true do end end\n");
    require_true(loaded.has_value(), "loop script should load");
    auto called = runtime.call_table_function("loop", "Loop", "run", {});
    require_true(!called.has_value(), "infinite loop should exceed instruction budget");
    require_true(called.error().message.find("instruction budget") != std::string::npos,
                 "budget error should be deterministic");
}

void bytecode_loading_is_rejected() {
    LuaRuntime runtime{};
    const std::string bytecode_like(1, static_cast<char>(0x1b));
    auto loaded = runtime.load_script("bytecode", bytecode_like + "Lua");
    require_true(!loaded.has_value(), "bytecode-looking chunks should be rejected");
}

} // namespace

int main() {
    constructs_and_loads_script();
    table_function_call_emits_event();
    missing_function_is_noop_success();
    syntax_and_runtime_errors_are_reported();
    emitted_events_preserve_call_order();
    sandbox_restrictions_are_installed();
    emit_event_rejects_nested_fields();
    instruction_budget_rejects_infinite_loop();
    bytecode_loading_is_rejected();
    return EXIT_SUCCESS;
}
