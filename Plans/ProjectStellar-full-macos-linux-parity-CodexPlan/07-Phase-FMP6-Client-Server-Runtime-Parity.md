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
```
