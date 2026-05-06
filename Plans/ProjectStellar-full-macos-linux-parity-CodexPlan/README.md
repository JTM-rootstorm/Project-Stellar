# Project Stellar full macOS/Linux parity Codex plan

Start with:

1. `00-MASTER-FullMacOSLinuxParity-CodexPlan.md`
2. Phase files in numeric order.
3. `11-Codex-Task-Cards.md` for parallel assignment.
4. `12-FMP-Acceptance-Matrix.md` for current FMP-0 acceptance status.

This bundle assumes the current `macos-compat` branch has an initial Metal backend but not full OpenGL material parity.

Build matrix presets:

```bash
cmake --preset linux-default
cmake --build --preset linux-default
ctest --preset linux-default

cmake --preset macos-default
cmake --build --preset macos-default
ctest --preset macos-default

cmake --preset macos-metal
cmake --build --preset macos-metal
ctest --preset macos-metal

cmake --preset macos-metal-only
cmake --build --preset macos-metal-only
ctest --preset macos-metal-only
```

The macOS Metal presets are Apple-only because `STELLAR_ENABLE_METAL=ON` is intentionally fatal on
non-Apple platforms.
