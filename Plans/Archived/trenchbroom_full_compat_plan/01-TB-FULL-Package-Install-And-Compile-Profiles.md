# TB-FULL-01 — Package Install and Compile Profile Hardening

Target branch: `trenchbroom-compat`

## Goal

Make the Stellar TrenchBroom game package work in both supported install modes:

1. **Repo-local mode:** user adds `tools/trenchbroom/Stellar/` directly from the checkout.
2. **Copied-package mode:** user copies `tools/trenchbroom/Stellar/` into TrenchBroom's user games directory and points it at the repository via configuration or environment.

This phase must remove path fragility, missing package assets, and compile-profile quoting issues.

## Key files to inspect first

- `tools/trenchbroom/Stellar/GameConfig.cfg`
- `tools/trenchbroom/Stellar/CompilationProfiles.cfg`
- `tools/trenchbroom/Stellar/README.md`
- `tools/bsp/compile_trenchbroom_bsp30.sh`
- `tools/bsp/compile_vhlt_bsp30.sh`
- `tools/bsp/validate_trenchbroom_bsp30.sh`
- `docs/TrenchBroom.md`
- `tests/fixtures/trenchbroom/README.md`

## Tasks

### TB-FULL-01.1 — Add missing package icon

Implement:

- Add `tools/trenchbroom/Stellar/Icon.png`.
- Keep it small, deterministic, and repository-owned.
- Do not use proprietary fonts or copied external artwork.
- Prefer a generated simple icon checked into the repo.

Acceptance:

- `GameConfig.cfg` references an existing file.
- Package can load without missing icon warnings.

### TB-FULL-01.2 — Add package-local tool shims

Problem: current compile tools in `GameConfig.cfg` point from the package to repo-root scripts. This breaks or becomes ambiguous when the package is copied into TrenchBroom's game directory.

Implement package-local wrappers:

- `tools/trenchbroom/Stellar/bin/stellar_tb_compile.sh`
- `tools/trenchbroom/Stellar/bin/stellar_tb_validate.sh`
- Optional helper library: `tools/trenchbroom/Stellar/bin/stellar_tb_env.sh`

Wrapper behavior:

1. Resolve repository root in this priority order:
   - `STELLAR_REPO_ROOT` environment variable.
   - A package-local text file such as `.stellar_repo_root` created by an install helper.
   - Walking upward from the wrapper path when package is repo-local.
   - Current working directory fallback only if it contains expected repo files.
2. Validate that resolved root contains:
   - `tools/bsp/compile_trenchbroom_bsp30.sh`
   - `tools/bsp/validate_trenchbroom_bsp30.sh`
   - `CMakeLists.txt`
3. Call the repo-root wrapper with fully quoted paths.
4. Pass through `STELLAR_CLIENT`, `STELLAR_SERVER`, `STELLAR_BSP30_TOOLCHAIN`, `STELLAR_BSP30_COMPILER`, `QBSP`, `STELLAR_VHLT_DIR`, `HLCSG`, `HLBSP`, `HLVIS`, `HLRAD`, and `RIPENT`.
5. Print actionable diagnostics when root or tools are missing.

Acceptance:

- Repo-local package wrapper works.
- Copied package wrapper works with `STELLAR_REPO_ROOT`.
- Wrapper handles spaces in repo path and map path.
- Wrapper exits nonzero with a clear message if root cannot be found.

### TB-FULL-01.3 — Harden `GameConfig.cfg`

Implement:

- Change `compilationTools` to call package-local tool shims rather than repo-root scripts directly.
- Ensure `materials.root` resolves in both repo-local and copied-package mode.
- If TrenchBroom requires paths relative to the package, keep material assets package-local.
- Keep `mapsource` and `mapoutput` sane for repo-local use.
- If copied-package mode cannot safely infer repo paths for `mapsource`/`mapoutput`, document `STELLAR_REPO_ROOT` and rely on compile profile output paths.

Acceptance:

- TrenchBroom can select the Stellar profile without path errors in both install modes.
- Materials are visible/selectable in the material browser.
- Compile/validate profiles invoke the package shims.

### TB-FULL-01.4 — Quote all compile profile parameters

Problem: unquoted `${MAP_FULL_PATH}` or output paths break when project directories contain spaces.

Implement:

- Update `CompilationProfiles.cfg` parameters with robust quoting supported by TrenchBroom.
- Test paths with spaces using a copied fixture path or shell-level smoke test.
- If TrenchBroom's profile format has strict quoting behavior, document the exact supported form and add a regression fixture for it.

Acceptance:

- Compile and validate profiles work when the checkout path or map path contains spaces.
- Wrapper tests cover this without requiring a GUI.

### TB-FULL-01.5 — Add install helper

Implement:

- `tools/trenchbroom/install_stellar_game_package.sh`

Behavior:

- Accept `--repo-root`, `--dest`, and `--copy` or `--link` mode.
- Copy or symlink `tools/trenchbroom/Stellar` into the selected destination.
- Write package-local `.stellar_repo_root` when copying.
- Validate destination contains package config, FGD, materials, WAD, icon, and shims.
- Print next manual steps for TrenchBroom preferences.

Acceptance:

- Running the helper produces a usable package directory.
- No generated absolute paths are committed.
- Docs show both manual and helper-based install.

### TB-FULL-01.6 — Add package path tests

Implement display-free tests/scripts:

- `tools/trenchbroom/Stellar/bin/stellar_tb_compile.sh --help` works from repo-local package.
- Same wrapper works from a temporary copied package when `STELLAR_REPO_ROOT` points at the checkout.
- Same wrapper fails clearly when root is absent.
- `bash -n` runs on all package scripts.
- Optional: a lightweight parser checks `GameConfig.cfg` and `CompilationProfiles.cfg` for required file references.

Acceptance commands:

```bash
bash -n tools/trenchbroom/Stellar/bin/stellar_tb_compile.sh
bash -n tools/trenchbroom/Stellar/bin/stellar_tb_validate.sh
bash -n tools/trenchbroom/install_stellar_game_package.sh
ctest --test-dir build -R 'trenchbroom|bsp_authoring|compile_wrapper' --output-on-failure
```

## Documentation updates

Update:

- `tools/trenchbroom/Stellar/README.md`
- `docs/TrenchBroom.md`
- `tests/fixtures/trenchbroom/README.md`

Required content:

- Repo-local install path.
- Copied-package install path.
- Required env vars for copied package mode.
- Expected package files.
- Troubleshooting for missing repo root, spaces in paths, missing VHLT tools, missing built client/server binaries.

## Phase done checklist

- [ ] `Icon.png` exists.
- [ ] Package-local compile/validate shims exist and are documented.
- [ ] `GameConfig.cfg` no longer relies on fragile direct repo-root compile tool paths.
- [ ] Compile profile arguments are safely quoted.
- [ ] Copy install mode is supported and tested.
- [ ] Docs include manual and helper install paths.
