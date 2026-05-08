# Project Stellar — TrenchBroom Compile Profile Undefined Variable Fix Plan

Target repo: `JTM-rootstorm/Project-Stellar`
Target branch: `mac-trenchbroom`
Primary area: `tools/trenchbroom/Stellar/`

## User-reported issue

TrenchBroom will not compile maps and reports:

```text
Compilation failed: At line 1, column 1: Cannot convert value 'undefined' of type 'Undefined' to type 'String'
```

## Executive summary

The failure is very likely happening inside TrenchBroom's compilation profile expression evaluation before the Stellar compile shim runs.

The current `CompilationProfiles.cfg` uses variables in invalid scopes and one non-existent variable:

- `workdir`: `${WORK_DIR_PATH}`
  - Problem: `WORK_DIR_PATH` is only available in Tool scope, not Workdir scope.
  - Use `${MAP_DIR_PATH}/../compiled` or `${MAP_DIR_PATH}` instead.
- `parameters`: `${MAP_FULL_PATH}`
  - Problem: TrenchBroom documents `MAP_FULL_NAME`, not `MAP_FULL_PATH`.
  - Use `${MAP_DIR_PATH}/${MAP_FULL_NAME}` instead.
- `tool`: `${STELLAR_BSP30_COMPILE}` / `${STELLAR_BSP30_VALIDATE}`
  - These variables are valid only if TrenchBroom has tool paths configured for the names declared in `GameConfig.cfg`.
  - `GameConfig.cfg` currently includes a `path` key in each compilation tool entry, but TrenchBroom's documented schema only requires `name` and optionally supports `description`. The UI still needs tool paths assigned in Preferences unless TrenchBroom happens to support the extra `path` key in the installed version.

The checked-in linter and smoke test currently enforce the bad profile shape, so Codex must update both the package and the tests.

## Sources to consult

- Official TrenchBroom manual, section: Compiling Maps / Compilation Tools / expression variables.
  - URL: `https://trenchbroom.github.io/manual/latest/index.html`
  - Key points:
    - `WORK_DIR_PATH` scope: Tool only.
    - `MAP_DIR_PATH`, `MAP_BASE_NAME`, `MAP_FULL_NAME` scope: Tool and Workdir.
    - Compilation tool names from `GameConfig.cfg` become Tool-scope variables after paths are set in game preferences.
    - `compilationTools` entries must have `name` and may have `description`.

## Files to inspect

- `tools/trenchbroom/Stellar/CompilationProfiles.cfg`
- `tools/trenchbroom/Stellar/GameConfig.cfg`
- `tools/trenchbroom/Stellar/README.md`
- `tools/trenchbroom/lint_stellar_compilation_profiles.py`
- `tools/trenchbroom/test_stellar_package_paths.sh`
- `tools/trenchbroom/install_stellar_game_package.sh`

## Current problematic profile shape

Current profile pattern:

```json
{
  "name": "Stellar BSP30 Fast",
  "workdir": "${WORK_DIR_PATH}",
  "tasks": [
    {
      "target": "${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp",
      "type": "tool",
      "tool": "${STELLAR_BSP30_COMPILE}",
      "parameters": "--map \"${MAP_FULL_PATH}\" --out \"${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp\" --profile fast"
    }
  ]
}
```

Expected failure mode:

1. TrenchBroom evaluates `workdir`.
2. `${WORK_DIR_PATH}` is undefined in Workdir scope.
3. TrenchBroom tries to convert Undefined to String.
4. Compile aborts before the compile shim runs.

After fixing `workdir`, `${MAP_FULL_PATH}` would likely produce the next undefined-variable failure in tool parameters.

## Required implementation

### Phase 1 — Patch `CompilationProfiles.cfg`

Replace invalid variables with TrenchBroom-documented variables.

Recommended complete replacement:

```json
// Stellar TrenchBroom compilation profiles.
// These profiles invoke project-owned wrappers so the same BSP30 and validation
// policy is used from the editor and from a terminal.
{
    "version": 1,
    "profiles": [
        {
            "name": "Stellar BSP30 Fast",
            "workdir": "${MAP_DIR_PATH}/../compiled",
            "tasks": [
                {
                    "target": "${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp",
                    "type": "tool",
                    "tool": "${STELLAR_BSP30_COMPILE}",
                    "parameters": "--map \"${MAP_DIR_PATH}/${MAP_FULL_NAME}\" --out \"${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp\" --profile fast"
                }
            ]
        },
        {
            "name": "Stellar BSP30 Full Lighting",
            "workdir": "${MAP_DIR_PATH}/../compiled",
            "tasks": [
                {
                    "target": "${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp",
                    "type": "tool",
                    "tool": "${STELLAR_BSP30_COMPILE}",
                    "parameters": "--map \"${MAP_DIR_PATH}/${MAP_FULL_NAME}\" --out \"${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp\" --profile full --toolchain vhlt"
                }
            ]
        },
        {
            "name": "Stellar Validate Existing BSP30",
            "workdir": "${MAP_DIR_PATH}/../compiled",
            "tasks": [
                {
                    "target": "${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp",
                    "type": "tool",
                    "tool": "${STELLAR_BSP30_VALIDATE}",
                    "parameters": "--map \"${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp\""
                }
            ]
        }
    ]
}
```

Notes:

- Keeping `target` with `${WORK_DIR_PATH}` is okay because task fields are evaluated in Tool scope after the profile working directory exists.
- Keeping output under `${WORK_DIR_PATH}` is okay because `WORK_DIR_PATH` is intended for tool/task usage.
- `${MAP_DIR_PATH}/../compiled` assumes maps are saved under `maps/src` and output should go to `maps/compiled`. That matches the package's current default workflow.

### Phase 2 — Patch `lint_stellar_compilation_profiles.py`

Update the linter so it catches the actual TrenchBroom expression-scope issue.

Required linter changes:

1. Reject `workdir == "${WORK_DIR_PATH}"`.
2. Reject any `workdir` containing `${WORK_DIR_PATH}`.
3. Reject any profile/task field containing `${MAP_FULL_PATH}`.
4. Accept `workdir == "${MAP_DIR_PATH}/../compiled"`.
5. Keep existing checks that tool references use `${TOOL_NAME}` and match declared compilation tools.
6. Optionally add a conservative variable validator for known TrenchBroom variables and known declared tool variables.

Suggested scope rules:

```python
WORKDIR_ALLOWED_VARIABLES = {
    "MAP_DIR_PATH",
    "MAP_BASE_NAME",
    "MAP_FULL_NAME",
    "GAME_DIR_PATH",
    "MODS",
    "APP_DIR_PATH",
}

TOOL_ALLOWED_VARIABLES = WORKDIR_ALLOWED_VARIABLES | {
    "WORK_DIR_PATH",
    "CPU_COUNT",
}
```

Dynamic tool variables:

- Add declared compilation tool names from `GameConfig.cfg` to Tool-scope variables.
- Do not add declared compilation tool names to Workdir-scope variables.

Minimum practical checks:

```python
if "${WORK_DIR_PATH}" in workdir:
    errors.append(f"profile {profile_name!r} workdir cannot use ${{WORK_DIR_PATH}}; use ${{MAP_DIR_PATH}} or another Workdir-scope variable")

for label, value in iter_profile_string_fields(...):
    if "${MAP_FULL_PATH}" in value:
        errors.append(f"{label} uses unsupported TrenchBroom variable ${{MAP_FULL_PATH}}; use ${{MAP_DIR_PATH}}/${{MAP_FULL_NAME}}")
```

### Phase 3 — Patch `test_stellar_package_paths.sh`

Update expected token checks.

Replace expectations for:

```text
"workdir": "${WORK_DIR_PATH}"
--map \"${MAP_FULL_PATH}\"
```

with:

```text
"workdir": "${MAP_DIR_PATH}/../compiled"
--map \"${MAP_DIR_PATH}/${MAP_FULL_NAME}\"
```

Add negative fixture checks:

1. A bad profile using `workdir: "${WORK_DIR_PATH}"` must fail lint.
2. A bad profile using `--map "${MAP_FULL_PATH}"` must fail lint.
3. Existing bad profile with display-name tool reference should still fail lint.

### Phase 4 — Clarify `GameConfig.cfg` tool behavior

Inspect whether TrenchBroom accepts the current `path` key under `compilationTools`.

Current shape:

```json
"compilationTools": [
    { "name": "STELLAR_BSP30_COMPILE", "path": "bin/stellar_tb_compile.sh" },
    { "name": "STELLAR_BSP30_VALIDATE", "path": "bin/stellar_tb_validate.sh" }
]
```

Official documented shape:

```json
"compilationTools": [
    { "name": "q3map2", "description": "Path to your q3map2 executable" }
]
```

Recommended conservative fix:

```json
"compilationTools": [
    {
        "name": "STELLAR_BSP30_COMPILE",
        "description": "Path to tools/trenchbroom/Stellar/bin/stellar_tb_compile.sh"
    },
    {
        "name": "STELLAR_BSP30_VALIDATE",
        "description": "Path to tools/trenchbroom/Stellar/bin/stellar_tb_validate.sh"
    }
]
```

Important: Do this only if manual or runtime testing confirms `path` is ignored or harmful. If retaining `path`, add `description` as well and document that users may still need to assign tool paths in Preferences.

### Phase 5 — Update `tools/trenchbroom/Stellar/README.md`

Add a troubleshooting note near compile tools:

```markdown
### TrenchBroom tool path setup

The Stellar game config declares compilation tool variables named `STELLAR_BSP30_COMPILE` and `STELLAR_BSP30_VALIDATE`. In TrenchBroom Preferences > Games > Stellar, set these paths to:

- `tools/trenchbroom/Stellar/bin/stellar_tb_compile.sh`
- `tools/trenchbroom/Stellar/bin/stellar_tb_validate.sh`

If TrenchBroom reports `Cannot convert value 'undefined' of type 'Undefined' to type 'String'`, open `CompilationProfiles.cfg` and verify it does not use `${WORK_DIR_PATH}` in `workdir` and does not use `${MAP_FULL_PATH}` anywhere.
```

### Phase 6 — Optional package install helper improvement

The install helper currently validates required files but cannot configure TrenchBroom's per-user tool path preferences.

Add final printed guidance after installation:

```text
In TrenchBroom Preferences > Games > Stellar, set compilation tool paths:
  STELLAR_BSP30_COMPILE  = <installed_package>/bin/stellar_tb_compile.sh
  STELLAR_BSP30_VALIDATE = <installed_package>/bin/stellar_tb_validate.sh
```

For copied packages, ensure `.stellar_repo_root` is written as today.

## Validation commands

Run from repo root after changes:

```bash
python3 tools/trenchbroom/lint_stellar_compilation_profiles.py \
  --game-config tools/trenchbroom/Stellar/GameConfig.cfg \
  --profiles tools/trenchbroom/Stellar/CompilationProfiles.cfg

bash tools/trenchbroom/test_stellar_package_paths.sh
```

If a BSP30 compiler or VHLT tools are available:

```bash
tools/trenchbroom/Stellar/bin/stellar_tb_compile.sh \
  --map tests/fixtures/trenchbroom/src/minimal_zup_room.map \
  --out /tmp/stellar_tb_compile/minimal_zup_room.bsp \
  --profile fast
```

Optional direct wrapper test:

```bash
tools/bsp/compile_trenchbroom_bsp30.sh \
  --map tests/fixtures/trenchbroom/src/minimal_zup_room.map \
  --out /tmp/stellar_tb_compile/minimal_zup_room.bsp \
  --profile fast \
  --allow-skip
```

Manual TrenchBroom validation:

1. Reinstall/link the package:

   ```bash
   rm -rf "$HOME/Library/Application Support/TrenchBroom/games/Stellar"
   tools/trenchbroom/install_stellar_game_package.sh \
     --dest "$HOME/Library/Application Support/TrenchBroom/games" \
     --link
   ```

2. Open TrenchBroom.
3. Select or refresh the Stellar game profile.
4. In Preferences > Games > Stellar, set:

   ```text
   STELLAR_BSP30_COMPILE  = /absolute/path/to/Project-Stellar/tools/trenchbroom/Stellar/bin/stellar_tb_compile.sh
   STELLAR_BSP30_VALIDATE = /absolute/path/to/Project-Stellar/tools/trenchbroom/Stellar/bin/stellar_tb_validate.sh
   ```

5. Open or create a map saved under `maps/src/`.
6. Run `Stellar BSP30 Fast`.
7. Expected output path: `maps/compiled/<map_name>.bsp`.

## Acceptance criteria

- TrenchBroom no longer reports:

  ```text
  Cannot convert value 'undefined' of type 'Undefined' to type 'String'
  ```

- `Stellar BSP30 Fast` runs the compile shim.
- `Stellar BSP30 Full Lighting` runs the compile shim with `--toolchain vhlt`.
- `Stellar Validate Existing BSP30` runs the validate shim against the compiled BSP.
- `lint_stellar_compilation_profiles.py` fails profiles that use `${WORK_DIR_PATH}` in `workdir`.
- `lint_stellar_compilation_profiles.py` fails profiles that use `${MAP_FULL_PATH}` anywhere.
- `test_stellar_package_paths.sh` passes with the corrected profile variables.
- Documentation tells users where to configure `STELLAR_BSP30_COMPILE` and `STELLAR_BSP30_VALIDATE` in TrenchBroom Preferences.

## Do not do

- Do not change the BSP compiler wrappers unless profile validation reveals a separate post-launch issue.
- Do not remove the package-local shims.
- Do not assume `${MAP_FULL_PATH}` is supported.
- Do not put `${WORK_DIR_PATH}` in profile `workdir`.
- Do not commit absolute local paths to `CompilationProfiles.cfg` or `GameConfig.cfg`.
- Do not replace the existing output convention unless the user explicitly wants a different map layout.

## Suggested Codex prompt

```text
On branch mac-trenchbroom, fix the Stellar TrenchBroom compilation profile undefined-variable failure.

The user sees: "Compilation failed: At line 1, column 1: Cannot convert value 'undefined' of type 'Undefined' to type 'String'".

Root cause: tools/trenchbroom/Stellar/CompilationProfiles.cfg uses ${WORK_DIR_PATH} in workdir, but TrenchBroom only exposes WORK_DIR_PATH in Tool scope. It also uses unsupported ${MAP_FULL_PATH}; TrenchBroom documents MAP_FULL_NAME and MAP_DIR_PATH instead.

Tasks:
1. Patch tools/trenchbroom/Stellar/CompilationProfiles.cfg:
   - workdir must be "${MAP_DIR_PATH}/../compiled".
   - --map must use "${MAP_DIR_PATH}/${MAP_FULL_NAME}".
   - output may continue to use "${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp" inside task fields.
2. Update tools/trenchbroom/lint_stellar_compilation_profiles.py to reject ${WORK_DIR_PATH} in workdir and reject ${MAP_FULL_PATH} anywhere.
3. Update tools/trenchbroom/test_stellar_package_paths.sh expected tokens and add negative tests for those two bad variables.
4. Update tools/trenchbroom/Stellar/README.md to document setting STELLAR_BSP30_COMPILE and STELLAR_BSP30_VALIDATE paths in TrenchBroom Preferences > Games > Stellar.
5. Optionally update install_stellar_game_package.sh final output to print those tool path setup instructions.

Validation:
- python3 tools/trenchbroom/lint_stellar_compilation_profiles.py --game-config tools/trenchbroom/Stellar/GameConfig.cfg --profiles tools/trenchbroom/Stellar/CompilationProfiles.cfg
- bash tools/trenchbroom/test_stellar_package_paths.sh
- If a BSP30 compiler/VHLT tools are present, run tools/trenchbroom/Stellar/bin/stellar_tb_compile.sh against tests/fixtures/trenchbroom/src/minimal_zup_room.map.

Do not commit absolute local paths. Do not remove the package-local shims.
```
