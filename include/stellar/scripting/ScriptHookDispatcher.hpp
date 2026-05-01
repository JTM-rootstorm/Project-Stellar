#pragma once

#include <span>
#include <string>
#include <vector>

#include "stellar/scripting/LuaRuntime.hpp"
#include "stellar/scripting/ScriptError.hpp"
#include "stellar/scripting/ScriptValue.hpp"

namespace stellar::scripting {

/** @brief Description of one Lua table hook invocation owned by a caller-specific script system. */
struct ScriptHookCall {
    /** @brief Stable script id used for error reporting and protected-call attribution. */
    std::string script_id;

    /** @brief Global Lua table expected to contain the hook function. */
    std::string table_name;

    /** @brief Function name to call on the Lua table when present. */
    std::string function_name;

    /** @brief Caller-built context table passed to the Lua hook. */
    ScriptCallContext context;
};

/** @brief Outputs and errors produced by dispatching a deterministic batch of Lua hooks. */
struct ScriptHookDispatchResult {
    /** @brief Events emitted by scripts through safe engine APIs in dispatch order. */
    std::vector<ScriptOutputEvent> output_events;

    /** @brief Script errors produced while verifying tables or protected-calling hooks. */
    std::vector<ScriptError> errors;
};

/**
 * @brief Dispatch Lua table hooks in span order, preserving per-call output draining semantics.
 *
 * Missing tables produce the existing table-missing ScriptError. Missing functions remain a no-op
 * through LuaRuntime::call_table_function. Output events are drained after each attempted protected
 * hook call to preserve script callback/output interleaving.
 */
[[nodiscard]] ScriptHookDispatchResult dispatch_script_hooks(
    LuaRuntime& runtime,
    std::span<const ScriptHookCall> calls) noexcept;

} // namespace stellar::scripting
