# Project Stellar — Socket Transport Agent Plan

Branch target: `socket-transport`

## Goal

Move Project Stellar from in-memory local presentation networking toward real remote socket play while preserving server authority, sandboxed Lua scripting, BSP canonical playable maps, backend-neutral runtime contracts, and display-free default tests.

## Source state

The BSP gameplay-loop package is archived under `Plans/Archived/bsp_gameplay_loop/`. The BSP presentation/networking polish package is archived under `Plans/Archived/bsp_presentation_networking_polish/`. The current networking foundation already includes transport-neutral packets, loopback endpoints, `NetworkPlayerCommand`, `NetworkWorldSnapshot`, `GameplayEvent`, snapshot codec/delta support, `LocalServerBridge`, and `ClientWorldReceiver`.

## Phase sequence

1. **ST-2 — Live client over local networked transport path.** Local mapped play sends input through loopback transport, pumps the authoritative server bridge, drains `ClientWorldReceiver`, and renders from the latest authoritative `NetworkWorldSnapshot`.
2. **ST-3 — Connection and session lifecycle.** Add deterministic hello/welcome/session state, protocol/map mismatch handling, disconnect and timeout diagnostics.
3. **ST-4 — Remote socket transport.** Add Linux-first localhost/LAN socket transport behind the existing `ClientTransport`/`ServerTransport` seam.
4. **ST-5 — Dedicated server entry point.** Add a server executable that validates/hosts a BSP map through authoritative runtime and transport session state.
5. **ST-6 — Client connect mode.** Add client CLI/config path for remote connect mode that renders from authoritative network snapshots.
6. **ST-7 — Hardening and archive.** Run final validation, update docs, and archive active socket-transport plans.

## ST-2 implementation notes

- `NetworkedClientRuntime` owns local loopback endpoints, `LocalServerBridge`, `ClientWorldReceiver`, and monotonic command sequencing.
- `PreparedApplicationRuntime` creates `networked_runtime` for mapped play by default; `LocalLoopbackRuntime` remains available for low-level tests/fallback.
- Player and gameplay presentation helpers consume `network::NetworkWorldSnapshot` without GPU handles or gameplay authority.
- Live mapped rendering uses authoritative network snapshots. No-map debug fallback remains unchanged.

## Deferred

Remote sockets, connection handshake/session lifecycle, dedicated server target, client connect mode, prediction, reconciliation, snapshot interpolation, public Internet deployment, authentication, encryption, matchmaking, and true simultaneous multi-player gameplay are deferred beyond ST-2.

## Validation policy

Each phase must run focused display-free tests plus full default `ctest --test-dir build --output-on-failure` before being marked complete. Remote/socket phases may use localhost only and must not require external network access.
