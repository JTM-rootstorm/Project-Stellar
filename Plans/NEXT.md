# Stellar Engine - Next Scope Handoff

Branch target: `bsp-gameplay-loop`

## Current entry point

Use `Plans/BspPresentationNetworkingPolish-AgentPlan.md` for the concise active handoff and
`Plans/ProjectStellar-BSP-PresentationNetworkingPolish-AgentPlan.md` for the detailed implementation
plan.

## Completed historical scope

The previous BSP gameplay-loop plan package is complete and archived at:
`Plans/Archived/bsp_gameplay_loop/`

Do not restart completed collision, movement, Lua scripting, object-collider, BSP migration, BSP
hardening, or BSP gameplay-loop foundation work.

## Active recommended scope

Presentation and networking polish over the completed BSP gameplay loop.

First slices:

- Live scripted authoritative runtime integration for the client loop.
- Gameplay snapshot presentation for sprites, pickups, and door/gate state.
- Snapshot/delta/event contracts.
- Local/remote transport bridge.
- HUD/audio/toolchain polish.

## Invariants

- BSP remains the canonical playable level format.
- Server authority remains mandatory.
- Lua scripting remains mandatory, sandboxed, and server-authoritative.
- Import never executes scripts.
- Runtime collision, movement, triggers, object colliders, and scripting remain backend-neutral.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable through the shared graphics abstraction.
- Rendering, audio, HUD, and UI are presentation only and never sources of gameplay truth.
- Do not add Source/VBSP, dynamic rigid bodies, moving brush simulation, full PBR, client-side
  gameplay scripting, or retired importer functionality unless explicitly requested.
