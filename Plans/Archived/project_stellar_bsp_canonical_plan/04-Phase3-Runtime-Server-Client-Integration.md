# Phase 3 — RuntimeWorld, Server, Scripting, and Client Startup Integration

Branch target: `bsp-integration`  
Primary agent: `@carmack`  
Gameplay metadata support: `@kojima` through `@director`  
Dependencies: Phase 1 and Phase 2 complete  
Parallelizable: limited; tests can be updated in parallel by subsystem after API changes land.

---

## Goal

Make BSP-backed `LevelAsset` drive the actual runtime world and server-authoritative gameplay seams.

At the end of this phase, a BSP map should be usable for display-free playable-world smoke tests: import → validate → build runtime world → spawn player → collide/move → fire triggers/object-collider Lua hooks.

---

## RuntimeWorld changes

Update `include/stellar/world/RuntimeWorld.hpp` and implementation:

- Store pointer/reference to caller-owned `assets::LevelAsset`, not `retired scene asset`.
- Build `physics::CollisionWorld` from `LevelAsset::level_collision`.
- Copy `LevelAsset::world_metadata` into runtime metadata.
- Preserve diagnostics:
  - `has_collision`,
  - collision mesh count,
  - collision triangle count,
  - marker count,
  - sprite marker count,
  - object collider marker count,
  - player spawn availability,
  - warnings.
- Add BSP-specific diagnostics only if they stay source-neutral or are clearly under an import diagnostic type.

Existing helper APIs should continue to work:

- `find_player_spawn`
- `find_markers_by_type`
- `find_spawn_by_archetype`
- `find_sprite_markers`
- `find_trigger_markers`

Add if missing:

- `find_object_collider_markers`

---

## Validation updates

### Collision validation

Update `validate_level_collision` docs/tests to say collision can come from BSP or other level sources. Remove retired model importer wording.

Add coverage for:

- `worldspawn` collision mesh,
- named brush entity collision mesh,
- duplicate named collision mesh behavior,
- empty collision map diagnostics,
- BSP import collision report if diagnostic type exists.

### World metadata validation

Update validation to understand BSP entity properties:

- script bindings on triggers and object colliders are supported,
- script bindings on sprites/spawns remain unsupported unless explicitly scoped,
- duplicate trigger/object-collider names remain deterministic diagnostics,
- object collider extents from brush model bounds are finite/nonzero.

---

## Server/session integration

Existing server APIs should require minimal logic changes if Phase 1 was done cleanly.

Verify and preserve:

- `WorldSession` initializes player from BSP `info_player_start` metadata.
- `MovementSimulation` uses BSP-backed `CollisionWorld` for authoritative movement.
- `RuntimeCollisionState` initializes from BSP collision mesh names.
- `collision.set_mesh_enabled` scripts can toggle named BSP collision meshes.
- Triggers are produced from BSP entity metadata and fire after authoritative movement.
- Object collider sensors are produced from BSP metadata and fire enter/stay/exit events.
- `object_collider.set_enabled` still validates by runtime id.

Add a display-free smoke test:

`tests/integration/BspPlayableWorldSmoke.cpp`

Fixture should include:

- minimal BSP geometry with floor/wall collision,
- `info_player_start`,
- trigger entity with script binding,
- object-collider sensor entity with script binding,
- sprite marker entity if metadata supports it.

Test path:

1. Load BSP fixture through `stellar_import_bsp`.
2. Validate collision and metadata.
3. Build `RuntimeWorld`.
4. Build `ScriptedWorldSession` with in-memory Lua scripts.
5. Tick movement into trigger/object collider.
6. Assert script outputs and native command validation.
7. Assert no display/GPU/window needed.

---

## Client startup validation

Update `ApplicationConfig` to use map terminology.

Recommended public changes:

```cpp
struct ApplicationConfig {
    std::optional<std::string> map_path;
    graphics::GraphicsBackend graphics_backend = graphics::GraphicsBackend::kOpenGL;
    bool validate_only = false;
};

struct ApplicationValidation {
    std::optional<assets::LevelAsset> level;
    std::optional<world::RuntimeWorldDiagnostics> runtime_world_diagnostics;
};
```

Remove animation-specific config fields unless another non-retired model importer system still uses them.

Update CLI behavior:

- Replace `--asset <path>` with `--map <path>`.
- `--validate-config --map path/to/map.bsp` loads BSP and validates level/runtime diagnostics.
- Error clearly if map extension is unsupported or BSP parse fails.
- No `retired importer feature gate` error path remains.

---

## Tests to update/remove

Replace retired model importer integration smoke tests with BSP equivalents:

- `playable_world_smoke` → BSP-backed smoke.
- `scripted_playable_world_smoke` → BSP-backed smoke.
- `scripted_collision_smoke` → BSP-backed collision toggle smoke.
- `scripted_object_collider_smoke` → BSP-backed object collider smoke.

Keep test names if useful, but their fixture/import path must be BSP.

---

## Acceptance criteria

- Runtime world assembly is BSP/LevelAsset-backed.
- Server movement/collision/triggers/object colliders/scripts work from BSP import output.
- Client validation accepts BSP maps without feature gates.
- The old retired model importer `--asset requires retired importer feature gate=ON` error path is gone.
- Display-free integration smoke covers the BSP playable path.

---

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_|playable_world_smoke|scripted_playable_world_smoke|scripted_collision_smoke|scripted_object_collider_smoke|server_world_session|scripted_world_session|client_map_validation_smoke|client_cli_map_validation)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

After CLI rename, update test names to `client_map_validation_smoke` or similar.

---

## Completion note template

Add to `docs/ImplementationStatus.md`:

```markdown
Phase BSP-3 is complete as of YYYY-MM-DD:

- Runtime world assembly now consumes BSP-backed `LevelAsset` data.
- Server movement, static collision filters, triggers, object-collider sensors, and Lua script hooks work from BSP metadata.
- Client startup validation now uses BSP map loading and map terminology.
- Added display-free BSP playable-world/scripted integration smoke coverage.

Validation run:

<commands and result>
```

---

## Completion note

Phase BSP-3 is complete as of 2026-05-01.

- Runtime world assembly consumes BSP-backed source-neutral `LevelAsset` data and exposes object
  collider marker lookup.
- Server movement, static collision filters, triggers, object-collider sensors, and Lua hooks are
  covered through generated BSP fixtures and authoritative scripted-session ticks.
- Client validation uses `--map`/`map_path`, loads `.bsp` maps through `stellar_import_bsp`, and no
  longer uses the old retired model importer feature-gate validation error path.
- Display-free BSP playable/scripted smokes, client map validation, and CLI map validation were added
  or renamed in CMake.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_|playable_world_smoke|scripted_playable_world_smoke|scripted_collision_smoke|scripted_object_collider_smoke|server_world_session|scripted_world_session|client_map_validation_smoke|client_cli_map_validation)$' --output-on-failure
ctest --test-dir build -R 'bsp_|playable_world_smoke|scripted_playable_world_smoke|scripted_collision_smoke|scripted_object_collider_smoke|server_world_session|scripted_world_session|client_map_validation_smoke|client_cli_map_validation' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure/build succeeded; phase-plan targeted CTest passed 9/9 selected tests, corrected
targeted CTest passed 11/11 including BSP importer and BSP playable smoke, and full CTest passed
33/33.
