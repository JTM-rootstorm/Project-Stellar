# Project Stellar — Removable Complexity Cleanup Plan

**Branch target:** `collision-movement`  
**Prepared for:** AI implementation agent / Codex / Kilo-style coding agent  
**Primary goal:** Remove accumulated phase-seam complexity without changing gameplay behavior.  
**Key product decision:** **Lua support is not optional. Treat Lua as core engine infrastructure.**

---

## 0. Agent Operating Contract

You are implementing a cleanup/refactor plan, not a feature expansion.

### Required behavior

- Work on the `collision-movement` branch.
- Preserve existing runtime behavior unless a task explicitly says otherwise.
- Keep changes small, reviewable, and phase-isolated.
- Prefer behavior-preserving refactors over rewrites.
- Run the narrowest relevant tests after each phase, then run full CTest before completion.
- Update `docs/ImplementationStatus.md` after each completed phase.
- Do not add a third-party physics engine.
- Do not make renderer, audio, or client presentation code authoritative for gameplay.
- Do not collapse server authority boundaries.
- Do not remove display-free tests.

### Lua policy

Lua is mandatory. Do not preserve or introduce a build mode where scripting is absent.

The desired end-state is:

- `thirdparty/lua` is always added to the build.
- `stellar_scripting` is always built.
- scripting unit tests are always compiled and registered.
- glTF-dependent scripted integration tests remain gated by `STELLAR_ENABLE_GLTF`, because glTF import is still optional.
- no code path requires `STELLAR_ENABLE_LUA_SCRIPTING`.
- documentation no longer describes Lua as optional.
- server-authoritative Lua sandboxing remains mandatory.

---

## 1. Current Complexity Assessment

The branch appears functionally advanced but has accumulated complexity from safe phase slicing. This is expected and fixable.

### Main removable complexity categories

1. **Stale branch-facing AI guidance**
   - `AGENTS.md` still points agents toward early Phase 6 ordering.
   - `docs/ImplementationStatus.md` describes much later completed work.
   - Old plans in `Plans/Archived/` are useful history, but they should not look like active instructions.

2. **Duplicated low-level geometry/math helpers**
   - Similar vector, finite-check, sanitization, capsule, AABB, and distance code appears in:
     - `src/physics/CollisionWorld.cpp`
     - `src/physics/CharacterController.cpp`
     - `src/world/TriggerSystem.cpp`
     - `src/world/ObjectCollider.cpp`

3. **Duplicated sensor overlap state machines**
   - `TriggerSystem` and `ObjectColliderSystem` both implement:
     - runtime sensor volumes
     - previous/current overlap tracking
     - enter/stay/exit event emission
   - Object colliders add IDs, enabled state, archetype/name data, and mutation exits, but the transition logic is conceptually shared.

4. **Duplicated Lua hook dispatch**
   - `TriggerScriptSystem` and `ObjectColliderScriptSystem` both:
     - map enter/stay/exit phases to hook names
     - build call contexts
     - call Lua table functions
     - collect errors
     - drain script output events

5. **CMake root-file growth**
   - `CMakeLists.txt` has become a large central registry for libraries, executables, tests, optional gates, and integration smokes.
   - Lua is currently gated by `STELLAR_ENABLE_LUA_SCRIPTING`, but Lua should now be core.

6. **Lua configuration ambiguity**
   - `LuaRuntimeConfig` exposes `restricted_sandbox`.
   - If Lua is core server-authoritative gameplay infrastructure, unrestricted standard libraries should not be a normal runtime configuration path.

---

## 2. Non-Goals

Do not do these in this cleanup pass:

- Do not replace the custom collision implementation with Bullet, PhysX, Jolt, Box2D, or another physics engine.
- Do not add rigid bodies.
- Do not add ECS ownership for object colliders unless separately requested.
- Do not add networking.
- Do not add client prediction.
- Do not add new Lua APIs beyond what is required for tests or cleanup.
- Do not introduce templated over-abstractions that make debugging harder.
- Do not merge all collision, trigger, and object-collider concepts into one public gameplay type.
- Do not delete archived plans unless explicitly requested by the user.

---

## 3. Recommended Phase Order

Phases are ordered to reduce risk. Each phase should be a separate commit or PR-sized slice.

Parallel-safe tasks are marked explicitly.

---

# Phase A — Align Documentation and Current Source of Truth

## Goal

Remove stale AI-agent confusion before refactoring code.

## Files likely touched

- `AGENTS.md`
- `docs/ImplementationStatus.md`
- optional new file: `Plans/NEXT.md` or `docs/NextImplementation.md`

## Tasks

1. Update `AGENTS.md` so it does **not** describe old Phase 6A-D work as the current branch plan.
2. Keep `AGENTS.md` focused on stable rules:
   - server authority
   - display-free testing
   - agent roles
   - documentation precedence
   - current status lives in `docs/ImplementationStatus.md`
3. Add or update a short active-current-work document:
   - recommended path: `Plans/NEXT.md`
   - purpose: one-page active handoff for the next implementation agent
4. In `docs/ImplementationStatus.md`, add a small "Cleanup Direction" section:
   - Lua is now mandatory.
   - near-term cleanup is phase-seam removal.
   - no gameplay behavior changes expected.

## Acceptance criteria

- No file tells agents to restart Phase 6A-D as active branch work.
- There is exactly one clear current-work entry point.
- Lua is described as core, not optional.
- Archived plans remain clearly historical.

## Validation

Documentation-only phase:

```bash
git diff -- AGENTS.md docs/ImplementationStatus.md Plans/NEXT.md
```

No build required unless code changes are made.

## Parallelization

Can run in parallel with Phase B only if the code agent and docs agent do not edit the same files.

---

# Phase B — Make Lua Core in Build and Documentation

## Goal

Remove optional Lua complexity.

## Files likely touched

- `CMakeLists.txt`
- `docs/Design.md`
- `docs/ImplementationStatus.md`
- any docs mentioning `STELLAR_ENABLE_LUA_SCRIPTING`
- possibly CI/build instructions if present

## Tasks

1. Remove the `STELLAR_ENABLE_LUA_SCRIPTING` option from `CMakeLists.txt`.
2. Always call:

```cmake
add_subdirectory(thirdparty/lua)
```

3. Always build `stellar_scripting`.
4. Always build and register scripting unit tests:
   - `lua_runtime`
   - `trigger_script`
   - `object_collider_script`
   - `script_command_processor`
   - `scripted_world_session`
5. Keep glTF-dependent scripted smoke tests inside `if(STELLAR_ENABLE_GLTF)`:
   - `scripted_playable_world_smoke`
   - `scripted_collision_smoke`
   - `scripted_object_collider_smoke`
6. Remove nested `if(STELLAR_ENABLE_LUA_SCRIPTING)` blocks.
7. Update validation commands in docs:
   - remove `-DSTELLAR_ENABLE_LUA_SCRIPTING=ON`
   - keep `-DSTELLAR_ENABLE_GLTF=ON` only where glTF importer tests are needed.
8. Search for the macro/option name and remove stale references:

```bash
grep -R "STELLAR_ENABLE_LUA_SCRIPTING" -n .
```

## Important constraints

- Do not remove Lua source, Lua tests, or sandboxing.
- Do not make scripting runtime a dependency of `stellar_world` or `stellar_server_core` unless already required by public APIs.
- Keep Lua linked through `stellar_scripting`.

## Acceptance criteria

- Default configure builds Lua and `stellar_scripting`.
- No `STELLAR_ENABLE_LUA_SCRIPTING` option remains.
- Scripting unit tests exist in default CTest.
- glTF scripted integration tests still require `STELLAR_ENABLE_GLTF=ON`.
- Docs consistently describe Lua as mandatory server-side scripting infrastructure.

## Validation

Minimum:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_lua_runtime_test stellar_trigger_script_system_test stellar_object_collider_script_system_test stellar_script_command_processor_test stellar_scripted_world_session_test -j$(nproc)
ctest --test-dir build -R '^(lua_runtime|trigger_script|object_collider_script|script_command_processor|scripted_world_session)$' --output-on-failure
```

Then:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

With glTF:

```bash
cmake -S . -B build-gltf -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build-gltf -j$(nproc)
ctest --test-dir build-gltf --output-on-failure
```

## Parallelization

Can run in parallel with Phase C if only one agent edits CMake/docs. Coordinate before editing `docs/ImplementationStatus.md`.

---

# Phase C — Extract Minimal Shared Geometry Helpers

## Goal

Remove duplicated low-level math/sanitization code while keeping collision behavior unchanged.

## Files likely added

Choose one of these paths:

- preferred: `include/stellar/math/Geometry3.hpp`
- acceptable: `include/stellar/world/GeometryPrimitives.hpp`

No `.cpp` file is required if helpers are simple inline functions.

## Files likely touched

- `src/physics/CollisionWorld.cpp`
- `src/physics/CharacterController.cpp`
- `src/world/TriggerSystem.cpp`
- `src/world/ObjectCollider.cpp`
- corresponding tests only if behavior accidentally changes

## Helper scope

Extract only boring, low-risk primitives:

```cpp
using Vec3 = std::array<float, 3>;

[[nodiscard]] bool is_finite(float value) noexcept;
[[nodiscard]] bool is_finite(Vec3 value) noexcept;

[[nodiscard]] Vec3 add(Vec3 lhs, Vec3 rhs) noexcept;
[[nodiscard]] Vec3 sub(Vec3 lhs, Vec3 rhs) noexcept;
[[nodiscard]] Vec3 mul(Vec3 value, float scalar) noexcept;
[[nodiscard]] float dot(Vec3 lhs, Vec3 rhs) noexcept;
[[nodiscard]] float length_squared(Vec3 value) noexcept;
[[nodiscard]] Vec3 normalize_or(Vec3 value, Vec3 fallback) noexcept;

[[nodiscard]] float sanitized_radius(float radius) noexcept;
[[nodiscard]] float sanitized_half_extent(float extent) noexcept;
[[nodiscard]] float sanitized_capsule_height(float height, float radius) noexcept;
```

If useful, also extract:

```cpp
struct Segment3 {
    Vec3 start{};
    Vec3 end{};
};

struct Aabb3 {
    Vec3 min{};
    Vec3 max{};
};

[[nodiscard]] float point_aabb_distance_squared(Vec3 point, Aabb3 bounds) noexcept;
[[nodiscard]] float point_segment_distance_squared(Vec3 point, Segment3 segment) noexcept;
```

## Avoid

- Do not move complex sweep/slide logic in this phase.
- Do not change tolerances unless tests prove a bug.
- Do not introduce GLM dependency into these backend-neutral internals.
- Do not expose a large public math framework.
- Do not alter public API data shapes.

## Acceptance criteria

- Duplicate helper definitions are reduced.
- Collision, trigger, and object-collider tests still pass.
- No gameplay behavior changes are expected.
- Any remaining duplicated helpers are intentionally local because they are algorithm-specific.

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_collision_world_test stellar_character_controller_test stellar_trigger_system_test stellar_object_collider_test -j$(nproc)
ctest --test-dir build -R '^(collision_world|character_controller|trigger_system|object_collider)$' --output-on-failure
```

Then full CTest.

## Parallelization

Should not run in parallel with Phase D because both may touch `TriggerSystem` and `ObjectCollider`.

---

# Phase D — Unify Sensor Overlap Transition Tracking

## Goal

Remove duplicated enter/stay/exit bookkeeping while preserving separate public concepts for triggers and object colliders.

## Files likely added

- `include/stellar/world/SensorOverlapTracker.hpp`
- optional: `src/world/SensorOverlapTracker.cpp` if not header-only
- optional test: `tests/world/SensorOverlapTracker.cpp`

## Files likely touched

- `include/stellar/world/TriggerSystem.hpp`
- `src/world/TriggerSystem.cpp`
- `include/stellar/world/ObjectCollider.hpp`
- `src/world/ObjectCollider.cpp`
- `CMakeLists.txt`
- related tests

## Recommended shape

Keep this internal/simple. Example data shape:

```cpp
struct SensorOverlapTransition {
    std::uint32_t id = 0;
    std::string name;
    bool entered = false;
    bool stayed = false;
    bool exited = false;
};

struct SensorOverlapSample {
    std::uint32_t id = 0;
    std::string name;
    bool currently_overlapping = false;
};
```

Then a tracker:

```cpp
class SensorOverlapTracker {
public:
    void reset(std::span<const SensorOverlapSample> samples);
    [[nodiscard]] std::vector<SensorOverlapTransition>
    update(std::span<const SensorOverlapSample> samples);
    [[nodiscard]] std::vector<SensorOverlapTransition>
    remove_or_disable(std::uint32_t id, std::string_view fallback_name) noexcept;
};
```

Alternative designs are acceptable if they are smaller and easier to test.

## TriggerSystem integration

`TriggerSystem` should continue to expose:

- `set_triggers`
- `update_sphere`
- `update_capsule`
- `triggers`

But internally it should build current overlap samples and let the shared tracker emit transitions.

## ObjectColliderSystem integration

`ObjectColliderSystem` should continue to expose existing public mutation APIs:

- `set_colliders`
- `replace_colliders_preserving_overlaps`
- `set_collider_enabled`
- `upsert_collider`
- `remove_collider`
- `update_player_capsule`
- `colliders`
- `diagnostics`

But it should delegate previous/current transition state to the shared tracker.

## Important constraints

- Preserve deterministic ordering.
- Preserve duplicate-id rejection behavior.
- Preserve synchronous exit results when disabling/removing currently overlapped object colliders.
- Preserve trigger name ordering if tests depend on it.
- Do not change public event structs unless necessary.

## Acceptance criteria

- `TriggerSystem` and `ObjectColliderSystem` no longer independently implement the same transition bookkeeping.
- Existing trigger/object-collider tests pass unchanged or with only mechanical expectation updates.
- Disable/remove object-collider exits still happen synchronously.
- Duplicate-id diagnostics remain deterministic.

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_trigger_system_test stellar_object_collider_test stellar_server_world_session_test stellar_scripted_object_collider_smoke_test -j$(nproc)
ctest --test-dir build -R '^(trigger_system|object_collider|server_world_session|scripted_object_collider_smoke)$' --output-on-failure
```

If `STELLAR_ENABLE_GLTF` is required for the smoke:

```bash
cmake -S . -B build-gltf -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build-gltf --target stellar_scripted_object_collider_smoke_test -j$(nproc)
ctest --test-dir build-gltf -R '^scripted_object_collider_smoke$' --output-on-failure
```

## Parallelization

Do not run in parallel with Phase C or Phase E unless file ownership is very carefully split.

---

# Phase E — Unify Lua Hook Dispatch for Sensor Events

## Goal

Remove duplicated Lua enter/stay/exit dispatch loops while keeping trigger and object-collider context fields specific.

## Files likely added

- `include/stellar/scripting/ScriptHookDispatcher.hpp`
- `src/scripting/ScriptHookDispatcher.cpp`
- `tests/scripting/ScriptHookDispatcher.cpp`

## Files likely touched

- `include/stellar/scripting/TriggerScriptSystem.hpp`
- `src/scripting/TriggerScriptSystem.cpp`
- `include/stellar/scripting/ObjectColliderScriptSystem.hpp`
- `src/scripting/ObjectColliderScriptSystem.cpp`
- `CMakeLists.txt`

## Recommended shape

Introduce a small dispatcher that knows how to:

- verify a Lua table exists
- call a table function
- collect `ScriptError`
- drain `ScriptOutputEvent`

Example:

```cpp
struct ScriptHookCall {
    std::string script_id;
    std::string table_name;
    std::string function_name;
    ScriptCallContext context;
};

struct ScriptHookDispatchResult {
    std::vector<ScriptOutputEvent> output_events;
    std::vector<ScriptError> errors;
};

[[nodiscard]] ScriptHookDispatchResult dispatch_script_hooks(
    LuaRuntime& runtime,
    std::span<const ScriptHookCall> calls) noexcept;
```

`TriggerScriptSystem` remains responsible for:

- mapping trigger event phase to:
  - `on_trigger_enter`
  - `on_trigger_stay`
  - `on_trigger_exit`
- building trigger-specific context fields.

`ObjectColliderScriptSystem` remains responsible for:

- mapping object-collider phase to:
  - `on_object_collider_enter`
  - `on_object_collider_stay`
  - `on_object_collider_exit`
- building object-collider-specific context fields.

## Important constraints

- Preserve script callback ordering.
- Preserve output-event ordering.
- Preserve missing table behavior.
- Preserve missing function no-op behavior.
- Preserve existing error messages unless tests require a mechanical update.
- Do not merge trigger scripts and object-collider scripts into one public class.

## Acceptance criteria

- Duplicate call/drain/error dispatch loops are removed.
- Trigger and object-collider script tests still pass.
- Scripted world session tests still pass.
- Integration smoke tests still pass under glTF.

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_trigger_script_system_test stellar_object_collider_script_system_test stellar_scripted_world_session_test -j$(nproc)
ctest --test-dir build -R '^(trigger_script|object_collider_script|scripted_world_session)$' --output-on-failure
```

With glTF:

```bash
cmake -S . -B build-gltf -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build-gltf --target stellar_scripted_playable_world_smoke_test stellar_scripted_collision_smoke_test stellar_scripted_object_collider_smoke_test -j$(nproc)
ctest --test-dir build-gltf -R '^(scripted_playable_world_smoke|scripted_collision_smoke|scripted_object_collider_smoke)$' --output-on-failure
```

## Parallelization

Can run after Phase B. Avoid parallel edits to `ScriptedWorldSession.cpp` or scripting CMake sections.

---

# Phase F — Simplify Lua Runtime Policy Surface

## Goal

Make Lua mandatory and server-safe by default without a normal unrestricted standard-library mode.

## Files likely touched

- `include/stellar/scripting/LuaRuntime.hpp`
- `src/scripting/LuaRuntime.cpp`
- `src/scripting/ScriptSandbox.cpp`
- `tests/scripting/LuaRuntime.cpp`
- docs mentioning Lua sandbox config

## Tasks

1. Remove or deprecate `LuaRuntimeConfig::restricted_sandbox`.
2. Always install the restricted sandbox.
3. Keep `instruction_budget`.
4. Preserve bytecode rejection.
5. Preserve `stellar.emit_event`.
6. Preserve protected calls.
7. Preserve deterministic output event ordering.
8. Update tests to assert restricted sandbox is always active.

## Decision point

If a development-only unrestricted mode is desired later, do not expose it as normal runtime config. It should require an explicit future design decision, likely a compile-time development flag or standalone tooling runtime.

## Acceptance criteria

- Runtime users cannot accidentally create an unrestricted gameplay Lua state.
- Lua remains mandatory, but sandboxed.
- Lua runtime tests pass.
- Documentation describes scripting as mandatory and sandboxed.

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_lua_runtime_test stellar_scripted_world_session_test -j$(nproc)
ctest --test-dir build -R '^(lua_runtime|scripted_world_session)$' --output-on-failure
```

## Parallelization

Can run after Phase B. Avoid parallel edits with Phase E if both touch Lua runtime tests.

---

# Phase G — Reduce Root CMake Boilerplate

## Goal

Make future target/test additions less error-prone without changing build behavior.

## Files likely touched

- `CMakeLists.txt`
- optional: `cmake/StellarTargets.cmake`
- optional: subsystem `CMakeLists.txt` files if the project wants directory-level split

## Preferred minimal path

Add small helper functions/macros near the top of root CMake or in `cmake/StellarTargets.cmake`:

```cmake
function(stellar_add_test_executable target_name test_name)
    add_executable(${target_name} ${ARGN})
    target_include_directories(${target_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME ${test_name} COMMAND ${target_name})
endfunction()
```

Then use more specific helpers only where helpful:

```cmake
stellar_add_world_test(stellar_trigger_system_test trigger_system tests/world/TriggerSystem.cpp)
stellar_add_scripting_test(stellar_lua_runtime_test lua_runtime tests/scripting/LuaRuntime.cpp)
```

## Alternative path

Split by subsystem:

- `src/physics/CMakeLists.txt`
- `src/world/CMakeLists.txt`
- `src/server/CMakeLists.txt`
- `src/scripting/CMakeLists.txt`
- `tests/CMakeLists.txt`

This is a larger diff. Prefer helper functions unless the root file remains hard to read after Lua gate removal.

## Important constraints

- Do not change target names unless necessary.
- Do not change CTest names unless necessary.
- Do not change default optional graphics context test behavior.
- Keep glTF optional.
- Keep Lua mandatory.
- Keep existing link boundaries.

## Acceptance criteria

- Root CMake is shorter or easier to scan.
- Adding one new display-free unit test no longer requires repeated boilerplate.
- All targets and test names still exist.
- Default and glTF-enabled builds pass.

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

cmake -S . -B build-gltf -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build-gltf -j$(nproc)
ctest --test-dir build-gltf --output-on-failure
```

## Parallelization

Run after Phase B. Do not run in parallel with phases adding/removing CMake targets.

---

# Phase H — Final Integration and Cleanup Review

## Goal

Prove the cleanup removed complexity without behavior regressions.

## Tasks

1. Run repo-wide stale-option search:

```bash
grep -R "STELLAR_ENABLE_LUA_SCRIPTING" -n .
grep -R "restricted_sandbox" -n .
```

Expected:
- no `STELLAR_ENABLE_LUA_SCRIPTING`
- no normal runtime `restricted_sandbox` config if Phase F completed

2. Run duplicate helper scan manually:
   - vector math helpers
   - capsule segment construction
   - point/AABB distance
   - enter/stay/exit transition loops
   - Lua call/drain/error loops

3. Update `docs/ImplementationStatus.md` with:
   - completed cleanup phases
   - validation commands and results
   - intentional remaining duplication, if any

4. Run full validation.

## Full validation

Default:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

glTF-enabled:

```bash
cmake -S . -B build-gltf -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build-gltf -j$(nproc)
ctest --test-dir build-gltf --output-on-failure
```

Optional graphics context tests remain opt-in:

```bash
cmake -S . -B build-graphics-context -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_OPENGL_CONTEXT_TESTS=ON \
  -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON
cmake --build build-graphics-context -j$(nproc)
ctest --test-dir build-graphics-context --output-on-failure
```

## Acceptance criteria

- Default build includes Lua and scripting tests.
- glTF-enabled build includes scripted glTF smoke tests.
- Full CTest passes in default and glTF-enabled configurations.
- Docs no longer describe Lua as optional.
- Current-work guidance no longer sends agents into archived/stale phase plans.
- Shared helper extraction reduced code duplication without obscuring algorithms.
- Trigger/object-collider semantics remain separate at public API level.
- Lua hook dispatch is shared internally but context-specific at system level.

---

## 4. Risk Register

### Risk: Over-abstracting collision math

Mitigation:
- Extract only small, boring helpers first.
- Leave sweep/slide and BVH logic local unless duplication is obvious and tests are strong.

### Risk: Changing deterministic event order

Mitigation:
- Preserve sorted trigger behavior.
- Preserve object-collider authored marker/id order.
- Add explicit ordering assertions if missing.

### Risk: Breaking scripted integration when Lua becomes mandatory

Mitigation:
- First remove only CMake gates.
- Keep link boundaries.
- Run default and glTF-enabled builds.

### Risk: Sandbox cleanup breaks tests expecting configurable unrestricted mode

Mitigation:
- Update tests to assert gameplay Lua is always sandboxed.
- Preserve only instruction budget configurability.

### Risk: Documentation churn becomes noisy

Mitigation:
- Keep `AGENTS.md` stable and branch-agnostic.
- Put active branch details in `docs/ImplementationStatus.md` and optionally `Plans/NEXT.md`.

---

## 5. Suggested Commit / PR Slices

1. `docs: align collision-movement cleanup guidance`
2. `build: make Lua scripting core infrastructure`
3. `world: extract shared geometry primitives`
4. `world: share sensor overlap transition tracking`
5. `scripting: share Lua hook dispatch`
6. `scripting: enforce sandboxed Lua runtime policy`
7. `build: reduce target and test boilerplate`
8. `docs: record removable-complexity cleanup completion`

---

## 6. Agent Checklist Before Marking Complete

- [ ] Lua is mandatory in default build.
- [ ] No `STELLAR_ENABLE_LUA_SCRIPTING` option remains.
- [ ] `stellar_scripting` always builds.
- [ ] Scripting tests always register in default CTest.
- [ ] glTF scripted smokes remain glTF-gated only.
- [ ] `AGENTS.md` no longer points to stale active Phase 6 work.
- [ ] Geometry helper duplication is reduced.
- [ ] Trigger and object-collider transition tracking duplication is reduced.
- [ ] Trigger and object-collider Lua dispatch duplication is reduced.
- [ ] Sandbox policy is clear and mandatory.
- [ ] Default full CTest passes.
- [ ] glTF-enabled full CTest passes.
- [ ] `docs/ImplementationStatus.md` records completed cleanup and validation.

---

## 7. Stop Conditions

Stop and report instead of guessing if:

- A refactor changes observable movement/collision behavior.
- Event ordering changes in a way not covered by existing tests.
- Lua mandatory build creates an unavoidable platform dependency problem.
- CMake target splitting causes target name or CTest name churn.
- A proposed shared abstraction makes public APIs less clear.
- A test failure indicates an existing hidden behavior dependency.

Report with:
- failed phase
- files touched
- exact command run
- failure output summary
- suspected cause
- smallest recommended next step

