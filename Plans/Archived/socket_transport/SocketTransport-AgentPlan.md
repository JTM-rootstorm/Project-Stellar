# Stellar Engine - Socket Transport Active Handoff

Branch target: `socket-transport`

## Active scope

Implement socket transport and networked session lifecycle over the existing transport-neutral contracts. Phase ST-2 moves local mapped client play onto `LoopbackTransportPair + LocalServerBridge + ClientWorldReceiver` before remote sockets are added.

## Phase status

- ST-2 — Live client over local networked transport path: active/completed in this slice.
- ST-3 — Connection and session lifecycle: next.
- ST-4 — Remote socket transport: deferred until ST-3.
- ST-5 — Dedicated server entry point: deferred.
- ST-6 — Client connect mode: deferred.
- ST-7 — Hardening, documentation, validation, archive: deferred.

## Invariants

- Server authority remains mandatory; clients render authoritative snapshots only.
- Lua scripting remains mandatory, sandboxed, and server-authoritative.
- BSP remains the canonical playable map format.
- Default tests remain display-free.
- No client prediction or reconciliation is in ST-2.
- Completed BSP gameplay-loop and BSP presentation/networking polish scopes remain archived and must not be restarted.
