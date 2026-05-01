# Stellar Engine - Current Branch Handoff

Branch target: `bsp-gameplay-loop`

## Current entry point

Use `docs/ImplementationStatus.md` as the current branch-facing source of truth.

Branch start policy:

- Create this branch from updated `main` after `collision-movement` has merged.
- Do not restart completed collision, movement, Lua scripting, object-collider, BSP migration, or BSP
  hardening work.

Active next scope:

- Gameplay loop expansion over BSP maps.

Archived completed plans:

- Collision, movement, trigger, object-collider, Lua scripting, and removable-complexity work:
  `Plans/Archived/`
- BSP hardening: `Plans/Archived/project_stellar_bsp_hardening_plan/`
- BSP canonical migration: `Plans/Archived/project_stellar_bsp_canonical_plan/`

## Active scope

BSP maps are the canonical playable level format. The next implementation scope is gameplay loop
expansion over BSP maps, building on the existing BSP metadata, server-authoritative runtime world,
client presentation, and sandboxed Lua command path.

Focus areas:

- ECS/entity spawn from BSP metadata.
- Player presentation from authoritative snapshots.
- Sprite, animation, and interaction loop.
- Item pickup and scripted doors/gates using the existing Lua command path.

## Invariants

- BSP remains the canonical playable level format.
- Server authority remains mandatory.
- Lua scripting remains mandatory and sandboxed.
- Runtime collision, movement, triggers, object colliders, and scripting remain backend-neutral.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable through the shared graphics abstraction.
- Do not add Source/VBSP, dynamic rigid bodies, full PBR, client-side gameplay scripting, or retired
  importer functionality unless explicitly requested.
