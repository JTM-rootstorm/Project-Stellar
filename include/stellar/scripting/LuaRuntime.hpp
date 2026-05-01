#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "stellar/scripting/ScriptError.hpp"
#include "stellar/scripting/ScriptValue.hpp"

namespace stellar::scripting {

/** @brief Config controlling safe Lua execution behavior. */
struct LuaRuntimeConfig {
    /** @brief Maximum Lua instruction hook callbacks per protected script call. */
    int instruction_budget = 10000;

    /** @brief True when the runtime installs restricted safe standard libraries only. */
    bool restricted_sandbox = true;
};

/** @brief Context passed to Lua callbacks as an event table. */
struct ScriptCallContext {
    /** @brief Authoritative server tick that produced the callback. */
    std::uint64_t tick = 0;

    /** @brief Trigger name or other source object name for this call. */
    std::string source_name;

    /** @brief Deterministically ordered primitive fields exposed to Lua. */
    std::vector<ScriptField> fields;
};

/**
 * @brief RAII owner for a restricted Lua state used by server-authoritative scripts.
 */
class LuaRuntime {
public:
    /** @brief Opaque implementation type for the owned Lua state. */
    struct Impl;

    /** @brief Create a Lua runtime and install the configured sandbox/API. */
    explicit LuaRuntime(LuaRuntimeConfig config = {});

    /** @brief Destroy the Lua state and all loaded script data. */
    ~LuaRuntime() noexcept;

    LuaRuntime(const LuaRuntime&) = delete;
    LuaRuntime& operator=(const LuaRuntime&) = delete;
    /** @brief Move a Lua runtime, transferring state ownership. */
    LuaRuntime(LuaRuntime&& other) noexcept;
    /** @brief Move-assign a Lua runtime, replacing any existing state. */
    LuaRuntime& operator=(LuaRuntime&& other) noexcept;

    /** @brief Load source text into the runtime under a stable script id. */
    [[nodiscard]] std::expected<void, ScriptError> load_script(std::string_view script_id,
                                                               std::string_view source);

    /** @brief Protected-call `table_name.function_name(context)` if present. */
    [[nodiscard]] std::expected<void, ScriptError> call_table_function(
        std::string_view script_id,
        std::string_view table_name,
        std::string_view function_name,
        const ScriptCallContext& context) noexcept;

    /** @brief Return true when a global Lua table exists for deterministic higher-level policy. */
    [[nodiscard]] bool has_table(std::string_view table_name) const noexcept;

    /** @brief Return and clear events emitted by Lua through `stellar.emit_event`. */
    [[nodiscard]] std::vector<ScriptOutputEvent> drain_output_events();

private:
    Impl* impl_ = nullptr;
};

} // namespace stellar::scripting
