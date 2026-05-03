# Project Stellar — VHLT BSP Toolchain Testing Plan

**Audience:** AI implementation agent working in `JTM-rootstorm/Project-Stellar`  
**Target branch:** `trenchbroom-compat`  
**Reference tool branch:** `JTM-rootstorm/VHLT-V34` branch `linux-compat`  
**Plan date:** 2026-05-02  
**Commit policy:** Do **not** commit. Produce patch/worktree changes only, then report changed files and validation results.

---

## 0. Objective

Add a repeatable, display-free test path that compiles Project Stellar TrenchBroom `.map` fixtures through the Linux VHLT tools now present under `tools/bsp/*`, validates the resulting BSP30 outputs with Stellar’s client/server validators, and updates docs/scripts so future map authoring uses the same VHLT-aware workflow.

This plan must preserve the current branch contract:

- BSP30 remains the TrenchBroom target.
- Z-up authoring remains 1 editor unit = 1 gameplay inch.
- Default CI must not require GPU/display access.
- Default CI must skip clearly if external/vendored VHLT tools are missing or not executable.
- Runtime validation must remain server-authoritative and script-safe.
- Generated BSP outputs stay under `build/` or another ignored output directory; do not check in compiled BSP binaries unless explicitly requested later.

---

## 1. Repository facts verified during planning

### Project Stellar

- Active documentation says the TrenchBroom-compatible workflow targets BSP30, Z-up, 1 authored unit = 1 gameplay inch, and uses `tools/bsp/` compile/validation wrappers.
- `tools/bsp/compile_trenchbroom_bsp30.sh` currently expects a single compiler executable invoked as `<compiler> <map> <out>` and then calls `tools/bsp/validate_trenchbroom_bsp30.sh`.
- `tools/bsp/validate_trenchbroom_bsp30.sh` checks the BSP header version and runs both:
  - `stellar-client --validate-map <map.bsp>`
  - `stellar-server --validate-config --map <map.bsp>`
- Current TrenchBroom source fixtures live at `tests/fixtures/trenchbroom/src/*.map` with this matrix:
  - `minimal_zup_room.map` — positive validation.
  - `entity_matrix_zup.map` — positive validation and supported entity/FGD coverage.
  - `scripted_interaction_zup.map` — positive validation when scripts are present beside the compiled BSP or script root.
  - `invalid_script_escape_zup.map` — compile should succeed, Stellar validation should fail because `_stellar_script` uses `../escape.lua`.
- `CMakeLists.txt` already registers generated BSP fixture tests and a `bsp_authoring_smoke_compile_wrapper` test that skips if no BSP compiler is available.
- The TrenchBroom package uses `tools/trenchbroom/Stellar/GameConfig.cfg` and `CompilationProfiles.cfg`; its profiles call Project Stellar wrappers instead of direct compiler binaries.

### VHLT-V34 linux-compat

- The Linux CMake build defines executable targets:
  - `hlcsg`
  - `hlbsp`
  - `hlvis`
  - `hlrad`
  - `ripent`
- The VHLT smoke pipeline uses the classic stage sequence from a working directory:
  1. `hlcsg minimal_box.map`
  2. `hlbsp minimal_box`
  3. `hlvis minimal_box`
  4. `hlrad minimal_box`
- The VHLT smoke fixture creates a WAD3 file before compile and references it from `worldspawn` via a `wad` key.
- Therefore Project Stellar needs a VHLT-aware wrapper or adapter; the existing single-executable `<compiler> <map> <out>` assumption is not enough for VHLT.

---

## 2. Deliverables

Implement these deliverables in Project Stellar unless a local inspection shows equivalent files already exist.

### New files

1. `tools/bsp/compile_vhlt_bsp30.sh`  
   VHLT-specific wrapper that runs `hlcsg`, `hlbsp`, `hlvis`, and `hlrad` in an isolated work directory, copies the final `.bsp` to `--out`, and optionally invokes Stellar validation.

2. `tools/bsp/run_vhlt_fixture_matrix.sh`  
   Display-free fixture matrix runner for CTest and manual use.

3. `tools/bsp/create_stellar_dev_wad.py`  
   Tiny deterministic WAD3 generator for the developer texture aliases used by fixture maps. This should follow the approach from the VHLT smoke fixture but include Project Stellar material aliases.

4. Optional but recommended: `tools/bsp/README.md`  
   Short toolchain reference for copied/vendored BSP tools and wrapper usage. Add only if there is no better existing docs location.

### Changed files

1. `tools/bsp/compile_trenchbroom_bsp30.sh`  
   Add VHLT delegation through an explicit `--toolchain auto|single|vhlt` option or an environment variable such as `STELLAR_BSP30_TOOLCHAIN=vhlt`. Preserve current single-compiler behavior.

2. `tools/bsp/validate_trenchbroom_bsp30.sh`  
   Usually no semantic change needed. Update usage text only if new wrapper variables or output expectations must be documented.

3. `CMakeLists.txt`  
   Register a new optional/skippable VHLT fixture-matrix CTest.

4. `docs/TrenchBroom.md`  
   Add VHLT toolchain setup, compile examples, output locations, and troubleshooting.

5. `docs/BspAuthoring.md`  
   Add VHLT validation commands to the map validation section and clarify WAD/material requirements for external compilers.

6. `tests/fixtures/trenchbroom/README.md`  
   Add the VHLT fixture matrix and expected positive/negative outcomes.

7. `tools/trenchbroom/Stellar/CompilationProfiles.cfg` and possibly `GameConfig.cfg`  
   Either keep the existing profiles stable by making `compile_trenchbroom_bsp30.sh` auto/delegate to VHLT, or add explicit “Stellar VHLT BSP30” compile profiles. Prefer stable existing profile names unless there is a real editor UX reason to expose both.

8. `docs/ImplementationStatus.md`  
   Add a concise “VHLT BSP toolchain testing” completion note after implementation. Do not rewrite historical completed phase text.

---

## 3. Phase plan

### Phase VHLT-0 — Baseline verification and local discovery

**Goal:** Confirm local branch state and exact VHLT file layout before writing scripts.

**Steps:**

1. Ensure the current branch is `trenchbroom-compat`.
2. Confirm VHLT tools exist and are executable under `tools/bsp/` or a subdirectory under it.
3. Record exact discovered paths for:
   - `hlcsg`
   - `hlbsp`
   - `hlvis`
   - `hlrad`
   - `ripent`
4. Run no-argument/help smoke commands with short timeouts and capture output to `build/vhlt/help/`:
   ```bash
   timeout 10s tools/bsp/hlcsg -help || true
   timeout 10s tools/bsp/hlbsp -help || true
   timeout 10s tools/bsp/hlvis -help || true
   timeout 10s tools/bsp/hlrad -help || true
   timeout 10s tools/bsp/ripent -help || true
   ```
5. Run current baseline validation before changes:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build -j$(nproc)
   ctest --test-dir build -R '^(bsp_authoring_smoke|bsp_validation|bsp_importer|client_cli_map_validation|client_cli_validate_map|server_cli_validate_config)' --output-on-failure
   ```

**Acceptance:** Existing BSP/importer/client/server validation still passes before VHLT changes.

**Parallel safety:** None. Do this first.

---

### Phase VHLT-1 — Add deterministic developer WAD generation

**Goal:** Provide VHLT with real WAD3 texture metadata for Project Stellar developer material aliases without requiring external WADs.

**Implement:** `tools/bsp/create_stellar_dev_wad.py`

**Required texture names:**

Use the slash aliases because they are WAD3-name-length friendly and already appear in the fixture maps:

- `dev/grid_12`
- `dev/grid_16`
- `dev/grid_32`
- `dev/grid_64`
- `dev/player_72`
- `dev/wall_96`

Avoid the longer `stellar_dev_*` names in WAD3 unless local testing proves the toolchain supports them. WAD3 miptex names are short fixed fields, and the slash aliases are the safer compiler-facing set.

**Behavior:**

- Generate a legal WAD3 file with deterministic pixel/palette data.
- Use simple checker/grid patterns; art quality is irrelevant for this test path.
- Do not require Pillow or external Python dependencies.
- Add `--out <path>` usage.
- Return nonzero on invalid arguments or file write errors.

**Validation:**

```bash
python3 tools/bsp/create_stellar_dev_wad.py --out build/tests/fixtures/trenchbroom/vhlt/stellar_dev.wad
python3 - <<'PY'
from pathlib import Path
p = Path('build/tests/fixtures/trenchbroom/vhlt/stellar_dev.wad')
assert p.read_bytes()[:4] == b'WAD3'
assert p.stat().st_size > 128
print('WAD3 smoke passed:', p)
PY
```

**Parallel safety:** Can run in parallel with docs drafting after VHLT-0.

---

### Phase VHLT-2 — Add `compile_vhlt_bsp30.sh`

**Goal:** Wrap the multi-stage VHLT pipeline behind a Project Stellar stable interface.

**Command shape:**

```bash
tools/bsp/compile_vhlt_bsp30.sh \
  --map tests/fixtures/trenchbroom/src/minimal_zup_room.map \
  --out build/tests/fixtures/trenchbroom/vhlt/compiled/minimal_zup_room.bsp \
  --profile full
```

**Required arguments:**

- `--map <source.map>`
- `--out <output.bsp>`
- `--profile fast|full|csg-bsp-only|validate-only` if useful. Keep at least `fast` and `full`.

**Recommended environment overrides:**

- `STELLAR_VHLT_DIR` — directory containing `hlcsg`, `hlbsp`, `hlvis`, `hlrad`, `ripent`; default search should include:
  - `tools/bsp`
  - `tools/bsp/vhlt`
  - `tools/bsp/bin`
  - `PATH`
- `HLCSG`, `HLBSP`, `HLVIS`, `HLRAD`, `RIPENT` — explicit per-tool overrides.
- `STELLAR_CLIENT` and `STELLAR_SERVER` — same meaning as existing validation wrapper.
- `STELLAR_VHLT_KEEP_WORK=1` — preserve temp work directory for debugging.
- `STELLAR_VHLT_LOG_DIR=<dir>` — optional log/artifact copy directory.

**Wrapper behavior:**

1. Locate all required VHLT tools. If a tool is missing and the wrapper is being used from CTest, exit `77` for skip; for direct manual invocation, fail with a clear diagnostic unless `--allow-skip` is passed.
2. Create an isolated working directory under `build/tests/fixtures/trenchbroom/vhlt/work/<fixture>/` or `mktemp -d`.
3. Copy the source `.map` to the working directory.
4. Generate `stellar_dev.wad` in the working directory.
5. Ensure the working map has a `worldspawn` `wad` key pointing to `stellar_dev.wad`.
   - Prefer transient injection into the copied map.
   - Do **not** add local absolute WAD paths to source-tree fixture maps.
   - Preserve all entity keys and brush planes exactly aside from the transient `wad` key.
6. Run VHLT from inside the working directory using the classic base-name flow:
   ```bash
   "$HLCSG" "$base.map"
   "$HLBSP" "$base"
   # Full profile only:
   "$HLVIS" "$base"
   "$HLRAD" "$base"
   ```
7. Copy `$work/$base.bsp` to `--out`.
8. Check final BSP header version is 30.
9. For positive fixtures, call `tools/bsp/validate_trenchbroom_bsp30.sh --map "$out"` unless `--no-stellar-validation` is passed.
10. Copy `.log`, `.err`, `.p*`, `.prt`, `.lin`, `.ent`, and relevant tool outputs into the log directory when available.

**Profile recommendations:**

- `fast`: run `hlcsg` + `hlbsp`, then Stellar validation. Use for quick iteration.
- `full`: run `hlcsg` + `hlbsp` + `hlvis` + `hlrad`, then Stellar validation. Use for the fixture matrix.
- `validate-only`: do not compile; only run header and Stellar validation on `--out` or `--bsp`.

Do not pass VHLT flags such as `-fast` until local `-help` output confirms the exact supported syntax. A no-extra-flags wrapper is slower but safer for first integration.

**Acceptance:**

This must produce a nonempty BSP30 for `minimal_zup_room.map` and pass Stellar validation:

```bash
tools/bsp/compile_vhlt_bsp30.sh \
  --map tests/fixtures/trenchbroom/src/minimal_zup_room.map \
  --out build/tests/fixtures/trenchbroom/vhlt/compiled/minimal_zup_room.bsp \
  --profile full

tools/bsp/validate_trenchbroom_bsp30.sh \
  --map build/tests/fixtures/trenchbroom/vhlt/compiled/minimal_zup_room.bsp
```

**Parallel safety:** Can run in parallel with Phase VHLT-3 if one agent owns only `compile_vhlt_bsp30.sh` and another owns the generic wrapper delegation.

---

### Phase VHLT-3 — Preserve and extend the existing TrenchBroom wrapper

**Goal:** Existing editor profiles and terminal commands should continue to work, while VHLT becomes a first-class toolchain.

**Modify:** `tools/bsp/compile_trenchbroom_bsp30.sh`

**Required behavior:**

- Add `--toolchain auto|single|vhlt`; default should be backward-compatible.
- Add `STELLAR_BSP30_TOOLCHAIN` environment override.
- `single` preserves the existing behavior:
  ```bash
  <compiler> <map> <out>
  ```
- `vhlt` delegates to:
  ```bash
  tools/bsp/compile_vhlt_bsp30.sh --map <map> --out <out> --profile <profile>
  ```
- `auto` should prefer an explicit `STELLAR_BSP30_COMPILER`/`QBSP` single compiler when set, otherwise use detected VHLT tools under `tools/bsp/`, otherwise search legacy single-compiler names as before.
- Preserve `validate-only` behavior.
- Preserve current usage examples and add VHLT examples.

**Acceptance:**

```bash
bash -n tools/bsp/compile_trenchbroom_bsp30.sh
bash -n tools/bsp/compile_vhlt_bsp30.sh
bash -n tools/bsp/validate_trenchbroom_bsp30.sh

STELLAR_BSP30_TOOLCHAIN=vhlt tools/bsp/compile_trenchbroom_bsp30.sh \
  --map tests/fixtures/trenchbroom/src/minimal_zup_room.map \
  --out build/tests/fixtures/trenchbroom/vhlt/compiled/wrapper_delegate_minimal_zup_room.bsp \
  --profile full
```

**Parallel safety:** Can run after the first working `compile_vhlt_bsp30.sh` skeleton exists.

---

### Phase VHLT-4 — Add fixture matrix runner

**Goal:** Exercise all current source `.map` fixtures through VHLT and Stellar validation.

**Implement:** `tools/bsp/run_vhlt_fixture_matrix.sh`

**Positive matrix:**

| Fixture | Compile stages | Stellar validation | Extra expectations |
| --- | --- | --- | --- |
| `minimal_zup_room` | `hlcsg`, `hlbsp`, `hlvis`, `hlrad` | must pass | BSP30 header, player spawn retained |
| `entity_matrix_zup` | full chain | must pass | supported entity classes retained |
| `scripted_interaction_zup` | full chain | must pass | copy `tests/fixtures/trenchbroom/scripts/gate.lua` and `pickup.lua` beside output under `scripts/` before validation |

**Negative matrix:**

| Fixture | Compile stages | Stellar validation | Expected result |
| --- | --- | --- | --- |
| `invalid_script_escape_zup` | full chain should succeed | must fail | failure must be due script path escape, not brush/toolchain corruption |

**Runner behavior:**

- Locate repository root and build root.
- Accept optional arguments:
  - `--source-root <repo>`
  - `--build-root <build>`
  - `--profile full|fast`
  - `--keep-going`
- Produce outputs under:
  - `build/tests/fixtures/trenchbroom/vhlt/compiled/`
  - `build/tests/fixtures/trenchbroom/vhlt/logs/`
  - `build/tests/fixtures/trenchbroom/vhlt/work/`
- If VHLT tools are absent, print one clear skip line and exit `77`.
- Positive fixture failure must return nonzero.
- Negative fixture behavior:
  1. Compile with `--no-stellar-validation`.
  2. Run `validate_trenchbroom_bsp30.sh --map <bad.bsp>` separately.
  3. Assert validation fails.
  4. Assert logs mention script path escape or the known diagnostic text. Use a broad but specific grep such as `script.*escape|kScriptPathEscape|path.*escape`.

**Optional BSP-level VHLT checks:**

- Probe `ripent` on VHLT-produced BSPs only after local `-help` confirms syntax.
- If syntax is confirmed, export entity lumps and verify expected classnames:
  - `worldspawn`
  - `info_player_start`
  - fixture-specific classes.
- Do not make `ripent` mandatory if its CLI syntax is unclear or unstable. The core acceptance is VHLT compile chain + Stellar validation.

**Acceptance:**

```bash
tools/bsp/run_vhlt_fixture_matrix.sh --source-root . --build-root build --profile full
```

Expected result: positive fixtures pass; invalid script escape compiles but fails Stellar validation for the expected reason.

**Parallel safety:** Can run after VHLT-2. Docs can be updated in parallel from this phase’s target behavior.

---

### Phase VHLT-5 — Wire CTest without making CI brittle

**Goal:** Make the VHLT matrix available to CTest while preserving display-free and missing-tool skip behavior.

**Modify:** `CMakeLists.txt`

**Recommended CTest addition:**

Add a test after `stellar-client` and `stellar-server` are declared:

```cmake
add_test(NAME bsp_authoring_smoke_vhlt_fixture_matrix
    COMMAND bash "${CMAKE_SOURCE_DIR}/tools/bsp/run_vhlt_fixture_matrix.sh"
        --source-root "${CMAKE_SOURCE_DIR}"
        --build-root "${CMAKE_BINARY_DIR}"
        --profile full
)
set_tests_properties(bsp_authoring_smoke_vhlt_fixture_matrix PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "STELLAR_CLIENT=$<TARGET_FILE:stellar-client>;STELLAR_SERVER=$<TARGET_FILE:stellar-server>"
)
```

If generator expressions in `ENVIRONMENT` are problematic for the project’s CMake version/generator, pass those paths as script arguments instead:

```cmake
COMMAND bash "${CMAKE_SOURCE_DIR}/tools/bsp/run_vhlt_fixture_matrix.sh"
    --source-root "${CMAKE_SOURCE_DIR}"
    --build-root "${CMAKE_BINARY_DIR}"
    --stellar-client "$<TARGET_FILE:stellar-client>"
    --stellar-server "$<TARGET_FILE:stellar-server>"
```

**Do not:**

- Make VHLT tests depend on a display/GPU.
- Fail the default test suite merely because VHLT binaries are missing.
- Check compiled BSP outputs into source.

**Validation:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_authoring_smoke_vhlt_fixture_matrix|bsp_authoring_smoke|client_cli_map_validation|server_cli_validate_config)' --output-on-failure
ctest --test-dir build --output-on-failure
```

**Parallel safety:** Can run after VHLT-4 exists.

---

### Phase VHLT-6 — Update TrenchBroom package profile references

**Goal:** Ensure editor and terminal workflows point to the same VHLT-aware wrappers.

**Preferred approach:** Keep existing profile names in `tools/trenchbroom/Stellar/CompilationProfiles.cfg` unchanged, because they already call `tools/bsp/compile_trenchbroom_bsp30.sh`. Once that wrapper delegates to VHLT, the editor inherits VHLT support automatically.

**Optional addition:** If local testing shows users need an explicit editor choice, add profiles:

- `Stellar VHLT BSP30 Fast`
- `Stellar VHLT BSP30 Full`

with parameters like:

```text
--map ${MAP_FULL_PATH} --out ${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp --profile full --toolchain vhlt
```

**Modify `GameConfig.cfg` only if:**

- A new separate wrapper tool must be exposed to TrenchBroom; or
- The existing compile wrapper path changes.

Do not add absolute local paths to the game config.

**Validation:**

- Open/check config syntax as text.
- Confirm profile command still references project-owned wrappers.
- Run shell syntax validation for wrappers.

**Parallel safety:** Can run in parallel with docs once wrapper command shape is finalized.

---

### Phase VHLT-7 — Documentation updates

**Goal:** Make VHLT the documented Linux BSP30 toolchain path without losing current generic compiler support.

#### `docs/TrenchBroom.md`

Add/update:

- “VHLT Linux toolchain” section:
  - tools expected under `tools/bsp/` or `STELLAR_VHLT_DIR`.
  - exact wrapper command examples.
  - output directory recommendation: `maps/compiled/` for manual maps, `build/tests/fixtures/trenchbroom/vhlt/compiled/` for generated test artifacts.
- “Compile to BSP30” section:
  - generic single compiler remains supported.
  - VHLT can be selected with `--toolchain vhlt` or `STELLAR_BSP30_TOOLCHAIN=vhlt`.
- “Validate and launch” section:
  - remind users to run `validate_trenchbroom_bsp30.sh` after compile.
- “Troubleshooting” section:
  - missing VHLT tools.
  - missing/invalid WAD generation.
  - VHLT rejects slash texture names.
  - incomplete brush diagnostics from `hlcsg`.
  - positive compile but Stellar validation failure due script path escape.

#### `docs/BspAuthoring.md`

Add/update:

- Validation command block for VHLT fixture matrix.
- Clarify that source `.map` files should remain clean authoring references; wrappers may inject temporary WAD references in build/work directories.
- Clarify VHLT compile success is not enough; Stellar client/server validation is required.

#### `tests/fixtures/trenchbroom/README.md`

Add/update:

- VHLT fixture matrix table.
- Expected positive/negative outcomes.
- Location of VHLT-generated outputs/logs.
- Manual command:
  ```bash
  tools/bsp/run_vhlt_fixture_matrix.sh --source-root . --build-root build --profile full
  ```

#### `docs/ImplementationStatus.md`

Add a concise new follow-up scope after implementation:

```markdown
## Completed Follow-up Scope — VHLT BSP Toolchain Testing

Status: complete as of YYYY-MM-DD.

- Added VHLT-aware BSP30 compile wrapper over `hlcsg`, `hlbsp`, `hlvis`, and `hlrad`.
- Added deterministic developer WAD generation for fixture/source-map compilation.
- Added display-free VHLT fixture matrix covering positive TrenchBroom fixtures and the invalid script path negative fixture.
- Updated TrenchBroom/BSP authoring docs and fixture README.
- Default tests skip clearly when VHLT tools are unavailable and remain display-free.

Validation run:

```bash
...
```
```

Do not rewrite old completed branch sections except where directly necessary to reference the new follow-up.

**Parallel safety:** Documentation can run in parallel with VHLT-5 after command names stabilize.

---

### Phase VHLT-8 — Final validation and handoff

**Goal:** Prove the whole path works and produce an AI-agent-readable result summary.

**Required commands:**

```bash
bash -n tools/bsp/compile_trenchbroom_bsp30.sh
bash -n tools/bsp/compile_vhlt_bsp30.sh
bash -n tools/bsp/validate_trenchbroom_bsp30.sh
bash -n tools/bsp/run_vhlt_fixture_matrix.sh
python3 tools/bsp/create_stellar_dev_wad.py --out build/tests/fixtures/trenchbroom/vhlt/stellar_dev.wad

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_authoring_smoke_vhlt_fixture_matrix|bsp_authoring_smoke|bsp_validation|bsp_importer|client_cli_map_validation|client_cli_validate_map|server_cli_validate_config|client_cli_map_validation_rejects_bad_map|server_cli_rejects_bad_map)' --output-on-failure
ctest --test-dir build --output-on-failure
```

**Manual commands to include in final handoff:**

```bash
STELLAR_BSP30_TOOLCHAIN=vhlt tools/bsp/compile_trenchbroom_bsp30.sh \
  --map tests/fixtures/trenchbroom/src/minimal_zup_room.map \
  --out build/tests/fixtures/trenchbroom/vhlt/compiled/manual_minimal_zup_room.bsp \
  --profile full

tools/bsp/run_vhlt_fixture_matrix.sh --source-root . --build-root build --profile full
```

**Final response/handoff must include:**

- Changed files.
- Whether VHLT tools were found locally.
- Exact VHLT tool paths used.
- Pass/fail/skip status for each fixture.
- Any VHLT warnings that appear consistently but do not fail validation.
- Location of compiled BSP outputs and logs.
- Confirmation that no commit was made.

---

## 4. Parallelization map

Use this if multiple AI agents/subtasks are available.

| Workstream | Can start after | Owns files | Notes |
| --- | --- | --- | --- |
| Tool discovery | immediately | none | Must finish before wrappers rely on exact paths. |
| WAD generator | VHLT-0 | `tools/bsp/create_stellar_dev_wad.py` | Safe parallel task. |
| VHLT wrapper | VHLT-0, WAD generator skeleton | `tools/bsp/compile_vhlt_bsp30.sh` | Central dependency for tests/docs. |
| Generic wrapper delegation | VHLT wrapper command shape | `tools/bsp/compile_trenchbroom_bsp30.sh` | Avoid simultaneous edits with VHLT wrapper owner if sharing helpers. |
| Fixture matrix runner | VHLT wrapper first working version | `tools/bsp/run_vhlt_fixture_matrix.sh` | Can iterate independently after wrapper interface stabilizes. |
| CMake integration | matrix runner exists | `CMakeLists.txt` | Must wait for runner path/args. |
| Docs | wrapper command shape stable | docs + fixture README + TrenchBroom profiles | Can draft early, finalize after validation. |
| Final validation | all code/docs done | none | Single owner to avoid conflicting build/test state. |

---

## 5. Acceptance criteria

The implementation is complete when all of these are true:

1. `minimal_zup_room.map`, `entity_matrix_zup.map`, and `scripted_interaction_zup.map` compile through VHLT and pass Stellar client/server validation.
2. `invalid_script_escape_zup.map` compiles through VHLT but fails Stellar validation for the expected script path escape reason.
3. The wrapper produces BSP30 files and verifies header version 30.
4. Default CTest remains display-free and does not hard-fail when VHLT binaries are absent; missing tools produce a clear skip.
5. Existing generic single-compiler workflow remains available.
6. Documentation references VHLT tools under `tools/bsp/*`, the new wrapper commands, the fixture matrix, and troubleshooting steps.
7. Generated BSP/WAD/log artifacts stay under ignored build/output paths.
8. No commits are made.

---

## 6. Known risks and mitigation

| Risk | Mitigation |
| --- | --- |
| VHLT requires a WAD referenced from `worldspawn`, while Project Stellar fixtures currently rely on runtime fallback material names. | Generate a temporary WAD and inject a temporary `wad` key into the copied work map only. Do not modify source fixtures with local WAD paths. |
| VHLT rejects slash texture names such as `dev/grid_32`. | First confirm with local compile. If rejected, add short compile-safe aliases such as `DEV_GRID32`, update developer texture fallback to recognize them, and document the mapping. Do not silently change runtime material semantics. |
| VHLT tools write outputs beside the input and expect a basename rather than explicit output path. | Always run in an isolated work directory and copy final `.bsp` to `--out`. |
| `hlvis`/`hlrad` option syntax differs from expectations. | Initially run no extra flags. Add fast/full flags only after `-help` confirms supported syntax. |
| `ripent` syntax is unclear. | Treat ripent checks as optional/probe-only until syntax is confirmed. Core test path is compile chain + Stellar validation. |
| Existing generated BSP fixtures are synthetic and may not be valid inputs for VHLT post-processing tools. | Do not require VHLT to process synthetic fixture-writer BSPs. Use VHLT on source `.map` fixtures and validate resulting BSPs with Stellar. |
| Scripted fixture validation fails because Lua scripts are not beside the compiled BSP. | Matrix runner must copy required scripts into `compiled/scripts/` or set the expected script root before validation. |

---

## 7. Suggested final handoff template

Use this template in the final implementation report:

```markdown
## VHLT BSP toolchain testing handoff

Branch: trenchbroom-compat
Commit made: no

### Files changed
- ...

### VHLT tools discovered
- hlcsg: ...
- hlbsp: ...
- hlvis: ...
- hlrad: ...
- ripent: ...

### Fixture results
| Fixture | Compile | Stellar validation | Notes |
| --- | --- | --- | --- |
| minimal_zup_room | pass | pass | ... |
| entity_matrix_zup | pass | pass | ... |
| scripted_interaction_zup | pass | pass | scripts copied to ... |
| invalid_script_escape_zup | pass | expected fail | diagnostic: ... |

### Validation commands run
```bash
...
```

### Artifacts
- Compiled BSPs: build/tests/fixtures/trenchbroom/vhlt/compiled/
- Logs: build/tests/fixtures/trenchbroom/vhlt/logs/

### Known follow-ups
- ...
```
