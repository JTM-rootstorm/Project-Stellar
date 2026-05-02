# Stellar Engine - Next Scope Handoff

Branch target: `trenchbroom-compat`

## Current entry point

`docs/ImplementationStatus.md` is the source of truth for branch status. The active scope is
TrenchBroom BSP30 compatibility and the Z-up migration. The current phase package is:

- `Plans/TrenchBroomCompat-AgentPlan.md`
- `Plans/trenchbroom_compat_plans/00-MASTER-TrenchBroomCompat-AgentPlan.md`

## Active scope

The branch is converting active runtime and authoring conventions from Y-up to Z-up while preserving
the 1 Stellar gameplay unit = 1 authored inch policy. The active TrenchBroom workflow targets
BSP30-authored maps with imported coordinates preserved 1:1.

Current branch goals:

- Convert active runtime, movement, collision, presentation, and BSP fixture assumptions to Z-up.
- Target TrenchBroom-authored BSP30 maps as the primary authoring workflow.
- Fully remove the prototype cube renderer/debug cube fallback so no-map and remote-without-local-map
  presentation uses a blank/static-less fallback.
- Add a TrenchBroom game config, FGD, material/editor assets, BSP30 compile wrapper, and validation
  fixtures.
- Preserve server authority, mandatory sandboxed Lua, BSP canonical runtime, and display-free
  validation.

## Active phase order

1. Phase 0 — branch, handoff, and docs baseline.
2. Phase 0.5 — remove prototype cube renderer and switch no-map rendering to a static-less fallback.
3. Phase 1 — central Z-up world-axis contract.
4. Phase 2 — Z-up runtime, collision, movement, scripting metadata, and fixtures.
5. Phase 3 — Z-up presentation, camera, input mapping, snapshots, and network-adjacent tests.
6. Phase 4 — BSP30 import/validation lockdown.
7. Phase 5 — TrenchBroom package, FGD, material/editor assets, and compile wrappers.
8. Phase 6 — exported map fixtures and end-to-end validation.
9. Phase 7 — final documentation, audits, archival, and handoff.

Parallelization guidance: Phase 0.5 should complete before Z-up conversion. Phases 1, 2, and 3 are
tightly coupled and must share the central axis contract. Phases 4 and 5 can proceed after Phase 0/0.5
if they avoid committing old Y-up examples. Phase 6 depends on Phases 2, 4, and 5. Phase 7 is last.

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

## Post-TrenchBroom follow-up options

After `trenchbroom-compat` is complete, pick one focused follow-up before implementation starts:

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
