# Stellar Engine - Next Scope Handoff

Branch target: `socket-transport`

## Current entry point

`docs/ImplementationStatus.md` is the source of truth for branch status. Active socket transport handoffs are:

- `Plans/SocketTransport-AgentPlan.md`
- `Plans/ProjectStellar-SocketTransport-AgentPlan.md`

## Active scope

Socket transport and networked session lifecycle are the active branch target. Phase ST-5 added the headless dedicated server entry point on top of the Linux/POSIX TCP socket transport. The next implementation slice is client connect mode:

`stellar-server + TCP SocketTransport + ClientHello + ServerWelcome + ClientWorldReceiver + NetworkWorldSnapshot presentation`

## Current phase status

- ST-2 — Live client over local networked transport path: completed.
- ST-3 — Connection and session lifecycle: completed.
- ST-4 — Remote socket transport: completed.
- ST-5 — Dedicated server entry point: completed.
- ST-6 — Client connect mode: next.
- ST-7 — Hardening, documentation, validation, and archival: deferred.

## Completed historical scope

Completed plan packages remain archived and must not be restarted:

- BSP gameplay loop: `Plans/Archived/bsp_gameplay_loop/`.
- BSP presentation/networking polish: `Plans/Archived/bsp_presentation_networking_polish/`.

Do not restart completed collision, movement, Lua scripting, object-collider, BSP migration, BSP hardening, BSP gameplay-loop foundation work, or BSP presentation/networking polish work.

## Invariants

- BSP remains the canonical playable level format.
- Server authority remains mandatory.
- Lua scripting remains mandatory, sandboxed, and server-authoritative.
- Import never executes scripts.
- Runtime collision, movement, triggers, object colliders, scripting, and networking contracts remain backend-neutral.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable through the shared graphics abstraction.
- Rendering, audio, HUD, and UI are presentation only and never sources of gameplay truth.
- No client prediction or reconciliation is active for ST-5/ST-6.
- Do not add Source/VBSP, dynamic rigid bodies, moving brush simulation, full PBR, client-side gameplay scripting, model/animation systems, or retired importer functionality unless explicitly requested.
