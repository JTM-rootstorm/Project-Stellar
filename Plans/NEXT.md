# Stellar Engine - Next Scope Handoff

Branch target: `bsp-gameplay-loop`

## Current entry point

`docs/ImplementationStatus.md` is the source of truth for completed branch status. The BSP
presentation/networking polish plan package is complete and archived at
`Plans/Archived/bsp_presentation_networking_polish/`.

## Completed historical scope

Completed plan packages:

- BSP gameplay loop: `Plans/Archived/bsp_gameplay_loop/`.
- BSP presentation/networking polish: `Plans/Archived/bsp_presentation_networking_polish/`.

Do not restart completed collision, movement, Lua scripting, object-collider, BSP migration, BSP
hardening, BSP gameplay-loop foundation work, or BSP presentation/networking polish work.

## Recommended Next Options

BSP presentation/networking polish is complete and archived. Select one next scope explicitly before
implementation starts:

- Remote socket transport and real multiplayer connection/session lifecycle over the existing
  remote-ready contracts.
- Client interpolation, prediction, and reconciliation against authoritative snapshots.
- Sprite atlas packing and sprite sheet animation for richer billboard presentation.
- Richer HUD rendering, UI, inventory presentation, and VFX over server-approved events.
- miniaudio-backed playback, local audio asset loading, and spatial audio/listener updates.
- BSP editor/toolchain polish, including automated remapping from editor-facing FGD fields to dotted
  Stellar BSP keys.

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
  gameplay scripting, model/animation systems, or retired importer functionality unless explicitly
  requested.
