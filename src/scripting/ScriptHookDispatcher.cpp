#include "stellar/scripting/ScriptHookDispatcher.hpp"

#include <iterator>
#include <utility>

namespace stellar::scripting {
namespace {

void append_drained_outputs(LuaRuntime& runtime, ScriptHookDispatchResult& result) {
    std::vector<ScriptOutputEvent> events = runtime.drain_output_events();
    result.output_events.insert(result.output_events.end(), std::make_move_iterator(events.begin()),
                                std::make_move_iterator(events.end()));
}

} // namespace

ScriptHookDispatchResult dispatch_script_hooks(LuaRuntime& runtime,
                                               std::span<const ScriptHookCall> calls) noexcept {
    ScriptHookDispatchResult result{};

    for (const ScriptHookCall& call : calls) {
        if (!runtime.has_table(call.table_name)) {
            result.errors.push_back(ScriptError{call.script_id, call.function_name,
                                                "Lua script table is missing: " + call.table_name});
            continue;
        }

        auto called = runtime.call_table_function(call.script_id, call.table_name,
                                                  call.function_name, call.context);
        if (!called.has_value()) {
            result.errors.push_back(std::move(called.error()));
        }
        append_drained_outputs(runtime, result);
    }

    return result;
}

} // namespace stellar::scripting
