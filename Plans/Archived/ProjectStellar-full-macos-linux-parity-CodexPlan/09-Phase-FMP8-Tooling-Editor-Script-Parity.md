---
phase: FMP-8
title: Tooling, Editor, and Script Parity
depends_on: [FMP-1]
can_parallelize: true
---

# FMP-8 — Tooling, Editor, and Script Parity

## Goal

Ensure developer workflows used by Linux validation are portable to macOS or explicitly marked optional.

## Tasks

1. Run shell syntax checks on macOS:
   ```bash
   bash -n tools/bsp/*.sh
   bash -n tools/trenchbroom/Stellar/bin/*.sh
   ```
2. Audit scripts for Linux-only assumptions such as `readlink -f`, GNU-only flags, `nproc`, `/proc`, hardcoded `.so`, and Linux-only compiler names.
3. Replace with portable helpers.
4. Verify TrenchBroom package-local shims work on macOS.
5. If external BSP compiler/VHLT is Linux-only, skip clearly with code `77`.
6. Confirm generated WAVs are byte-stable on macOS/Python.
7. Confirm Doxygen/docs consistency scripts work on macOS.
8. Document Homebrew dependencies and optional tools.

## Acceptance criteria

- All repo-owned scripts used by default validation run on macOS.
- Optional external tool gaps skip clearly.
- Generated audio/assets are stable across platforms.
- TrenchBroom authoring docs include macOS setup notes.

## Validation

```bash
bash -n tools/bsp/compile_trenchbroom_bsp30.sh
bash -n tools/bsp/compile_vhlt_bsp30.sh
bash -n tools/bsp/validate_trenchbroom_bsp30.sh
bash -n tools/bsp/run_vhlt_fixture_matrix.sh
bash -n tools/trenchbroom/Stellar/bin/stellar_tb_compile.sh
bash -n tools/trenchbroom/Stellar/bin/stellar_tb_validate.sh
python3 tools/audio/generate_retro_footsteps.py --out /tmp/stellar-footsteps-macos
python3 tools/docs/check_docs_consistency.py
ctest --test-dir build-macos-metal -R '^(trenchbroom|bsp_)' --output-on-failure
```
