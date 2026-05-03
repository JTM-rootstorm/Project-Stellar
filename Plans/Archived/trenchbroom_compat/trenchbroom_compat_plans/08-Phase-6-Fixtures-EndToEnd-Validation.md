# Phase 6 — Fixtures and End-to-End Validation

Branch target: `trenchbroom-compat`

## Goal

Prove that the branch can load and validate BSP30 maps that represent the intended TrenchBroom workflow.

## Required fixture coverage

At minimum:

1. `minimal_zup_room`
   - 192 × 192 × 96 X/Y/Z room.
   - floor at `z = 0`.
   - ceiling at `z = 96`.
   - `info_player_start origin = "0 0 36"`.
   - developer grid/wall materials.

2. `entity_matrix_zup`
   - `info_player_start`
   - `info_stellar_spawn`
   - `trigger_stellar`
   - `trigger_multiple`
   - `trigger_once`
   - `stellar_sprite`
   - `env_sprite`
   - `stellar_object_collider`
   - `func_wall`
   - `func_door`
   - `func_button`

3. `scripted_interaction_zup`
   - one trigger script binding;
   - one object-collider pickup script binding;
   - one named static gate/door blocker;
   - valid asset-relative script paths.

4. `invalid_script_escape_zup`
   - script path escape case expected to fail validation.

## Fixture strategy

Preferred:

```text
tests/fixtures/trenchbroom/src/*.map
tests/fixtures/trenchbroom/compiled/*.bsp
tests/fixtures/trenchbroom/scripts/*.lua
```

If checking in compiled BSPs is undesirable, provide deterministic generated BSP30 fixtures and keep `.map` sources as human/editor references. However, at least one fixture should exercise the exact key names emitted by the TrenchBroom FGD/wrapper.

## Tasks

### 6.1 Build fixtures

Create or update the fixture maps using Z-up policy:

- horizontal plane X/Y;
- vertical Z;
- player start at `"0 0 36"`;
- brush entities use BSP submodels;
- alias keys are `_stellar_*` or dotted keys exactly as selected in Phase 5.

### 6.2 Add fixture documentation

For each fixture, add a short README:

```text
tests/fixtures/trenchbroom/README.md
```

Include:

- source `.map` file;
- compiler/profile used;
- expected BSP version;
- expected entities;
- expected validation outcome.

### 6.3 Add CTest coverage

Add tests for:

- client map validation of `minimal_zup_room`;
- server validate-config of `minimal_zup_room`;
- importer metadata extraction of `entity_matrix_zup`;
- scripted runtime validation of `scripted_interaction_zup`;
- expected failure of `invalid_script_escape_zup`;
- compile wrapper smoke if compiler is available.

External compiler tests should be opt-in or skipped gracefully when the compiler is absent, unless the toolchain is vendored.

### 6.4 Manual/editor validation checklist

Document a manual path:

1. Open TrenchBroom.
2. Select Stellar game config.
3. Open `minimal_zup_room.map`.
4. Confirm grid and wall textures are visible/selectable.
5. Confirm entity classes appear with smart properties.
6. Compile with BSP30 profile.
7. Run validation wrapper.
8. Launch local client with `stellar-client --map <compiled.bsp>`.

### 6.5 Acceptance launch

Run:

```bash
stellar-client --validate-map tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp
stellar-server --validate-config --map tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp
```

If a display/GPU run is available, optionally run:

```bash
stellar-client --map tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp
```

Do not make display/GPU validation mandatory in default CI.

## Files likely changed

- `tests/fixtures/trenchbroom/**`
- `tests/assets/**`
- `tests/client/**`
- `tests/server/**`
- `tests/world/**`
- `CMakeLists.txt`
- `docs/TrenchBroom.md`
- `docs/BspAuthoring.md`

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_authoring_smoke|bsp_validation|bsp_importer|client_map_validation_smoke|client_cli_map_validation|dedicated_server|runtime_world|server_world_session|scripted_world_session)' --output-on-failure
```

Broad:

```bash
ctest --test-dir build --output-on-failure
```

Manual:

```bash
tools/bsp/validate_trenchbroom_bsp30.sh tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp
```

## Acceptance criteria

- [ ] BSP30 Z-up minimal room validates.
- [ ] Entity matrix validates and metadata matches expected classes/keys.
- [ ] Scripted interaction fixture validates and runs through authoritative runtime tests.
- [ ] Invalid script path fixture fails deterministically.
- [ ] CTest covers fixtures display-free.
- [ ] Manual TrenchBroom open/compile/validate checklist is documented.

## Parallelization

Safe workstreams:

- Agent A: minimal room fixture.
- Agent B: entity matrix fixture.
- Agent C: scripted fixtures and Lua scripts.
- Agent D: CTest integration.
- Agent E: manual docs.

Coordinate on FGD key naming selected in Phase 5.
