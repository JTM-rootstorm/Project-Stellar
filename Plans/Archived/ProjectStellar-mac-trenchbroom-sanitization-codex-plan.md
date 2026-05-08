# Project Stellar — `mac-trenchbroom` Sanitization Fix Plan for Codex

Target repository: `JTM-rootstorm/Project-Stellar`  
Target branch: `mac-trenchbroom`  
Merge target: `main`  
Primary areas:

- `tools/trenchbroom/`
- `tools/trenchbroom/Stellar/`
- `tools/bsp/`
- `docs/TrenchBroom.md`
- `.gitignore`

## Scope clarification

macOS support for this work means **Apple Silicon macOS only**: `Darwin:arm64` / `Darwin:aarch64`.

Do **not** add Intel macOS support, Intel macOS test paths, Rosetta assumptions, or documentation implying Intel macOS is supported. Linux support remains `Linux:x86_64` / `Linux:amd64`.

## Executive summary

The branch is mostly merge-ready from a static sanity pass. The corrected TrenchBroom compile profile shape is good, the linter coverage is useful, and the docs now mostly match the intended package/shim workflow.

Before merging, patch these merge-goblin hotspots:

1. Symlink install mode likely fails to find the repo root when TrenchBroom runs shims through a linked game package.
2. `auto` toolchain selection does not recognize externally configured VHLT tools via `STELLAR_VHLT_DIR` or `HLCSG` / `HLBSP` / `HLVIS` / `HLRAD`, even though the VHLT wrapper itself supports them.
3. Host executable compatibility checks can falsely reject universal Mach-O binaries on Apple Silicon because they reject any `file` output containing `x86_64`, even if `arm64` is also present.
4. Documentation should make the TrenchBroom game path / tool path setup a little more explicit for copied or linked packages.

The compile-profile undefined-variable fix itself appears sane. Do not undo it.

## Current good state to preserve

Preserve the current `CompilationProfiles.cfg` variable shape:

- `workdir`: `${MAP_DIR_PATH}/../compiled`
- source map path: `${MAP_DIR_PATH}/${MAP_FULL_NAME}`
- output path inside task/tool fields: `${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp`
- tool references: `${STELLAR_BSP30_COMPILE}` and `${STELLAR_BSP30_VALIDATE}`

Preserve the current `GameConfig.cfg` tool declaration style:

- compilation tools should declare variable-safe names.
- descriptions are acceptable and safer than relying on undocumented `path` fields.
- users still configure actual tool paths in TrenchBroom Preferences.

Preserve these policies:

- Source `.map` files stay clean.
- No absolute local WAD paths in source maps.
- Package-local shims remain in `tools/trenchbroom/Stellar/bin/`.
- External compiler coverage remains optional and may skip with return code `77`.
- No changes should be committed directly to `main`.

## Phase 1 — Fix linked package repository-root resolution

### Problem

`tools/trenchbroom/install_stellar_game_package.sh --link` creates `DEST/Stellar -> <repo>/tools/trenchbroom/Stellar`.

The shim env helper currently resolves the package root with a logical `cd ... && pwd` style path. If a user executes:

```bash
$HOME/Library/Application Support/TrenchBroom/games/Stellar/bin/stellar_tb_compile.sh
```

and `Stellar` is a symlink, the resolver may walk upward through the user games directory instead of the physical repo checkout. Since link mode does not write `.stellar_repo_root`, repo discovery can fail unless `STELLAR_REPO_ROOT` is exported.

This is especially relevant on macOS because Finder-launched TrenchBroom does not reliably inherit shell exports.

### Files to patch

- `tools/trenchbroom/Stellar/bin/stellar_tb_env.sh`
- `tools/trenchbroom/test_stellar_package_paths.sh`
- optionally `tools/trenchbroom/install_stellar_game_package.sh`
- `tools/trenchbroom/Stellar/README.md`
- `docs/TrenchBroom.md`

### Implementation

Patch env path normalization so symlinked package shims resolve to the physical checkout path.

Recommended minimal patch pattern:

```bash
stellar_tb_env_package_root() {
    local source_path="${BASH_SOURCE[0]}"
    local source_dir
    source_dir="$(cd -P "$(dirname "$source_path")" && pwd -P)" || return 1
    cd -P "$source_dir/.." && pwd -P
}

stellar_tb_env_normalize_dir() {
    local path="$1"
    [[ -n "$path" ]] || return 1
    [[ -d "$path" ]] || return 1
    cd -P "$path" && pwd -P
}
```

Use `pwd -P` consistently in the env helper. Avoid requiring GNU `realpath` on macOS.

### Test additions

Extend `tools/trenchbroom/test_stellar_package_paths.sh`:

1. Create a temporary `Games With Spaces` directory.
2. Run install helper with `--link`.
3. Unset `STELLAR_REPO_ROOT`.
4. Invoke linked package shims through the linked path:
   - `"$linked_package/bin/stellar_tb_compile.sh" --help`
   - `"$linked_package/bin/stellar_tb_validate.sh" --help`
5. Add a fake compile wrapper capture test through the linked path, similar to the existing copied-package quoted argument test.

The test should prove:

- linked shims find the repo root without `STELLAR_REPO_ROOT`.
- quoted map/output args survive paths with spaces.
- no `.stellar_repo_root` is required for `--link`.

### Documentation update

Clarify:

- `--copy` writes `.stellar_repo_root`.
- `--link` resolves the physical checkout path automatically.
- `STELLAR_REPO_ROOT` remains an override and is still useful if the checkout moves or if the package is manually copied without `.stellar_repo_root`.

## Phase 2 — Make `auto` toolchain selection honor external VHLT env

### Problem

`tools/bsp/compile_vhlt_bsp30.sh` supports:

- `STELLAR_VHLT_DIR`
- `HLCSG`
- `HLBSP`
- `HLVIS`
- `HLRAD`
- `RIPENT`

But `tools/bsp/compile_trenchbroom_bsp30.sh` only auto-selects VHLT when repo-local platform tools are found under:

- `tools/bsp/macos-arm64/`
- `tools/bsp/linux-x86_64/`
- legacy repo-local layouts

So an Apple Silicon user with external host-native VHLT tools in `STELLAR_VHLT_DIR` may still get:

```text
no BSP30 toolchain found
```

unless they explicitly set `STELLAR_BSP30_TOOLCHAIN=vhlt` or use the Full Lighting profile, which passes `--toolchain vhlt`.

### Files to patch

- `tools/bsp/compile_trenchbroom_bsp30.sh`
- `tools/trenchbroom/test_stellar_package_paths.sh`
- optionally add a focused test script under `tools/bsp/`

### Implementation

Teach `compile_trenchbroom_bsp30.sh` auto-selection to detect external VHLT availability.

Recommended helper behavior:

- VHLT is available when all required tools for a fast geometry compile are host-executable:
  - `hlcsg`
  - `hlbsp`
- For full lighting, `compile_vhlt_bsp30.sh` will still require:
  - `hlvis`
  - `hlrad`

For auto-selection, requiring `hlcsg` + `hlbsp` is enough because `fast` only needs those and `full` is already explicitly VHLT in the profile.

Add lookup order to `has_vhlt_tools_under_bsp` or a renamed helper:

1. Explicit per-tool env:
   - `HLCSG`
   - `HLBSP`
2. `STELLAR_VHLT_DIR/<tool>`
3. platform repo-local directory:
   - Apple Silicon: `tools/bsp/macos-arm64/<tool>`
   - Linux x86_64: `tools/bsp/linux-x86_64/<tool>`
4. legacy repo-local layouts:
   - `tools/bsp/<tool>`
   - `tools/bsp/vhlt/<tool>`
   - `tools/bsp/bin/<tool>`
5. optional `PATH` fallback for `hlcsg` / `hlbsp`, if desired.

Keep explicit single compilers higher priority:

1. `STELLAR_BSP30_COMPILER`
2. `QBSP`
3. VHLT tools
4. legacy single compilers on `PATH`

### Test additions

Add a no-real-compiler unit test using a fake repo:

1. Create temp fake repo with:
   - `CMakeLists.txt`
   - `tools/bsp/compile_trenchbroom_bsp30.sh` copied from real repo
   - fake `tools/bsp/compile_vhlt_bsp30.sh` that records args and exits 0
   - fake `tools/bsp/validate_trenchbroom_bsp30.sh`
   - fake `tools/bsp/validate_trenchbroom_map_source.py`
2. Create `STELLAR_VHLT_DIR` containing executable fake `hlcsg` and `hlbsp`.
3. Ensure no `STELLAR_BSP30_COMPILER` or `QBSP` is set.
4. Run:

```bash
STELLAR_VHLT_DIR="$fake_vhlt_dir" bash "$fake_repo/tools/bsp/compile_trenchbroom_bsp30.sh"   --map "$fake_map"   --out "$fake_out"   --profile fast
```

5. Assert the fake `compile_vhlt_bsp30.sh` was called with:
   - `--map "$fake_map"`
   - `--out "$fake_out"`
   - `--profile fast`

This avoids needing a valid BSP, real VHLT, `stellar-client`, or `stellar-server`.

## Phase 3 — Fix Apple Silicon universal Mach-O executable checks

### Problem

The current compatibility guard rejects Apple Silicon hosts when `file` output contains `x86_64`. That can reject universal Mach-O binaries, which often contain both `arm64` and `x86_64`.

The branch does not need Intel macOS support, but it should accept Apple Silicon-compatible universal binaries.

### Files to patch

Patch duplicated `host_tool_is_executable` logic in:

- `tools/bsp/compile_trenchbroom_bsp30.sh`
- `tools/bsp/compile_vhlt_bsp30.sh`
- `tools/trenchbroom/test_stellar_package_paths.sh`

Optional follow-up:

- Extract shared shell helpers into `tools/bsp/host_tool_compat.sh`.
- Source the helper from both wrappers and tests.

Prefer a minimal duplicate patch for this branch unless the helper extraction stays very small.

### Required platform behavior

Supported:

- `Darwin:arm64`
- `Darwin:aarch64`
- `Linux:x86_64`
- `Linux:amd64`

Not supported:

- Intel macOS
- non-x86_64 Linux
- Windows

### Apple Silicon logic

On Darwin arm64/aarch64:

- reject ELF.
- accept Mach-O arm64.
- accept universal Mach-O if it includes arm64, even if it also includes x86_64.
- reject Mach-O x86_64-only.
- do not add Rosetta behavior.

Pseudo-logic:

```bash
case "$host:$machine" in
    Darwin:arm64|Darwin:aarch64)
        case "$info" in
            *ELF*) return 1 ;;
            *Mach-O*)
                [[ "$info" == *arm64* || "$info" == *aarch64* ]] || return 1
                ;;
        esac
        ;;
esac
```

### Linux x86_64 logic

On Linux x86_64/amd64:

- reject Mach-O.
- reject aarch64 / ARM aarch64 ELF.
- accept x86-64 ELF.

Keep the existing Linux behavior unless test output reveals a false positive/negative.

### Test additions

Add or extend shell test coverage using a small testable helper function.

Mock `file` output cases:

Apple Silicon accepted:

```text
Mach-O 64-bit executable arm64
Mach-O universal binary with 2 architectures: [x86_64] [arm64]
Mach-O universal binary with 2 architectures: [arm64] [x86_64]
```

Apple Silicon rejected:

```text
ELF 64-bit LSB executable
Mach-O 64-bit executable x86_64
```

Linux x86_64 accepted:

```text
ELF 64-bit LSB executable, x86-64
```

Linux x86_64 rejected:

```text
Mach-O 64-bit executable arm64
ELF 64-bit LSB executable, ARM aarch64
```

Do not add Intel macOS acceptance tests.

## Phase 4 — Tighten docs for TrenchBroom game path and tool path setup

### Problem

The package docs correctly describe tool variables, but users can still confuse:

- the TrenchBroom game package directory,
- the Stellar game path/material search root,
- the compile tool variable paths,
- copied vs linked installs.

This can surface as missing material thumbnails, undefined tool variables, or shims failing to find the repo.

### Files to patch

- `tools/trenchbroom/Stellar/README.md`
- `docs/TrenchBroom.md`
- optionally `tools/trenchbroom/install_stellar_game_package.sh`

### Implementation

Add explicit setup bullets after install examples:

```markdown
After installing or linking the package, open TrenchBroom Preferences > Games > Stellar and verify:

- The Stellar game package points at the installed `Stellar/` directory.
- `STELLAR_BSP30_COMPILE` points at `<installed package>/bin/stellar_tb_compile.sh`.
- `STELLAR_BSP30_VALIDATE` points at `<installed package>/bin/stellar_tb_validate.sh`.
- For copied packages, `.stellar_repo_root` exists or `STELLAR_REPO_ROOT` is exported.
- For linked packages, the shims resolve the physical checkout path; `STELLAR_REPO_ROOT` is optional.
```

For macOS, keep the note:

- Launch TrenchBroom from a terminal when relying on shell exports.
- Finder-launched apps may not inherit exports.

But also say that a helper-installed copied package should not need `STELLAR_REPO_ROOT` unless the checkout moved.

## Phase 5 — Re-run focused validation

Run from repo root after patches.

### Static/script checks

```bash
python3 tools/trenchbroom/lint_stellar_compilation_profiles.py   --game-config tools/trenchbroom/Stellar/GameConfig.cfg   --profiles tools/trenchbroom/Stellar/CompilationProfiles.cfg

bash tools/trenchbroom/test_stellar_package_paths.sh
python3 tools/bsp/test_normalize_vhlt_light_angles.py
python3 tools/bsp/test_map_rewrite.py
```

### CMake/CTest checks

On Apple Silicon macOS:

```bash
cmake --preset macos-default
cmake --build --preset macos-default

ctest --test-dir build-macos   -R "trenchbroom|vhlt|bsp_lightmaps|docs_consistency"   --output-on-failure
```

If using Metal presets:

```bash
cmake --preset macos-metal
cmake --build --preset macos-metal

ctest --test-dir build-macos-metal   -R "trenchbroom|vhlt|bsp_lightmaps|docs_consistency"   --output-on-failure
```

On Linux x86_64, if available:

```bash
cmake --preset linux-default
cmake --build --preset linux-default

ctest --test-dir build-linux   -R "trenchbroom|vhlt|bsp_lightmaps|docs_consistency"   --output-on-failure
```

External compiler / VHLT tests may skip with return code `77` when host-native tools are unavailable.

### Manual Apple Silicon TrenchBroom smoke

Use Apple Silicon macOS hardware.

1. Remove any stale user package:

   ```bash
   rm -rf "$HOME/Library/Application Support/TrenchBroom/games/Stellar"
   ```

2. Test copy install:

   ```bash
   tools/trenchbroom/install_stellar_game_package.sh      --dest "$HOME/Library/Application Support/TrenchBroom/games"      --copy
   ```

3. Open TrenchBroom.
4. Select the Stellar profile.
5. In Preferences > Games > Stellar, set:
   - `STELLAR_BSP30_COMPILE = <installed package>/bin/stellar_tb_compile.sh`
   - `STELLAR_BSP30_VALIDATE = <installed package>/bin/stellar_tb_validate.sh`
6. Open `templates/minimal_zup_room.map` or save a test map under `maps/src/`.
7. Run `Stellar BSP30 Fast`.
8. Confirm output lands in `maps/compiled/<map_name>.bsp`.
9. Run `Stellar Validate Existing BSP30`.

Repeat with `--link` install after the symlink resolver patch:

```bash
rm -rf "$HOME/Library/Application Support/TrenchBroom/games/Stellar"

tools/trenchbroom/install_stellar_game_package.sh   --dest "$HOME/Library/Application Support/TrenchBroom/games"   --link
```

The linked package should work without `STELLAR_REPO_ROOT`.

## Acceptance criteria

- `--copy` package install still writes `.stellar_repo_root`.
- `--link` package install works without `STELLAR_REPO_ROOT`.
- Linked package shims resolve the physical repo checkout.
- Paths with spaces remain supported.
- `auto` toolchain selection recognizes external VHLT tools from `STELLAR_VHLT_DIR`.
- Apple Silicon macOS accepts arm64 and universal Mach-O tools.
- Apple Silicon macOS rejects x86_64-only Mach-O tools.
- No Intel macOS support paths or docs are added.
- Existing compile-profile linter checks still pass.
- Existing negative linter fixtures still fail as expected.
- Documentation clearly tells users where to set TrenchBroom tool paths.
- Focused CTest group passes or skips optional external compiler coverage with return code `77`.

## Do not do

- Do not add Intel macOS support.
- Do not rely on Rosetta.
- Do not change the output convention away from `maps/compiled/`.
- Do not restore `${WORK_DIR_PATH}` in profile `workdir`.
- Do not introduce `${MAP_FULL_PATH}`.
- Do not commit absolute local paths.
- Do not remove package-local shims.
- Do not make external compilers mandatory for default CI.
- Do not modify source `.map` files during VHLT compile; keep work-map rewrites isolated.
- Do not merge into `main` from Codex unless explicitly instructed.

## Suggested Codex prompt

```text
On branch mac-trenchbroom in JTM-rootstorm/Project-Stellar, perform the final sanitization fixes before merging to main.

Important platform scope: macOS means Apple Silicon only (Darwin arm64/aarch64). Do not add Intel macOS or Rosetta support. Linux x86_64 remains supported.

Tasks:
1. Fix linked TrenchBroom package repo-root resolution.
   - In tools/trenchbroom/Stellar/bin/stellar_tb_env.sh, resolve package/root paths physically with cd -P / pwd -P so --link installs work without STELLAR_REPO_ROOT.
   - Add/extend tests in tools/trenchbroom/test_stellar_package_paths.sh for --link installs through a path with spaces, with STELLAR_REPO_ROOT unset.

2. Make tools/bsp/compile_trenchbroom_bsp30.sh auto-select VHLT when external host-native tools are configured.
   - Detect STELLAR_VHLT_DIR and/or HLCSG/HLBSP for fast compile auto-selection.
   - Preserve explicit single compiler priority: STELLAR_BSP30_COMPILER, then QBSP, then VHLT, then PATH single compilers.
   - Add a fake-repo/fake-wrapper test so no real VHLT or BSP output is required.

3. Fix host executable architecture checks for Apple Silicon.
   - Accept arm64 Mach-O and universal Mach-O that includes arm64.
   - Reject x86_64-only Mach-O on Apple Silicon.
   - Reject ELF on Darwin.
   - Keep Linux x86_64 behavior.
   - Patch duplicated host_tool_is_executable logic in compile_trenchbroom_bsp30.sh, compile_vhlt_bsp30.sh, and test_stellar_package_paths.sh, or extract a tiny shared helper if that stays clean.
   - Do not add Intel macOS support.

4. Tighten docs in tools/trenchbroom/Stellar/README.md and docs/TrenchBroom.md.
   - Make game package path and compile tool path setup explicit.
   - Clarify --copy writes .stellar_repo_root.
   - Clarify --link resolves the physical checkout path after the fix.
   - Keep the macOS terminal-launch note for shell exports.

Validation:
- python3 tools/trenchbroom/lint_stellar_compilation_profiles.py --game-config tools/trenchbroom/Stellar/GameConfig.cfg --profiles tools/trenchbroom/Stellar/CompilationProfiles.cfg
- bash tools/trenchbroom/test_stellar_package_paths.sh
- python3 tools/bsp/test_normalize_vhlt_light_angles.py
- python3 tools/bsp/test_map_rewrite.py
- ctest --test-dir build-macos -R "trenchbroom|vhlt|bsp_lightmaps|docs_consistency" --output-on-failure

External compiler coverage may skip with return code 77 when host-native tools are unavailable. Do not make external compiler tooling mandatory for default CI.
```
