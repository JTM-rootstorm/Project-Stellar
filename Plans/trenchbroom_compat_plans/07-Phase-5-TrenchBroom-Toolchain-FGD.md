# Phase 5 — TrenchBroom Toolchain, FGD, Materials, and BSP30 Compile Wrappers

Branch target: `trenchbroom-compat`

## Goal

Add the project-owned TrenchBroom integration package and make the editor-to-BSP30-to-validation workflow reproducible.

## Required deliverables

```text
tools/trenchbroom/Stellar/
  GameConfig.cfg
  CompilationProfiles.cfg
  README.md
  stellar_entities.fgd
  materials/ or textures/ as needed

tools/bsp/
  compile_trenchbroom_bsp30.sh
  validate_trenchbroom_bsp30.sh
```

Exact filenames may differ, but the workflow must be documented and scriptable.

## Tasks

### 5.1 Create TrenchBroom game profile

Add a Stellar game configuration that points TrenchBroom at:

- Stellar FGD;
- project texture/material assets;
- map source directory;
- BSP output directory;
- compile tools/wrappers.

Recommended source/output layout:

```text
maps/src/
maps/compiled/
```

or, for tests only:

```text
tests/fixtures/trenchbroom/src/
tests/fixtures/trenchbroom/compiled/
```

### 5.2 Fix FGD field naming

Current helper FGD uses plain fields such as `stellar_script`, which docs say are not importer aliases unless remapped.

For TrenchBroom compatibility, prefer one of these:

Option A — direct dotted keys if TrenchBroom/compiler preserve them:

```text
stellar.script(string)
stellar.table(string)
stellar.extents(string)
stellar.once(choices)
```

Option B — importer-supported aliases, recommended if dotted keys are risky:

```text
_stellar_script(string)
_stellar_table(string)
_stellar_extents(string)
_stellar_once(choices)
_stellar_sprite(string)
_stellar_texture(string)
_stellar_size(string)
_stellar_alpha(choices)
_stellar_collider(choices)
_stellar_enabled(choices)
_stellar_collision(choices)
```

Do not leave plain `stellar_script` in exported BSP output unless a wrapper remaps it before compile.

Recommended branch choice: use importer-supported underscore aliases in the FGD for reliability, and document the exact mapping.

### 5.3 Update FGD classes

Ensure the TrenchBroom FGD covers:

- `worldspawn`
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

Update defaults to Z-up:

```text
info_player_start origin default: "0 0 36"
```

Clearly document:

- `func_door` and `func_button` are named static collision metadata only;
- no moving brush simulation exists in this branch;
- object-collider sensors do not block movement;
- script bindings are authoritative-runtime metadata only.

### 5.4 Editor material/texture package

Make the developer textures selectable in TrenchBroom:

- `stellar_dev_grid_12`
- `stellar_dev_grid_16`
- `stellar_dev_grid_32`
- `stellar_dev_grid_64`
- `stellar_dev_player_72`
- `stellar_dev_wall_96`
- slash aliases if the toolchain supports them:
  - `dev/grid_12`
  - `dev/grid_16`
  - `dev/grid_32`
  - `dev/grid_64`
  - `dev/player_72`
  - `dev/wall_96`

For BSP30/GoldSrc-style tooling, an editor-visible WAD3 may be needed even though the engine has procedural fallback materials. If creating WAD tooling is too large, provide interim editor textures plus docs explaining that runtime validation uses deterministic fallback by material name.

### 5.5 BSP30 compile wrapper

Create a wrapper that:

- accepts `.map` input and `.bsp` output;
- uses the selected BSP30 compiler/toolchain;
- fails if compiler is missing;
- fails if output is not BSP30;
- fails if BSP output is missing entity keys needed by Stellar;
- invokes display-free validation after compile.

Suggested interface:

```bash
tools/bsp/compile_trenchbroom_bsp30.sh \
  --map maps/src/test_room.map \
  --out maps/compiled/test_room.bsp \
  --profile fast
```

Profiles:

- `fast`: CSG/BSP only or minimal light/VIS for quick iteration.
- `full`: CSG/BSP + VIS + LIGHT.
- `validate-only`: skip compile, validate existing BSP30.

### 5.6 Validation wrapper

Create a wrapper that runs:

```bash
stellar-client --validate-map <map.bsp>
stellar-server --validate-config --map <map.bsp>
```

Optionally validate entity keys by dumping entity text through an existing or new debug command. If no entity dump command exists, add a small test/tool only if it is low-cost.

### 5.7 Documentation

Create or complete `docs/TrenchBroom.md`:

Required sections:

- installing/locating TrenchBroom;
- adding the Stellar game profile;
- selecting BSP30 workflow;
- using Z-up coordinates;
- first room dimensions;
- placing `info_player_start` at `0 0 36`;
- choosing developer materials;
- using alias keys;
- compiling to BSP30;
- validating and launching;
- unsupported/deferred features.

Update `docs/BspAuthoring.md` to point to `docs/TrenchBroom.md`.

## Files likely changed

- `tools/trenchbroom/Stellar/GameConfig.cfg`
- `tools/trenchbroom/Stellar/CompilationProfiles.cfg`
- `tools/trenchbroom/Stellar/stellar_entities.fgd`
- `tools/trenchbroom/Stellar/README.md`
- `tools/bsp/compile_trenchbroom_bsp30.sh`
- `tools/bsp/validate_trenchbroom_bsp30.sh`
- `docs/TrenchBroom.md`
- `docs/BspAuthoring.md`
- `CMakeLists.txt` if scripts/tests are wired into CTest

## Validation

Script validation:

```bash
bash -n tools/bsp/compile_trenchbroom_bsp30.sh
bash -n tools/bsp/validate_trenchbroom_bsp30.sh
```

FGD/key audit:

```bash
git grep -n 'stellar_script\|stellar_extents\|stellar_sprite\|stellar_collision' -- tools/trenchbroom tools/bsp docs tests ':!build*/**'
git grep -n '_stellar_script\|stellar.script' -- tools/trenchbroom docs tests ':!build*/**'
```

Build/test:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_authoring_smoke|client_cli_map_validation|dedicated_server)' --output-on-failure
```

## Acceptance criteria

- [ ] TrenchBroom Stellar game profile exists.
- [ ] FGD no longer depends on plain non-alias placeholder keys.
- [ ] FGD defaults are Z-up.
- [ ] BSP30 compile wrapper exists.
- [ ] Validation wrapper exists.
- [ ] Developer materials are available or documented for TrenchBroom use.
- [ ] `docs/TrenchBroom.md` provides end-to-end setup/use instructions.
- [ ] Scripts lint with `bash -n`.

## Parallelization

Safe workstreams:

- Agent A: GameConfig/CompilationProfiles.
- Agent B: FGD cleanup.
- Agent C: material/editor texture package.
- Agent D: compile/validation wrappers.
- Agent E: TrenchBroom docs.

Coordinate on exact paths and script names before wiring CTest.
