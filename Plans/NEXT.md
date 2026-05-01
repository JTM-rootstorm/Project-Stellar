# Stellar Engine - Next Scope Handoff

Branch target: `socket-transport`

## Current entry point

`docs/ImplementationStatus.md` is the source of truth for branch status. Socket transport and networked session lifecycle are complete. Completed socket transport handoffs are archived under:

- `Plans/Archived/socket_transport/SocketTransport-AgentPlan.md`
- `Plans/Archived/socket_transport/ProjectStellar-SocketTransport-AgentPlan.md`
- `Plans/Archived/socket_transport/kilo_socket_transport_plans/`

## Completed scope

The branch now has:

`stellar-server + stellar-client --connect HOST:PORT + Linux/POSIX TCP SocketTransport + ClientHello + ServerWelcome + ClientWorldReceiver + NetworkWorldSnapshot presentation`

Implemented limits remain explicit: one accepted TCP client, one active authoritative player slot, remote dynamic/fallback rendering only, and no client prediction, interpolation, reconciliation, map transfer, UDP, authentication, encryption, matchmaking, or public Internet deployment.

## Completed phase status

- ST-2 — Live client over local networked transport path: completed.
- ST-3 — Connection and session lifecycle: completed.
- ST-4 — Remote socket transport: completed.
- ST-5 — Dedicated server entry point: completed.
- ST-6 — Client connect mode: completed.
- ST-7 — Hardening, documentation, validation, and archival: completed.

## Post-ST next options

Pick one focused follow-up before implementation starts:

- Client interpolation/presentation smoothing over authoritative snapshots.
- Client prediction/reconciliation, explicitly scoped against server authority.
- True multi-player simulation beyond the current single-client/single-active-player limit.
- UDP/unreliable transport or transport-selection work.
- Map distribution/caching or remote presentation-map workflow.
- Richer HUD/UI/VFX presentation.
- miniaudio-backed playback integration for server-approved events.
- Sprite atlas/sheet animation.

## Completed historical scope

Completed plan packages remain archived and must not be restarted:

- BSP gameplay loop: `Plans/Archived/bsp_gameplay_loop/`.
- BSP presentation/networking polish: `Plans/Archived/bsp_presentation_networking_polish/`.
- Socket transport: `Plans/Archived/socket_transport/`.

Do not restart completed collision, movement, Lua scripting, object-collider, BSP migration, BSP hardening, BSP gameplay-loop foundation work, BSP presentation/networking polish work, or socket transport/session lifecycle work.

## Invariants

- BSP remains the canonical playable level format.
- Server authority remains mandatory.
- Lua scripting remains mandatory, sandboxed, and server-authoritative.
- Import never executes scripts.
- Runtime collision, movement, triggers, object colliders, scripting, and networking contracts remain backend-neutral.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable through the shared graphics abstraction.
- Rendering, audio, HUD, and UI are presentation only and never sources of gameplay truth.
- No client prediction, interpolation, map transfer, or reconciliation is active in the completed ST scope.
- Do not add Source/VBSP, dynamic rigid bodies, moving brush simulation, full PBR, client-side gameplay scripting, model/animation systems, or retired importer functionality unless explicitly requested.
