#pragma once

#include <string>
#include <vector>

#include "stellar/server/WorldSession.hpp"
#include "stellar/scripting/LuaRuntime.hpp"
#include "stellar/scripting/ScriptError.hpp"
#include "stellar/scripting/ScriptValue.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::scripting {

/** @brief Trigger callback phase exposed to Lua scripts. */
enum class TriggerScriptPhase {
    kEnter,
    kStay,
    kExit,
};

/** @brief Result of invoking trigger scripts for one authoritative snapshot. */
struct TriggerScriptResult {
    /** @brief Events emitted by scripts through safe engine APIs. */
    std::vector<ScriptOutputEvent> output_events;

    /** @brief Script errors produced during this invocation pass. */
    std::vector<ScriptError> errors;
};

/**
 * @brief Server-side trigger scripting bridge over RuntimeWorld metadata and MovementTriggerEvents.
 */
class TriggerScriptSystem {
public:
    /** @brief Build trigger-to-script bindings from a runtime world's copied metadata. */
    explicit TriggerScriptSystem(const stellar::world::RuntimeWorld& world);

    /** @brief Invoke script hooks for authoritative trigger events in deterministic order. */
    [[nodiscard]] TriggerScriptResult process_trigger_events(
        LuaRuntime& runtime,
        const stellar::server::WorldSnapshot& snapshot) noexcept;

private:
    struct Binding {
        std::string trigger_name;
        std::string script_id;
        std::string table_name;
    };

    std::vector<Binding> bindings_;
};

} // namespace stellar::scripting
