# Phase PN-1 — Live Scripted Authoritative Runtime Integration

Optimized for Kilo Code intake.

## Owner

- Primary: `@carmack`
- Coordinator/reviewer: `@director`
- Escalate to `@director` if script source root policy becomes product/design ambiguous.

## Goal

Make the live client use the server-authoritative scripted runtime when the loaded BSP map contains trigger or object-collider script bindings.

The existing scripted gameplay path is tested, but live client startup currently constructs a plain `LocalLoopbackRuntime` over `server::WorldSession`. This phase closes that gap.

## Required reading

- `AGENTS.md`
- `.kilo/agents/director.md`
- `docs/ImplementationStatus.md`
- `docs/Design.md`, especially server-authoritative scripting and networking sections
- `docs/BspAuthoring.md`
- `include/stellar/client/Application.hpp`
- `include/stellar/client/ApplicationConfig.hpp`
- `src/client/Application.cpp`
- `src/client/ApplicationConfig.cpp`
- `include/stellar/client/LocalLoopbackRuntime.hpp`
- `src/client/LocalLoopbackRuntime.cpp`
- `include/stellar/scripting/ScriptedWorldSession.hpp`
- `src/scripting/ScriptedWorldSession.cpp`
- `include/stellar/scripting/ScriptRegistry.hpp`
- `include/stellar/server/WorldSession.hpp`
- `include/stellar/server/GameplayWorld.hpp`

## Current implementation anchors

- `PreparedApplicationRuntime` owns `ApplicationValidation`, `RuntimeWorld`, and an optional `LocalLoopbackRuntime`.
- `prepare_application_runtime()` loads/validates `.bsp`, builds `RuntimeWorld`, then constructs `LocalLoopbackRuntime`.
- `LocalLoopbackRuntime` owns `server::WorldSession` directly.
- `ScriptedWorldSession::create()` loads scripts from a `ScriptRegistry`, wraps `WorldSession`, ticks movement first, then trigger and object-collider scripts, then applies validated native commands.
- `ScriptRegistry` is currently an in-memory map.

## Scope

Implement a client-side local authoritative runtime that can tick either:

1. Plain `server::WorldSession` for maps with no script bindings.
2. `scripting::ScriptedWorldSession` for maps with trigger/object-collider script bindings.

Do not add remote networking in this phase. Do not add client-side script execution.

## Recommended design

### 1. Preserve public client loop shape

Keep a single client-facing object that offers the current `LocalLoopbackRuntime` behavior:

```cpp
latest_snapshot()
update(input, delta_seconds)
```

Internally, it can use one of these designs:

Option A, preferred for minimal churn:

```cpp
class LocalLoopbackRuntime {
    std::variant<server::WorldSession, scripting::ScriptedWorldSession> session_;
    ...
};
```

Option B, acceptable if cleaner:

```cpp
class ILocalAuthoritativeSession {
public:
    virtual ~ILocalAuthoritativeSession() = default;
    virtual server::WorldSnapshot latest_snapshot() const = 0;
    virtual ScriptAwareTickResult tick(...) noexcept = 0;
};
```

Do not leak Lua types into rendering or platform layers.

### 2. Add script-source loading for client runtime preparation

The current `ScriptRegistry` requires in-memory source strings. Add a small helper to populate it from loaded BSP metadata.

Recommended helper location:

```text
include/stellar/client/ScriptRegistryLoader.hpp
src/client/ScriptRegistryLoader.cpp
```

or, if that creates an unwanted client/scripting dependency, place under:

```text
include/stellar/scripting/ScriptRegistryLoader.hpp
src/scripting/ScriptRegistryLoader.cpp
```

Suggested API:

```cpp
struct ScriptRegistryLoadConfig {
    std::filesystem::path map_path;
    std::optional<std::filesystem::path> script_root;
};

struct ScriptRegistryLoadResult {
    ScriptRegistry registry;
    std::vector<std::string> loaded_script_ids;
    std::vector<ScriptError> errors;
};

std::expected<ScriptRegistryLoadResult, platform::Error>
load_script_registry_for_world(const world::RuntimeWorld& world,
                               const ScriptRegistryLoadConfig& config);
```

Policy:

- Only load script ids referenced by trigger/object-collider metadata.
- Reuse the existing import restriction that rejects absolute paths and parent escapes.
- Resolve asset-relative ids against:
  1. Explicit `ApplicationConfig::script_root`, if added.
  2. Otherwise `parent_path(config.map_path)`.
- Missing script source is a startup/runtime preparation error for maps that reference scripts.
- Empty/no-script maps still use plain runtime and should not require a script root.
- Do not allow script ids to read outside the root after normalization.

### 3. Extend `ApplicationConfig` carefully

Add only what is needed:

```cpp
std::optional<std::string> script_root;
```

Optional CLI addition if existing parser supports it:

```text
--script-root path
```

If adding CLI support is too broad, use map directory only in this phase and document explicit script root as deferred.

### 4. Construct scripted runtime when needed

In `prepare_application_runtime()`:

1. Load BSP and build `RuntimeWorld`.
2. Detect whether `RuntimeWorld` has any trigger/object-collider script bindings.
3. If no script bindings:
   - Construct plain `LocalLoopbackRuntime`.
4. If script bindings exist:
   - Build `ScriptRegistry`.
   - Construct `ScriptedWorldSession` through `ScriptedWorldSession::create`.
   - Store script diagnostics in prepared runtime validation if useful.
   - Construct local runtime with scripted session mode.

Suggested new validation fields:

```cpp
std::vector<stellar::scripting::ScriptError> script_errors;
std::vector<std::string> loaded_script_ids;
bool scripted_runtime_enabled = false;
```

Add to `ApplicationValidation` or a runtime-preparation-specific diagnostics object.

### 5. Surface script command results and errors safely

`ScriptedWorldSession::tick()` returns script events, script errors, and command results. The live client runtime should preserve this information in a frame result.

Extend `LocalLoopbackFrameResult`:

```cpp
std::vector<scripting::ScriptOutputEvent> script_events;
std::vector<scripting::ScriptError> script_errors;
std::vector<scripting::ScriptCommandResult> command_results;
bool scripted = false;
```

The live client can ignore these initially, but tests and later presentation phases need them.

### 6. Keep object-collider mutation APIs working

If `LocalLoopbackRuntime` currently exposes object-collider mutation APIs, preserve them. For scripted mode, route mutations to the underlying `WorldSession` if available.

If `ScriptedWorldSession` does not expose those mutators, add narrow forwarding methods to it rather than duplicating state.

Required forwards may include:

```cpp
set_object_colliders
replace_object_colliders_preserving_overlaps
set_object_collider_enabled
upsert_object_collider
remove_object_collider
```

Only add methods that current client/tests need.

## Tests

Add or update display-free tests.

Recommended new tests:

```text
tests/client/LocalLoopbackRuntime.cpp
tests/client/ApplicationRuntimeScripted.cpp
tests/scripting/ScriptedWorldSession.cpp
tests/integration/BspPlayableWorldSmoke.cpp
```

Cases:

1. Plain map without scripts still constructs plain runtime and passes existing movement tests.
2. Scripted BSP fixture constructs scripted local runtime.
3. Missing script source for referenced script id fails `prepare_application_runtime()` with deterministic error.
4. Script path escape remains rejected before runtime preparation.
5. Scripted runtime frame collects pickup once and marks gameplay entity inactive.
6. Scripted trigger frame toggles named gate collision and updates door/gate `open` state in `gameplay_world`.
7. Runtime accumulates script errors without crashing when a Lua callback fails.
8. `validate_only` remains display-free and does not create a window or renderer.

## Validation commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar-client -j$(nproc)
cmake --build build --target \
  stellar_client_local_loopback_runtime_test \
  stellar_client_map_validation_smoke \
  stellar_scripted_world_session_test \
  stellar_script_command_processor_test \
  stellar_bsp_playable_world_smoke_test \
  -j$(nproc)

ctest --test-dir build -R '^(client_local_loopback_runtime|client_map_validation_smoke|client_cli_map_validation|scripted_world_session|script_command_processor|bsp_playable_world_smoke|bsp_scripted_playable_world_smoke|bsp_scripted_collision_smoke|bsp_scripted_object_collider_smoke)$' --output-on-failure
```

Then run full tests if focused validation passes:

```bash
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- Existing no-script BSP client path still works.
- Maps with trigger/object-collider script bindings construct a scripted authoritative runtime.
- Live client frame updates can produce script events, errors, and command results.
- Pickup and gate/door interactions in scripted mode update authoritative snapshots.
- No client-side gameplay scripting added.
- Default tests remain display-free.
- Docs mention live client uses authoritative server-side script runtime for script-bound maps.
