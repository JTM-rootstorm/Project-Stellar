#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "stellar/server/WorldSession.hpp"
#include "stellar/scripting/LuaRuntime.hpp"
#include "stellar/scripting/ScriptError.hpp"
#include "stellar/scripting/ScriptValue.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::scripting {

/** @brief Object-collider callback phase exposed to Lua scripts. */
enum class ObjectColliderScriptPhase {
    kEnter,
    kStay,
    kExit,
};

/** @brief Result of invoking object-collider scripts for one authoritative snapshot. */
struct ObjectColliderScriptResult {
    /** @brief Events emitted by scripts through safe engine APIs. */
    std::vector<ScriptOutputEvent> output_events;

    /** @brief Script errors produced during this invocation pass. */
    std::vector<ScriptError> errors;
};

/** @brief Invokes Lua hooks for server-authoritative object collider events. */
class ObjectColliderScriptSystem {
public:
    /** @brief Build script bindings from RuntimeWorld object collider metadata. */
    explicit ObjectColliderScriptSystem(const stellar::world::RuntimeWorld& world);

    /** @brief Process object collider events from an authoritative WorldSnapshot. */
    [[nodiscard]] ObjectColliderScriptResult process_object_collider_events(
        LuaRuntime& runtime,
        const stellar::server::WorldSnapshot& snapshot) noexcept;

private:
    struct Binding {
        std::uint32_t collider_id = 0;
        std::string collider_name;
        std::string script_id;
        std::string table_name;
    };

    std::vector<Binding> bindings_;
};

} // namespace stellar::scripting
