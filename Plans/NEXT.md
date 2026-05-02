# Stellar Engine - Next Scope Handoff

Branch target: `trenchbroom-compat`

## Current entry point

`docs/ImplementationStatus.md` is the source of truth for branch status. The active scope is
complete TrenchBroom BSP30 compatibility and the Z-up migration. The completed phase package is
archived under:

- `Plans/Archived/trenchbroom_compat/TrenchBroomCompat-AgentPlan.md`
- `Plans/Archived/trenchbroom_compat/trenchbroom_compat_plans/00-MASTER-TrenchBroomCompat-AgentPlan.md`

## Completed trenchbroom-compat scope

The branch now uses Z-up runtime and authoring conventions while preserving the 1 Stellar gameplay
unit = 1 authored inch policy. The supported TrenchBroom workflow targets BSP30-authored maps with
imported coordinates preserved 1:1.

Completed branch outcomes:

- Runtime, movement, collision, presentation, and BSP fixture assumptions use Z-up.
- TrenchBroom-authored BSP30 maps are the primary authoring workflow.
- No-map and remote-without-local-map presentation use a blank/static-less fallback.
- The repo includes a TrenchBroom game config, FGD aliases, developer material references, BSP30
  compile/validation wrappers, source-map fixtures, and generated BSP30 test fixtures.
- Preserve server authority, mandatory sandboxed Lua, BSP canonical runtime, and display-free
  validation.

## Completed phase status

- Phase 0 — branch, handoff, and docs baseline: completed.
- Phase 0.5 — prototype cube renderer removal and static-less no-map fallback: completed.
- Phase 1 — central Z-up world-axis contract: completed.
- Phase 2 — Z-up runtime, collision, movement, scripting metadata, and fixtures: completed.
- Phase 3 — Z-up presentation, camera, input mapping, snapshots, and network-adjacent tests:
  completed.
- Phase 4 — BSP30 import/validation lockdown: completed.
- Phase 5 — TrenchBroom package, FGD, material/editor assets, and compile wrappers: completed.
- Phase 6 — exported map fixtures and end-to-end validation: completed.
- Phase 7 — final documentation, audits, archival, and handoff: completed.

## Deferred post-socket options

Socket transport and networked session lifecycle are complete and remain deferred follow-up context,
not active work. Completed socket transport handoffs are archived under:

- `Plans/Archived/socket_transport/SocketTransport-AgentPlan.md`
- `Plans/Archived/socket_transport/ProjectStellar-SocketTransport-AgentPlan.md`
- `Plans/Archived/socket_transport/kilo_socket_transport_plans/`

## Completed socket-transport scope

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

## Next follow-up options

Pick one focused follow-up before implementation starts:

- Richer map editor workflow polish, including editor-visible WAD/material preview generation.
- Room/portal semantics beyond sealed brush rooms.
- Map distribution/caching or remote presentation-map workflow.
- Client interpolation/presentation smoothing over authoritative snapshots.
- Client prediction/reconciliation, explicitly scoped against server authority.
- True multi-player simulation beyond the current single-client/single-active-player limit.
- UDP/unreliable transport or transport-selection work.
- Richer HUD/UI/VFX presentation.
- miniaudio-backed playback integration for server-approved events.
- Sprite atlas/sheet animation.
- Moving brush simulation only if explicitly selected later.

## Completed historical scope

Completed plan packages remain archived and must not be restarted:

- BSP gameplay loop: `Plans/Archived/bsp_gameplay_loop/`.
- BSP presentation/networking polish: `Plans/Archived/bsp_presentation_networking_polish/`.
- Socket transport: `Plans/Archived/socket_transport/`.
- TrenchBroom BSP30 compatibility and Z-up migration: `Plans/Archived/trenchbroom_compat/`.

Do not restart completed collision, movement, Lua scripting, object-collider, BSP migration, BSP hardening, BSP gameplay-loop foundation work, BSP presentation/networking polish work, or socket transport/session lifecycle work.

## Invariants

- BSP remains the canonical playable level format.
- The active world-axis convention is Z-up.
- TrenchBroom authoring targets BSP30, with 1 editor unit = 1 gameplay inch.
- Default player spawn centers are authored 36 inches above the floor for the default capsule.
- Server authority remains mandatory.
- Lua scripting remains mandatory, sandboxed, and server-authoritative.
- Import never executes scripts.
- Runtime collision, movement, triggers, object colliders, scripting, and networking contracts remain backend-neutral.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable through the shared graphics abstraction.
- Rendering, audio, HUD, and UI are presentation only and never sources of gameplay truth.
- No client prediction, interpolation, map transfer, or reconciliation is active in the completed ST scope.
- Do not add Source/VBSP, dynamic rigid bodies, moving brush simulation, full PBR, client-side gameplay scripting, model/animation systems, or retired importer functionality unless explicitly requested.
