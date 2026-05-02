# Phase 2 — Z-up Runtime, Collision, Movement, Scripting Metadata, and Fixtures

Branch target: `trenchbroom-compat`

## Goal

Complete the runtime-side Y-up to Z-up conversion for gameplay simulation, collision, runtime world data, script-facing metadata, generated BSP fixtures, and validation tests.

## Required behavior

- Z is the vertical axis everywhere in active gameplay/runtime code.
- Gravity applies along -Z.
- Ground probing and walkable normals compare against +Z.
- Character capsule axis is +Z.
- Player start center is `z = 36` for a 72-inch capsule when floor is `z = 0`.
- Runtime bounds and extents continue to use `"x y z"` ordering, with Z as vertical.
- Triggers and object-collider extents remain author-authored values; do not silently swap axes during import.

## Tasks

### 2.1 Movement and physics conversion

Audit and update:

- `stellar::physics::CharacterController`
- collision query helpers
- ground probe logic
- sweep-and-slide code
- step-up code
- slope checks
- gravity integration
- terminal fall behavior
- `WorldSession` or equivalent authoritative movement state

Search patterns:

```bash
git grep -n 'gravity\|ground\|slope\|step_height\|snap\|up' -- include/stellar src tests ':!build*/**'
git grep -n 'position\[1\]\|velocity\[1\]\|displacement\[1\]' -- include src tests ':!build*/**'
git grep -n '0\.0F, 1\.0F, 0\.0F\|0, 1, 0' -- include src tests ':!build*/**'
```

Expected changes:

- vertical position component becomes index 2 where code is truly vertical;
- horizontal movement uses X/Y plane;
- ground normals compare to +Z;
- gravity mutates Z velocity;
- step-up attempts lift along +Z and snap along -Z.

Do not blindly replace all index-1 accesses; some may be legitimate forward/Y horizontal motion after conversion.

### 2.2 Runtime world and metadata conversion

Audit and update:

- runtime marker creation;
- spawn selection;
- trigger bounds;
- object-collider sensor bounds;
- named static collision bounds;
- script event payload coordinate examples/tests;
- `stellar.extents` parsing tests;
- collision mesh bounds tests.

Rules:

- Keep entity key parsing order `"x y z"`.
- Treat third value as vertical once the branch is Z-up.
- Do not convert imported BSP coordinates from Z-up to Y-up.
- Do not add a legacy Y-up map compatibility mode unless separately requested.

### 2.3 Generated BSP/test fixture conversion

Update generated or checked-in fixtures so gameplay rooms become Z-up:

Old Y-up pattern:

```text
floor at y = 0
ceiling at y = 96
x/z horizontal footprint
player origin = "0 36 0"
```

New Z-up pattern:

```text
floor at z = 0
ceiling at z = 96
x/y horizontal footprint
player origin = "0 0 36"
```

Update generated collision triangles, face normals, bounds, and expected import data accordingly.

Targets to inspect:

```bash
git grep -n 'gameplay_room\|0 36 0\|floor at y\|ceiling at y\|y = 96\|x/z' -- tests src docs tools ':!build*/**'
```

### 2.4 Script metadata and Lua-facing event tests

Update script event tests and docs so event positions are Z-up.

Rules:

- Existing Lua sandbox/server-authority rules remain unchanged.
- Import still only records script metadata.
- Runtime still loads Lua only on authoritative runtime side.
- Script ids remain asset-relative.
- Axis migration must not give Lua direct access to engine internals.

### 2.5 Collision enable/disable and named blockers

Ensure named `func_wall`, `func_door`, and `func_button` static collision metadata still works after axis migration.

- Closed gate blocks in X/Y plane.
- Open gate disables named static mesh.
- Door/gate visual/presentation metadata reflects server-owned state.
- No moving brush simulation is added.

## Files likely changed

- `src/physics/**`
- `include/stellar/physics/**`
- `src/server/**`
- `include/stellar/server/**`
- `src/world/**`
- `include/stellar/world/**`
- `src/assets/bsp/**`
- `tests/physics/**`
- `tests/server/**`
- `tests/world/**`
- `tests/assets/**`
- `tests/fixtures/**`

Exact paths may differ; use grep and CMake target names to locate current files.

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(character_controller|collision_validation|runtime_world|server_world_session|scripted_world_session|trigger_script|object_collider_script|script_command_processor|bsp_playable_world_smoke|bsp_scripted)' --output-on-failure
```

Broad:

```bash
ctest --test-dir build --output-on-failure
```

Audit:

```bash
git grep -n '0 36 0\|floor at y = 0\|floor at `y = 0`\|ceiling at y = 96\|Y is up\|Y-up' -- docs include src tests tools Plans ':!Plans/Archived/**' ':!build*/**'
git grep -n '0\.0F, 1\.0F, 0\.0F\|0, 1, 0' -- include src tests ':!build*/**'
```

Remaining hits must be either:
- intentionally historical/archived;
- unrelated horizontal +Y forward behavior;
- documented as such in code comments.

## Acceptance criteria

- [ ] Character movement uses +Z up and -Z gravity.
- [ ] Player spawn center policy uses `z = 36`.
- [ ] Gameplay room fixtures use X/Y horizontal footprint and Z vertical height.
- [ ] Trigger/object-collider bounds are correct under Z-up.
- [ ] Scripted pickup and door/gate tests still pass.
- [ ] No hidden Y-up compatibility conversion is introduced.
- [ ] Full CTest passes or failures are only in Phase 3 presentation code scheduled next.

## Parallelization

Potential parallel workstreams:

- Agent A: physics and character controller.
- Agent B: runtime world metadata and server session.
- Agent C: fixture generation and tests.
- Agent D: Lua/script event tests.

Coordinate through the central axis constants from Phase 1. Avoid merging fixture expected-value changes before the corresponding code path changes.
