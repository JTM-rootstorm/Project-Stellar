#include "stellar/scripting/ScriptHookDispatcher.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

stellar::scripting::ScriptCallContext context(std::uint64_t tick = 1) {
    stellar::scripting::ScriptCallContext ctx{};
    ctx.tick = tick;
    ctx.source_name = "Source";
    return ctx;
}

void dispatch_preserves_call_order_and_drains_outputs_per_call() {
    stellar::scripting::LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "ordered",
        "A = {}; B = {}\n"
        "function A.on_event(event) stellar.emit_event('a', {tick = event.tick}) end\n"
        "function B.on_event(event) stellar.emit_event('b', {}) end\n");
    require_true(loaded.has_value(), "ordered script should load");

    const std::vector calls{
        stellar::scripting::ScriptHookCall{.script_id = "ordered",
                                           .table_name = "A",
                                           .function_name = "on_event",
                                           .context = context(7)},
        stellar::scripting::ScriptHookCall{.script_id = "ordered",
                                           .table_name = "B",
                                           .function_name = "on_event",
                                           .context = context(8)},
    };

    auto result = stellar::scripting::dispatch_script_hooks(runtime, calls);
    require_true(result.errors.empty(), "ordered dispatch should not error");
    require_true(result.output_events.size() == 2, "ordered dispatch should emit two events");
    require_true(result.output_events[0].name == "a", "first hook output should be first");
    require_true(result.output_events[1].name == "b", "second hook output should be second");
}

void missing_table_reports_deterministic_error() {
    stellar::scripting::LuaRuntime runtime{};
    const std::vector calls{
        stellar::scripting::ScriptHookCall{.script_id = "missing",
                                           .table_name = "MissingTable",
                                           .function_name = "on_event",
                                           .context = context()},
    };

    auto result = stellar::scripting::dispatch_script_hooks(runtime, calls);
    require_true(result.output_events.empty(), "missing table should emit nothing");
    require_true(result.errors.size() == 1, "missing table should report one error");
    require_true(result.errors[0].script_id == "missing", "missing table script id");
    require_true(result.errors[0].operation == "on_event", "missing table callback name");
    require_true(result.errors[0].message.find("MissingTable") != std::string::npos,
                 "missing table error should name table");
}

void missing_function_is_noop() {
    stellar::scripting::LuaRuntime runtime{};
    auto loaded = runtime.load_script("quiet", "Quiet = {}\n");
    require_true(loaded.has_value(), "quiet script should load");

    const std::vector calls{
        stellar::scripting::ScriptHookCall{.script_id = "quiet",
                                           .table_name = "Quiet",
                                           .function_name = "on_missing",
                                           .context = context()},
    };

    auto result = stellar::scripting::dispatch_script_hooks(runtime, calls);
    require_true(result.errors.empty(), "missing function should not error");
    require_true(result.output_events.empty(), "missing function should not emit");
}

void runtime_error_does_not_stop_later_calls() {
    stellar::scripting::LuaRuntime runtime{};
    auto loaded = runtime.load_script(
        "mixed",
        "Bad = {}; Good = {}\n"
        "function Bad.on_event(event) error('expected failure') end\n"
        "function Good.on_event(event) stellar.emit_event('good_after_bad', {}) end\n");
    require_true(loaded.has_value(), "mixed script should load");

    const std::vector calls{
        stellar::scripting::ScriptHookCall{.script_id = "mixed",
                                           .table_name = "Bad",
                                           .function_name = "on_event",
                                           .context = context()},
        stellar::scripting::ScriptHookCall{.script_id = "mixed",
                                           .table_name = "Good",
                                           .function_name = "on_event",
                                           .context = context()},
    };

    auto result = stellar::scripting::dispatch_script_hooks(runtime, calls);
    require_true(result.errors.size() == 1, "runtime error should be reported once");
    require_true(result.errors[0].message.find("expected failure") != std::string::npos,
                 "runtime error should include Lua message");
    require_true(result.output_events.size() == 1, "later hook should still emit");
    require_true(result.output_events[0].name == "good_after_bad", "later hook output should remain");
}

} // namespace

int main() {
    dispatch_preserves_call_order_and_drains_outputs_per_call();
    missing_table_reports_deterministic_error();
    missing_function_is_noop();
    runtime_error_does_not_stop_later_calls();
    return EXIT_SUCCESS;
}
