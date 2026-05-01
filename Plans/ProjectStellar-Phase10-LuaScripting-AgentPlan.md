# Project Stellar — Phase 10 Lua Scripting Combined Implementation Plan

**Branch target:** `lua-scripting`  
**Repository:** `JTM-rootstorm/Project-Stellar`  
**Prepared for:** AI implementation agents such as Codex, Kilo, or other code-writing agents  
**Generated:** 2026-05-01  
**Recommended PR strategy:** One phase per PR/branch slice  
**Primary goal:** Add server-authoritative Lua scripting so triggers, world objects, and future entities can be scripted instead of compiled, while preserving the current backend-neutral, display-free, deterministic validation posture.

---

## 0. AI Agent Intake Summary

### Task packet

Implement a Lua scripting subsystem for Project Stellar in controlled phases. Do **not** restart earlier collision, movement, world metadata, trigger, runtime world, or local loopback work. The branch already has the correct runtime seam: glTF metadata becomes `RuntimeWorld`; authoritative movement produces `MovementTriggerEvent`s; `WorldSession` emits deterministic snapshots and trigger events.

The first scripting implementation should attach to that seam, not to rendering or client-side presentation.

### Mandatory architectural decision

Use **Lua 5.4.x**, embedded behind an engine-owned `stellar_scripting` C++ wrapper using the raw Lua C API. Do not introduce sol2, LuaBridge, ChaiScript, AngelScript, Wren, QuickJS, or another language/binding wrapper in the initial implementation slice.

### Core rule

Scripts are gameplay code. Gameplay is server-authoritative. Therefore, scripts run on the authoritative server/runtime side and emit validated commands/events through explicit C++ APIs. Scripts must not directly mutate renderer, audio, platform, or raw C++ object state.

### First useful feature

Trigger script hooks:

- `on_trigger_enter(event)`
- `on_trigger_stay(event)`
- `on_trigger_exit(event)`

These should be invoked from authoritative trigger events after server movement has resolved.

### Documentation requirement

Update both:

1. `docs/ImplementationStatus.md` — current branch-facing authority.
2. `docs/Design.md` — broad architecture and future design.

`docs/ImplementationStatus.md` currently still describes `collision-movement` as the branch target. Change it to `lua-scripting` when Phase 10 begins.

---

## 1. Current Branch Context Snapshot

Treat these as already implemented first passes:

- Static glTF collision extraction into backend-neutral collision data.
- Collision queries, BVH pruning, ground probing, and character movement.
- Billboard sprite rendering data and metadata markers.
- World metadata extraction from glTF node conventions.
- Runtime world assembly over imported `SceneAsset` data.
- Trigger volumes and deterministic trigger enter/stay/exit state.
- Server-authoritative movement simulation.
- `WorldSession` for one local authoritative player slot.
- `LocalLoopbackRuntime` for in-process local/single-player runtime presentation.
- Playable-world smoke test covering import -> validation -> runtime world -> authoritative session.

Important existing seams:

- `stellar::world::RuntimeWorld` copies world metadata and optionally owns a collision query world.
- `stellar::world::TriggerSystem` already converts metadata trigger markers into deterministic trigger transitions.
- `stellar::server::MovementTriggerTracker` already converts runtime trigger overlaps into authoritative `MovementTriggerEvent`s.
- `stellar::server::WorldSession::tick()` already emits `WorldSnapshot` containing `trigger_events`.
- `WorldMarker::extras_json` already preserves raw glTF node extras, but validation currently warns that it is unparsed.

---

## 2. Non-Negotiable Constraints

### Engine architecture constraints

- Keep server authority intact.
- Keep the client as presentation unless a future plan explicitly scopes client-side prediction or presentation-only scripts.
- Keep renderer and audio handles out of authoritative gameplay state.
- Keep default tests display-free.
- Preserve OpenGL/Vulkan parity assumptions by not touching rendering for this phase.
- Keep collision and metadata backend-neutral.
- Use small explicit C++ APIs.
- Use RAII for Lua state ownership.
- Public APIs require Doxygen `@brief` comments.
- Prefer `std::expected<T, Error>` or a project-compatible equivalent for fallible operations.
- Do not use exceptions as normal control flow.
- Preserve deterministic ordering for script invocation and script output events.

### Scripting constraints

- Import must not execute scripts.
- Scripts must not receive raw C++ pointers.
- Scripts must not directly mutate renderer, audio, platform, file system, OS, or process state.
- Scripts must not trust client-provided state.
- Scripts must be called through protected Lua calls.
- Runtime errors become deterministic diagnostics or script error events, not crashes.
- Add an instruction/call budget to prevent a script from hanging a server tick.
- Random behavior, if added later, must use server-owned seeded RNG APIs rather than wall-clock or process-global randomness.

### Dependency constraints

- Vendor or otherwise pin Lua. Do not rely on an unpinned system Lua package for the core implementation.
- Avoid adding a general JSON dependency in the first slice unless absolutely required.
- Do not introduce a third-party C++ Lua binding wrapper in Phase 10A-10C.

---

## 3. Recommended Module Boundaries

Implement this layering:

```text
stellar_world          # metadata, runtime world, trigger volumes
stellar_server_core    # authoritative movement/session/events; no Lua dependency
stellar_scripting      # Lua runtime + scripted wrapper around server/world
stellar_client_runtime # may optionally consume scripted outputs later; no initial Lua dependency
```

Do **not** link Lua directly into `stellar_world` or `stellar_server_core`.

Reason: `stellar_server_core` should remain the deterministic native authoritative runtime. `stellar_scripting` should be an opt-in layer over it.

---

## 4. Phase Overview

### Phase 10A — Lua Runtime Foundation

Add optional Lua scripting support and a safe engine-owned Lua wrapper.

### Phase 10B — Script Binding Metadata

Allow world metadata markers to reference script IDs/tables without executing scripts during import.

### Phase 10C — Trigger Script Hooks

Invoke Lua callbacks from authoritative trigger enter/stay/exit events and buffer deterministic script outputs.

### Phase 10D — Scripted Session Wrapper

Add a wrapper around `WorldSession` that runs authoritative ticks and then invokes scripts from emitted events.

### Phase 10E — Documentation and Playable Integration Smoke

Update `Design.md`, `ImplementationStatus.md`, and extend the playable-world smoke path or add a separate scripting smoke test.

### Deferred Phase 11+ — Entity/Object Scripting

Add entity/object hooks only after ECS/runtime entity ownership is concrete.

---

## 5. Phase 10A — Lua Runtime Foundation

### Objective

Create the `stellar_scripting` target and implement a small RAII Lua runtime wrapper with protected calls, sandbox setup, error reporting, and display-free tests.

### Files to add

```text
include/stellar/scripting/ScriptError.hpp
include/stellar/scripting/LuaRuntime.hpp
include/stellar/scripting/ScriptValue.hpp
include/stellar/scripting/ScriptSandbox.hpp
src/scripting/LuaRuntime.cpp
src/scripting/ScriptSandbox.cpp
tests/scripting/LuaRuntime.cpp
```

### Files to modify

```text
CMakeLists.txt
```

### CMake requirements

Add an option:

```cmake
option(STELLAR_ENABLE_LUA_SCRIPTING "Enable Lua scripting runtime" ON)
```

Add a vendored/pinned Lua dependency under:

```text
thirdparty/lua/
```

The exact third-party CMake layout can vary, but the final graph should provide a `lua` or `stellar_lua` target linked privately by `stellar_scripting`.

Representative CMake shape:

```cmake
if(STELLAR_ENABLE_LUA_SCRIPTING)
    add_subdirectory(thirdparty/lua)

    add_library(stellar_scripting
        src/scripting/LuaRuntime.cpp
        src/scripting/ScriptSandbox.cpp
    )

    target_include_directories(stellar_scripting PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(stellar_scripting PUBLIC
        stellar_world
        stellar_server_core
    )

    target_link_libraries(stellar_scripting PRIVATE
        lua
    )

    add_executable(stellar_lua_runtime_test
        tests/scripting/LuaRuntime.cpp
    )

    target_include_directories(stellar_lua_runtime_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(stellar_lua_runtime_test PRIVATE
        stellar_scripting
    )

    add_test(NAME lua_runtime COMMAND stellar_lua_runtime_test)
endif()
```

If the vendored Lua target name differs, keep the public target named `stellar_scripting`.

### Public API: `ScriptError.hpp`

```cpp
#pragma once

#include <string>

namespace stellar::scripting {

/** @brief Error produced while loading or executing a script. */
struct ScriptError {
    /** @brief Stable script identifier, path, or marker-provided script id. */
    std::string script_id;

    /** @brief Lua table/function name or engine operation that failed. */
    std::string operation;

    /** @brief Human-readable diagnostic message. */
    std::string message;
};

} // namespace stellar::scripting
```

### Public API: `ScriptValue.hpp`

Use simple value types to avoid adding a JSON dependency in the first scripting phase.

```cpp
#pragma once

#include <string>
#include <vector>

namespace stellar::scripting {

/** @brief Primitive script value type accepted by initial engine event output. */
enum class ScriptValueType {
    kNil,
    kBoolean,
    kNumber,
    kString,
};

/** @brief Primitive key/value emitted by script code through safe engine APIs. */
struct ScriptField {
    /** @brief Stable field key. */
    std::string key;

    /** @brief Type of the stored script value. */
    ScriptValueType type = ScriptValueType::kNil;

    /** @brief Boolean value when type is kBoolean. */
    bool bool_value = false;

    /** @brief Numeric value when type is kNumber. */
    double number_value = 0.0;

    /** @brief String value when type is kString. */
    std::string string_value;
};

/** @brief Event emitted by a script and later validated/applied by native server code. */
struct ScriptOutputEvent {
    /** @brief Stable event name. */
    std::string name;

    /** @brief Deterministically ordered primitive event fields. */
    std::vector<ScriptField> fields;
};

} // namespace stellar::scripting
```

### Public API: `LuaRuntime.hpp`

```cpp
#pragma once

#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "stellar/scripting/ScriptError.hpp"
#include "stellar/scripting/ScriptValue.hpp"

namespace stellar::scripting {

/** @brief Config controlling safe Lua execution behavior. */
struct LuaRuntimeConfig {
    /** @brief Maximum instruction hook count per protected script call. */
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
    /** @brief Create a Lua runtime and install the configured sandbox/API. */
    explicit LuaRuntime(LuaRuntimeConfig config = {});

    /** @brief Destroy the Lua state and all loaded script data. */
    ~LuaRuntime() noexcept;

    LuaRuntime(const LuaRuntime&) = delete;
    LuaRuntime& operator=(const LuaRuntime&) = delete;
    LuaRuntime(LuaRuntime&&) noexcept;
    LuaRuntime& operator=(LuaRuntime&&) noexcept;

    /** @brief Load source text into the runtime under a stable script id. */
    [[nodiscard]] std::expected<void, ScriptError> load_script(
        std::string_view script_id,
        std::string_view source);

    /** @brief Protected-call `table_name.function_name(context)` if present. */
    [[nodiscard]] std::expected<void, ScriptError> call_table_function(
        std::string_view script_id,
        std::string_view table_name,
        std::string_view function_name,
        const ScriptCallContext& context) noexcept;

    /** @brief Return and clear events emitted by Lua through `stellar.emit_event`. */
    [[nodiscard]] std::vector<ScriptOutputEvent> drain_output_events();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace stellar::scripting
```

### Sandbox requirements

Allowed by default:

- Basic Lua control-flow and table behavior required for normal scripts.
- `math`
- `string`
- `table`
- Engine-provided `stellar` API table.

Disabled by default:

- `io`
- `os`
- `package`
- `debug`
- `dofile`
- `loadfile`
- unrestricted `require`
- bytecode loading

Engine API for initial slice:

```lua
stellar.emit_event(name, fields)
```

Where `fields` may contain only primitive string keys and nil/boolean/number/string values. Reject nested tables in Phase 10A.

### Implementation notes

- Use `luaL_newstate` and close through RAII.
- Use protected calls such as `lua_pcall`.
- Use a Lua hook for instruction budget enforcement.
- Convert Lua errors to `ScriptError`.
- Clear script call stack deterministically after errors.
- Do not expose C++ object pointers to Lua.
- Do not execute script code during import; only runtime load/call should execute.
- `load_script` may execute top-level Lua chunk code to define tables/functions. That is acceptable only when explicitly loading scripts into the runtime, not during glTF import.

### Phase 10A tests

Add display-free tests for:

- Runtime constructs and destructs.
- Simple script loads.
- Table function call works.
- Missing function is a no-op success or deterministic warning. Choose one policy and test it.
- Syntax error returns `ScriptError`.
- Runtime error returns `ScriptError`.
- `stellar.emit_event` records output events in deterministic order.
- `io`, `os`, `package`, and `debug` are unavailable in restricted mode.
- Instruction budget rejects an infinite loop or very long loop without hanging tests.

Recommended test executable:

```text
tests/scripting/LuaRuntime.cpp
```

Recommended CTest name:

```text
lua_runtime
```

### Phase 10A acceptance criteria

- `STELLAR_ENABLE_LUA_SCRIPTING=ON` builds a `stellar_scripting` library.
- Lua is vendored/pinned or otherwise reproducibly available.
- Default scripting tests are display-free.
- No rendering, audio, platform windowing, OpenGL, or Vulkan code is touched.
- `stellar_server_core` does not link to Lua.
- All public scripting APIs have Doxygen `@brief` comments.

---

## 6. Phase 10B — Script Binding Metadata

### Objective

Allow imported/assembled world metadata to carry script binding information without executing scripts.

### Files to modify

```text
include/stellar/assets/WorldMetadataAsset.hpp
src/import/gltf/WorldMetadataImport.cpp
src/world/WorldMetadataValidation.cpp
include/stellar/world/WorldMetadataValidation.hpp
tests/world/WorldMetadataValidation.cpp
tests/import/gltf/ImporterRegression.cpp
```

### Public API change: `WorldMetadataAsset.hpp`

Add a script binding type:

```cpp
#include <optional>
```

```cpp
namespace stellar::assets {

/** @brief Optional script binding attached to an authored world marker. */
struct WorldScriptBinding {
    /** @brief Stable script identifier or asset-relative script path. */
    std::string script_id;

    /** @brief Optional Lua table name containing hook functions. */
    std::string table_name;
};

/**
 * @brief Backend-neutral authored marker from a world or level source asset.
 */
struct WorldMarker {
    WorldMarkerType type = WorldMarkerType::kEntitySpawn;
    std::string name;
    std::string archetype;
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
    std::string extras_json;

    /** @brief Optional script binding copied from authoring metadata. */
    std::optional<WorldScriptBinding> script;
};

} // namespace stellar::assets
```

If minimizing field churn is preferred, direct `std::string script_id` and `std::string script_table` fields are acceptable. The `std::optional<WorldScriptBinding>` shape is cleaner and self-documenting.

### glTF authoring convention

Support this convention in node `extras`:

```json
{
  "stellar": {
    "script": "scripts/door.lua",
    "table": "Door"
  }
}
```

Interpretation:

- `stellar.script` -> `WorldScriptBinding::script_id`
- `stellar.table` -> `WorldScriptBinding::table_name`

Both values must be strings.

If `table` is omitted, the runtime may derive a table name later from marker name or script file stem, but Phase 10B should prefer explicit `table`.

### Parsing policy

The importer currently preserves raw `extras_json`. For Phase 10B, implement one of these policies:

Preferred minimal policy:

- Keep `extras_json` unchanged.
- Add a small helper that extracts only `stellar.script` and `stellar.table` string values from raw JSON.
- Do not add a general JSON dependency.
- Reject malformed or unsupported script binding shapes by leaving `script` empty and allowing validation to warn from `extras_json` if needed.

Alternative if a JSON utility already exists in-tree:

- Use it only for metadata extraction.
- Do not widen glTF extension support.

### Validation additions

Add validation findings:

- `script_binding_empty_script_id` — error for script binding with empty script id.
- `script_binding_empty_table_name` — warning for script binding with empty table name.
- `script_binding_path_traversal` — error or warning for script id containing `..`, absolute path prefixes, or platform path escape patterns.
- `script_binding_unsupported_marker_type` — optional warning if scripts are attached to marker types not yet invoked.

Do not fail runtime world assembly by default. Keep validation as deterministic diagnostics.

### Phase 10B tests

Add or extend tests for:

- Marker with no extras remains scriptless.
- Marker with valid `stellar.script` and `stellar.table` creates `WorldScriptBinding`.
- Trigger marker script binding survives import -> `SceneAsset` -> `RuntimeWorld` copy.
- Invalid or empty script id creates deterministic validation finding.
- Raw `extras_json` behavior remains backward-compatible.
- Import still does not execute scripts.

### Phase 10B acceptance criteria

- `WorldMarker` can carry optional script binding data.
- glTF metadata extraction supports the documented extras shape.
- Runtime world copies script binding data through existing metadata copy behavior.
- Validation reports deterministic script binding diagnostics.
- No Lua runtime is required for import-only tests.

---

## 7. Phase 10C — Trigger Script Hooks

### Objective

Bind trigger markers to Lua script hooks and invoke them from authoritative trigger events after server movement resolves.

### Files to add

```text
include/stellar/scripting/TriggerScriptSystem.hpp
src/scripting/TriggerScriptSystem.cpp
tests/scripting/TriggerScriptSystem.cpp
```

### Files to modify

```text
CMakeLists.txt
```

### Public API: `TriggerScriptSystem.hpp`

```cpp
#pragma once

#include <span>
#include <string>
#include <vector>

#include "stellar/assets/WorldMetadataAsset.hpp"
#include "stellar/server/MovementTriggerIntegration.hpp"
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
    struct Binding;
    std::vector<Binding> bindings_;
};

} // namespace stellar::scripting
```

### Binding behavior

Build bindings from `RuntimeWorld::world_metadata.markers` where:

- marker type is `WorldMarkerType::kTrigger`
- marker name matches `MovementTriggerEvent::trigger_name`
- marker has `script` binding

Sorting/order:

- Preserve metadata order for binding construction.
- Process `WorldSnapshot::trigger_events` in snapshot order.
- For duplicate trigger names, invoke matching script bindings in metadata order.
- Drain script output events after each callback or after the full pass, but ensure final output ordering is deterministic. Preferred: drain after each callback and append to result.

### Lua callback names

Map event flags to callbacks:

- `entered == true` -> `on_trigger_enter(event)`
- `stayed == true` -> `on_trigger_stay(event)`
- `exited == true` -> `on_trigger_exit(event)`

A single `MovementTriggerEvent` should normally have only one transition flag. If multiple flags are true because of a future change, process in this stable order:

1. enter
2. stay
3. exit

### Lua event table fields

Pass a Lua table with at least:

```lua
{
    tick = 123,
    trigger_name = "DoorOpen",
    phase = "enter",
    player_id = 1,
    player_position_x = 0.0,
    player_position_y = 0.0,
    player_position_z = 0.0
}
```

If multiple players are added later, Phase 10C may use the first local player snapshot because the current `WorldSession` is a one-player implementation. Document this limitation.

### Example script

```lua
Door = {}

function Door.on_trigger_enter(event)
    stellar.emit_event("door_open_requested", {
        trigger = event.trigger_name,
        phase = event.phase,
        tick = event.tick
    })
end

function Door.on_trigger_stay(event)
    stellar.emit_event("door_still_open", {
        trigger = event.trigger_name
    })
end

function Door.on_trigger_exit(event)
    stellar.emit_event("door_close_requested", {
        trigger = event.trigger_name
    })
end
```

### Missing callback policy

Recommended policy:

- Missing script table: return `ScriptError`.
- Missing callback function on an existing table: no-op success.
- Runtime error in callback: append `ScriptError`; continue to later callbacks unless the runtime stack is compromised.

Document and test this policy.

### Phase 10C tests

Add display-free tests for:

- Trigger enter invokes `on_trigger_enter`.
- Trigger stay invokes `on_trigger_stay`.
- Trigger exit invokes `on_trigger_exit`.
- Missing callback is no-op.
- Missing table produces deterministic error.
- Runtime error in one trigger script does not crash and does not prevent deterministic reporting.
- Duplicate trigger names process in deterministic metadata order.
- Script output event order is deterministic across repeated runs.
- A trigger marker without script binding is ignored.

### Phase 10C acceptance criteria

- Trigger scripts run only from authoritative trigger events.
- Script outputs are buffered, not directly applied to engine state.
- No rendering, audio, or platform dependency is introduced.
- Tests remain display-free.

---

## 8. Phase 10D — Scripted Session Wrapper

### Objective

Provide a clean high-level runtime wrapper that ticks the authoritative `WorldSession`, then processes script hooks from the emitted snapshot/events.

### Files to add

```text
include/stellar/scripting/ScriptRegistry.hpp
include/stellar/scripting/ScriptedWorldSession.hpp
src/scripting/ScriptRegistry.cpp
src/scripting/ScriptedWorldSession.cpp
tests/scripting/ScriptedWorldSession.cpp
```

### Public API: `ScriptRegistry.hpp`

```cpp
#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>

#include "stellar/scripting/ScriptError.hpp"

namespace stellar::scripting {

/** @brief In-memory registry of script sources keyed by stable script id. */
class ScriptRegistry {
public:
    /** @brief Add or replace a script source by id. */
    void set_script(std::string script_id, std::string source);

    /** @brief Find script source text by id, or return nullptr when absent. */
    [[nodiscard]] const std::string* find_script(std::string_view script_id) const noexcept;

private:
    std::unordered_map<std::string, std::string> scripts_;
};

} // namespace stellar::scripting
```

Initial registry can be memory-only. File-backed loading can be deferred or added behind explicit asset-root validation.

### Public API: `ScriptedWorldSession.hpp`

```cpp
#pragma once

#include <span>
#include <vector>

#include "stellar/server/WorldSession.hpp"
#include "stellar/scripting/LuaRuntime.hpp"
#include "stellar/scripting/ScriptRegistry.hpp"
#include "stellar/scripting/ScriptValue.hpp"
#include "stellar/scripting/TriggerScriptSystem.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::scripting {

/** @brief Result of one authoritative scripted world tick. */
struct ScriptedWorldFrame {
    /** @brief Authoritative snapshot after native movement/session simulation. */
    stellar::server::WorldSnapshot snapshot;

    /** @brief Events emitted by scripts during this frame. */
    std::vector<ScriptOutputEvent> script_events;

    /** @brief Script errors produced during this frame. */
    std::vector<ScriptError> script_errors;
};

/**
 * @brief Authoritative world-session wrapper that invokes server-side scripts after native ticks.
 */
class ScriptedWorldSession {
public:
    /** @brief Construct a scripted session over caller-owned runtime world data. */
    ScriptedWorldSession(const stellar::world::RuntimeWorld& world,
                         stellar::server::WorldSessionConfig session_config,
                         ScriptRegistry registry,
                         LuaRuntimeConfig lua_config = {});

    /** @brief Advance one authoritative tick and process script callbacks from emitted events. */
    [[nodiscard]] ScriptedWorldFrame tick(
        std::span<const stellar::server::PlayerCommand> commands) noexcept;

    /** @brief Return latest authoritative snapshot without replaying script events. */
    [[nodiscard]] const stellar::server::WorldSnapshot& latest_snapshot() const noexcept;

private:
    ScriptRegistry registry_;
    LuaRuntime runtime_;
    stellar::server::WorldSession session_;
    TriggerScriptSystem trigger_scripts_;
    stellar::server::WorldSnapshot latest_snapshot_{};
};

} // namespace stellar::scripting
```

### Script loading policy

On construction:

- Inspect trigger script bindings.
- Load each unique `script_id` from `ScriptRegistry` into `LuaRuntime` once.
- Missing script source should be reported deterministically.

Because constructors cannot return `std::expected`, either:

1. Add a static factory:

```cpp
static std::expected<ScriptedWorldSession, ScriptError> create(...);
```

or

2. Store construction errors and expose them through an accessor.

Preferred: use a static factory for fallible construction.

### Recommended factory shape

```cpp
class ScriptedWorldSession {
public:
    /** @brief Create a scripted session and load all scripts required by current world metadata. */
    [[nodiscard]] static std::expected<ScriptedWorldSession, ScriptError> create(
        const stellar::world::RuntimeWorld& world,
        stellar::server::WorldSessionConfig session_config,
        ScriptRegistry registry,
        LuaRuntimeConfig lua_config = {});

    // ...
};
```

If move construction becomes awkward because `LuaRuntime` owns a raw `lua_State`, ensure `LuaRuntime` is safely movable or store it in a private `std::unique_ptr` implementation.

### Phase 10D tests

Add display-free tests for:

- `ScriptedWorldSession::create` loads required scripts.
- Missing script id returns deterministic error.
- `tick()` emits native snapshot and script events.
- Repeated scripted path produces identical snapshots and script events.
- Script errors are reported without crashing the session.
- `latest_snapshot()` does not replay script events.

### Phase 10D acceptance criteria

- A scripted authoritative tick path exists without changing `WorldSession` behavior.
- Native `WorldSession` tests remain unchanged.
- Scripted behavior is opt-in through `stellar_scripting`.
- Deterministic repeat-output tests pass.

---

## 9. Phase 10E — Documentation and Playable Integration Smoke

### Objective

Update architecture docs and add one integration test proving the existing playable-world path can execute a trigger script without requiring graphics/window context.

### Files to modify

```text
docs/ImplementationStatus.md
docs/Design.md
CMakeLists.txt
```

### Files to add or modify for tests

Option A — add a dedicated scripting smoke test:

```text
tests/integration/ScriptedPlayableWorldSmoke.cpp
```

Option B — extend existing playable smoke only if the diff stays readable:

```text
tests/integration/PlayableWorldSmoke.cpp
```

Preferred: add a dedicated `ScriptedPlayableWorldSmoke.cpp` to avoid overloading the Phase 9F test.

### CTest name

```text
scripted_playable_world_smoke
```

### Integration test scenario

Create/generated glTF fixture:

- Visible render geometry.
- Collision-only floor/walls.
- `SPAWN_Player` marker.
- `TRIGGER_DoorOpen` marker.
- Trigger marker `extras` with script binding:

```json
{
  "stellar": {
    "script": "scripts/door.lua",
    "table": "Door"
  }
}
```

Use in-memory `ScriptRegistry`:

```lua
Door = {}

function Door.on_trigger_enter(event)
    stellar.emit_event("door_open_requested", {
        trigger = event.trigger_name,
        tick = event.tick
    })
end
```

Validation assertions:

- Import succeeds.
- Collision validation succeeds.
- Metadata validation succeeds or reports only expected script warnings if any.
- Runtime world has player spawn and trigger marker.
- Trigger marker has script binding.
- Scripted session loads script.
- Moving through trigger emits `door_open_requested` exactly when enter event occurs.
- Repeat run produces identical snapshots and script outputs.
- Test requires no display/GPU/context.

### `docs/ImplementationStatus.md` update

At top, change:

```md
Branch target: `collision-movement`
```

to:

```md
Branch target: `lua-scripting`
```

Add a new current status section above Phase 9F:

```md
Phase 10A is active for the `lua-scripting` branch:

- Add optional `STELLAR_ENABLE_LUA_SCRIPTING` and a vendored/pinned Lua dependency.
- Add `stellar_scripting` as an opt-in server-authoritative scripting layer.
- Keep `stellar_world` and `stellar_server_core` Lua-free.
- Add RAII Lua runtime ownership, protected-call error handling, restricted sandbox behavior,
  and display-free tests.

Phase 10 planned sequence:

1. Phase 10A — Lua Runtime Foundation.
2. Phase 10B — Script Binding Metadata.
3. Phase 10C — Trigger Script Hooks.
4. Phase 10D — Scripted Session Wrapper.
5. Phase 10E — Documentation and Scripted Playable Smoke.

Do not restart Phase 6-9 work. The Phase 9F playable-world path is the integration seam for
server-authoritative trigger scripting.
```

After each phase, append completion notes at the top of the current status section.

### `docs/Design.md` update

#### Header

Update:

```md
**Version:** 0.1.5 (lua-scripting integration direction)
**Last Updated:** 2026-05-01
```

#### Key Design Principles

Add:

```md
- **Scripted gameplay through server authority:** scripts may express entity, trigger,
  object, and world behavior, but script execution is server-owned and all effects flow
  through validated engine APIs.
```

#### Technical Stack

Add:

```md
- **Scripting:** Lua embedded through an engine-owned C++ wrapper, with a restricted
  server-side API and deterministic display-free test coverage.
```

#### Current Branch Direction

Replace the old branch direction with:

```md
Current branch: `lua-scripting`.

Primary near-term goal: add an embedded scripting layer so triggers, world markers,
future entities, and gameplay objects can be authored in scripts rather than compiled
C++ while preserving server authority, deterministic tests, and renderer/audio separation.

Recommended implementation order:

1. **Phase 10A — Lua Runtime Foundation**
   - Add a `stellar_scripting` target and vendored/pinned Lua dependency.
   - Wrap `lua_State` behind RAII and expected-style error reporting.
   - Install a restricted sandbox and deterministic execution budget.
   - Keep default tests display-free.

2. **Phase 10B — Script Binding Metadata**
   - Add explicit script binding fields to backend-neutral world metadata.
   - Extract supported script binding keys from glTF node `extras`.
   - Keep import as metadata only; importing must never execute scripts.

3. **Phase 10C — Trigger Script Hooks**
   - Invoke scripts from authoritative trigger enter/stay/exit events.
   - Buffer script outputs and apply them through validated server APIs.
   - Add deterministic trigger scripting tests.

4. **Phase 10D — Scripted Session Wrapper**
   - Wrap `WorldSession` with an opt-in scripting layer.
   - Keep native server movement/session behavior available without Lua.
   - Add deterministic scripted tick tests.

5. **Phase 10E — Scripted Playable Smoke**
   - Validate import -> metadata script binding -> runtime world -> scripted authoritative session.
   - Keep smoke coverage display-free.
```

#### Architecture

Update the server responsibility text to include scripted/native game logic.

Add server responsibilities:

```md
- Own script execution for authoritative gameplay behavior.
- Validate and apply script outputs through the same server-owned rules used by native systems.
```

#### New scripting section

Add as `9.5` to avoid renumbering the whole document, or add as a new numbered section and update the table of contents.

```md
### 9.5 Scripting Subsystem

**Primary ownership:** `@carmack` for runtime/build/server integration and `@kojima`
for gameplay script contracts, coordinated by `@director` because scripting crosses
metadata, ECS, server simulation, and presentation events.

#### Scripting Goals

The scripting layer lets gameplay objects, triggers, entity archetypes, and world logic
be authored without recompiling the engine. Scripts are gameplay code, so they run under
server authority. The client may present script-approved events, but client-side scripts
must not become authoritative unless explicitly scoped as presentation-only tooling.

#### Lua Runtime Direction

Lua is the preferred embedded scripting language for the first implementation because it
has a small C API, can be vendored as a C dependency, and can be wrapped behind explicit
engine APIs without requiring renderer, audio, or ECS internals to leak into script code.

The engine owns the Lua state wrapper, script registry, sandbox policy, error handling,
and binding surface. Scripts should not receive raw C++ pointers.

#### Script Binding Model

Imported world metadata may reference script identifiers, but import must not execute
script code. glTF node `extras` may provide script binding metadata such as:

```json
{
  "stellar": {
    "script": "scripts/door.lua",
    "table": "Door"
  }
}
```

Runtime world assembly copies these script bindings into backend-neutral metadata.
Execution happens only when a server/runtime system explicitly invokes a script hook.

#### Initial Hooks

Initial supported hooks:

- `on_trigger_enter(event)`
- `on_trigger_stay(event)`
- `on_trigger_exit(event)`

Future ECS/entity hooks:

- `on_spawn(entity)`
- `on_tick(entity, dt)`
- `on_interact(entity, instigator)`
- `on_damage(entity, damage)`
- `on_destroy(entity)`

#### Script API Boundaries

Scripts may:

- read stable event context,
- read safe world/query data exposed by the server,
- emit validated gameplay or presentation events,
- request entity/object actions through command buffers once ECS support exists.

Scripts must not:

- mutate renderer/audio state directly,
- access raw C++ pointers,
- perform arbitrary file, OS, package, or debug operations,
- trust client-provided state,
- bypass server validation.

#### Determinism and Safety

Script calls should use protected Lua calls. Runtime errors become deterministic diagnostics
or server events rather than crashes. Execution order should be stable, and scripts should
have instruction/time budgets to prevent a bad script from hanging the server tick. Random
behavior should use server-owned seeded RNG APIs rather than wall-clock or process-global
random state.
```

#### Dependencies table

Add:

```md
| Lua | Server-side gameplay scripting | Vendored or otherwise pinned; compiled only when `STELLAR_ENABLE_LUA_SCRIPTING=ON` |
```

#### Build targets

Add:

```md
- `stellar_scripting` when `STELLAR_ENABLE_LUA_SCRIPTING=ON`.
```

#### Directory structure

Add:

```text
include/stellar/scripting/
src/scripting/
tests/scripting/
assets/scripts/          # optional examples/fixtures
thirdparty/lua/
```

#### Testing strategy

Add:

```md
Scripting tests must be display-free by default and should cover:

- Lua runtime creation/destruction.
- Script load failures and protected-call errors.
- Sandbox restrictions.
- Trigger enter/stay/exit script invocation.
- Deterministic script output ordering.
- Script execution budget failure behavior.
- glTF metadata script binding extraction when glTF support is enabled.
```

---

## 10. Validation Commands

Run after every phase unless a narrower phase-specific run is enough for intermediate development.

### Default build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

### glTF + Lua scripting validation

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

### Narrow scripting validation

```bash
ctest --test-dir build -R 'lua|script|trigger_script|scripted_playable' --output-on-failure
```

### Context tests

Do not enable OpenGL/Vulkan context tests for these phases unless rendering code is touched. Rendering code should not be touched in Phase 10A-10E.

---

## 11. Agent Assignment Guidance

Use existing agent roles only. Do not create new agents.

### `@director`

Owns cross-subsystem sequencing and design compliance.

Use for:

- Final phase scoping.
- Cross-boundary review.
- Deciding whether `Design.md` should add a new section or use `9.5` to avoid renumbering.
- Reviewing server-authority boundaries.

### `@carmack`

Primary implementer for:

- CMake/dependency plumbing.
- `stellar_scripting` target.
- Lua RAII runtime wrapper.
- Scripted session wrapper.
- Server-authoritative integration.
- Display-free runtime tests.

### `@kojima`

Primary implementer/reviewer for:

- Trigger script hook semantics.
- Future entity/object hook contracts.
- Script output event names and gameplay meaning.
- Gameplay-safe script API boundaries.

### `@miyamoto`

No direct Phase 10A-10E implementation unless script output later drives sprite/render presentation. Do not touch rendering in the first scripting slice.

### `@suzuki`

No direct Phase 10A-10E implementation unless script output later drives audio presentation. Do not touch audio in the first scripting slice.

---

## 12. Suggested Agent Prompts

### Phase 10A prompt

```text
@carmack implement Phase 10A from Plans/ProjectStellar-Phase10-LuaScripting-AgentPlan.md.
Add optional Lua scripting support through a new stellar_scripting target. Vendor/pin Lua under
thirdparty/lua, wrap lua_State behind RAII, install a restricted sandbox, support protected script
loading/calls, expose stellar.emit_event with primitive fields only, add an instruction budget, and
add display-free LuaRuntime tests. Do not link Lua into stellar_world or stellar_server_core. Do not
touch rendering/audio/platform windowing code. Update docs/ImplementationStatus.md with Phase 10A
completion notes after validation.
```

### Phase 10B prompt

```text
@carmack implement Phase 10B from Plans/ProjectStellar-Phase10-LuaScripting-AgentPlan.md.
Add optional script binding metadata to WorldMarker, extract stellar.script and stellar.table from
glTF node extras without executing scripts, copy bindings through RuntimeWorld metadata, and add
validation diagnostics/tests. Keep import independent of Lua runtime execution. Update documentation
status after validation.
```

### Phase 10C prompt

```text
@kojima design and implement Phase 10C trigger script semantics with @director coordination as needed.
Add TriggerScriptSystem under stellar_scripting to invoke on_trigger_enter/on_trigger_stay/on_trigger_exit
from authoritative MovementTriggerEvent data. Scripts must emit buffered events only. Add deterministic
display-free tests for hook invocation, missing callbacks, errors, duplicate trigger order, and output
ordering. Do not mutate native gameplay state directly from Lua.
```

### Phase 10D prompt

```text
@carmack implement Phase 10D ScriptedWorldSession. Wrap WorldSession without changing native session
behavior. Load unique script bindings through ScriptRegistry/LuaRuntime, tick the native session,
process trigger scripts from emitted snapshots, and return ScriptedWorldFrame with native snapshot,
script events, and script errors. Add repeatability tests and update ImplementationStatus.md.
```

### Phase 10E prompt

```text
@director coordinate Phase 10E documentation and integration smoke. Update docs/Design.md and
docs/ImplementationStatus.md for lua-scripting. Add a display-free scripted playable-world smoke test
covering glTF metadata script binding, RuntimeWorld copy, ScriptedWorldSession, trigger enter callback,
and deterministic script output. Keep graphics context tests opt-in and untouched.
```

---

## 13. Risk Register and Mitigations

### Risk: Lua leaks into core server/world modules

Mitigation:

- Keep `stellar_scripting` as the only Lua-owning module.
- Link `stellar_scripting` against `stellar_world` and `stellar_server_core`, not the other way around.
- Native tests for `stellar_server_core` must pass with scripting disabled.

### Risk: Scripts bypass server authority

Mitigation:

- Scripts emit buffered events/commands only.
- Native C++ applies or rejects those outputs.
- Do not expose raw state pointers or mutable references.

### Risk: Script hangs server tick

Mitigation:

- Use Lua instruction hooks.
- Add a budget to `LuaRuntimeConfig`.
- Test with an infinite loop script.

### Risk: Sandbox accidentally exposes OS/file APIs

Mitigation:

- Do not open all standard libraries with `luaL_openlibs` unless unsafe libraries are immediately removed and tests prove removal.
- Prefer explicitly opening only allowed libraries.
- Test `io`, `os`, `package`, and `debug` are unavailable.

### Risk: glTF extras parsing becomes too broad

Mitigation:

- Extract only `stellar.script` and `stellar.table` in Phase 10B.
- Preserve raw `extras_json` unchanged.
- Avoid adding broad gameplay semantics to import.

### Risk: Non-deterministic output ordering

Mitigation:

- Process snapshots in existing vector order.
- Process metadata bindings in imported marker order.
- Drain script events at stable points.
- Add repeat-run tests.

### Risk: Constructor error handling becomes awkward

Mitigation:

- Prefer static `create(...) -> std::expected<ScriptedWorldSession, ScriptError>` for fallible session creation.
- Keep constructors for already-valid internal moves only if needed.

---

## 14. Future Phase 11+ Entity/Object Scripting Direction

Do not implement this until ECS/runtime entity ownership is concrete.

Future script hooks may include:

```lua
function on_spawn(entity) end
function on_tick(entity, dt) end
function on_interact(entity, instigator) end
function on_damage(entity, damage) end
function on_destroy(entity) end
```

Future C++ direction:

- Script components should store script id/table plus small serializable state.
- Scripts should read/write only whitelisted component fields through command buffers or validated APIs.
- Runtime entity IDs should be stable and serializable.
- Script state save/load needs explicit design before persistence is promised.
- Client presentation events should remain downstream from server-approved script/native events.

Future systems to consider:

- Script asset loading from a validated asset root.
- Hot reload in development builds only.
- Script-defined archetype defaults.
- Script state serialization.
- Server-owned deterministic RNG exposed as `stellar.random_*`.
- Presentation event bridge for audio/VFX/UI.
- Script debugging/logging hooks without exposing Lua `debug` library to gameplay scripts.

---

## 15. Completion Notes Template

Each implementation phase should end by updating `docs/ImplementationStatus.md` with a completion note like this:

```md
Phase 10X is complete as of YYYY-MM-DD:

- Added ...
- Preserved ...
- Tests cover ...
- Deferred ...

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```
```

If validation could not be run, say exactly why and list the narrow tests that should be run by the next agent.

---

## 16. Final Acceptance Criteria for the Combined Phase 10 Effort

Phase 10 is successful when:

- `lua-scripting` branch documentation identifies Phase 10 as the active direction.
- `stellar_scripting` exists and owns all Lua integration.
- Lua runtime is RAII-managed and protected-call based.
- Restricted sandbox behavior is tested.
- Instruction budget behavior is tested.
- World metadata can carry script bindings from glTF `extras`.
- Import never executes scripts.
- Trigger enter/stay/exit hooks can call Lua.
- Script outputs are buffered and deterministic.
- `ScriptedWorldSession` can wrap native `WorldSession` without changing native behavior.
- Scripted playable-world smoke test passes display-free.
- Default non-scripting tests still pass.
- No rendering/audio/platform-window code is touched unless a later explicit phase scopes presentation integration.
