---
phase: FMP-6
title: Client/Server Runtime Parity on macOS
depends_on: [FMP-1, FMP-5]
can_parallelize: true
---

# FMP-6 — Client/Server Runtime Parity on macOS

## Goal

Prove runtime modes behave on macOS the same way they behave on Linux.

## Runtime modes to validate

1. No-map client fallback.
2. `stellar-client --validate-config`.
3. `stellar-client --validate-map <fixture>`.
4. Single-player: `stellar-client --map <fixture> --renderer metal`.
5. Listen host.
6. Remote client connected to dedicated/listen server.
7. Dedicated server.

## Tasks

1. Add a macOS runtime smoke script that can run locally and in CI when display is available.
2. Keep network/server tests display-free where possible.
3. Validate macOS socket fixes under actual server/client loopback.
4. Verify relative mouse capture does not cause startup failure on macOS.
5. Verify asset paths work from repo root, build directory, and optional app bundle/current working directory.
6. Verify renderer choice does not affect server-authoritative simulation.

## Acceptance criteria

- Dedicated server and network tests pass on macOS.
- Single-player/listen/remote smoke is documented and passes locally.
- Renderer choice does not affect authority outputs.
- No macOS runtime failure caused by input capture, socket behavior, or asset path lookup.

## Validation

```bash
ctest --test-dir build-macos-metal -R '^(loopback_transport|transport_loopback|transport_socket|socket_transport|network_session|server_runtime|dedicated_server|listen_server_host|client_single_player_runtime|client_connect|client_world_receiver|client_map_validation_smoke|client_cli_validate_map)$' --output-on-failure
tools/ci/run_macos_runtime_smoke.sh
STELLAR_FORCE_DISPLAY_SMOKE=1 tools/ci/run_macos_runtime_smoke.sh
STELLAR_RUNTIME_BUILD_DIR=build-macos-metal-only tools/ci/run_macos_runtime_smoke.sh
STELLAR_RUNTIME_BUILD_DIR=build-macos-metal-only STELLAR_FORCE_DISPLAY_SMOKE=1 tools/ci/run_macos_runtime_smoke.sh
```

2026-05-06 display-attached macOS validation:

- `cmake --preset macos-metal` passed.
- `cmake --build --preset macos-metal` passed.
- `cmake --preset macos-metal-only` passed.
- `cmake --build --preset macos-metal-only` passed.
- `ctest --test-dir build-macos-metal-only --output-on-failure` completed 104 tests with 0 failures
  and expected skips for `metal_context_smoke`, `trenchbroom_vhlt_fixture_matrix`, and
  `bsp_authoring_smoke_compile_wrapper`.
- `build-macos-metal/stellar-client --validate-display --renderer metal` passed.
- `build-macos-metal-only/stellar-client --validate-display --renderer metal` passed.
- `tools/ci/run_macos_runtime_smoke.sh` passed its display-free runtime checks and skipped
  single-player display smoke because the script display detector did not report an attached
  display.
- `STELLAR_FORCE_DISPLAY_SMOKE=1 tools/ci/run_macos_runtime_smoke.sh` passed with 0 skips and ran
  the single-player Metal display smoke for 5 seconds.
- `STELLAR_RUNTIME_BUILD_DIR=build-macos-metal-only tools/ci/run_macos_runtime_smoke.sh` passed its
  display-free runtime checks and skipped single-player display smoke for the same detector reason.
- `STELLAR_RUNTIME_BUILD_DIR=build-macos-metal-only STELLAR_FORCE_DISPLAY_SMOKE=1
  tools/ci/run_macos_runtime_smoke.sh` passed with 0 skips and ran the single-player Metal display
  smoke for 5 seconds.
- `STELLAR_RUN_METAL_CONTEXT_TESTS=1 ctest --test-dir build-macos-metal -R
  '^metal_context_smoke$' --output-on-failure` passed 1/1.
- `STELLAR_RUN_METAL_CONTEXT_TESTS=1 ctest --test-dir build-macos-metal-only -R
  '^metal_context_smoke$' --output-on-failure` passed 1/1.

The forced display smokes logged 2560x1440 Metal drawables with matching layer drawable, depth
texture, and viewport sizes in both Metal-enabled build trees.
