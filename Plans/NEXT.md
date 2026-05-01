# Stellar Engine - Next Scope Handoff

Branch target: `bsp-gameplay-loop`

## Current entry point

Use `docs/ImplementationStatus.md` as the completed `bsp-gameplay-loop` branch source of truth.
The gameplay-loop phase handoffs are retained for history at `Plans/BspGameplayLoop-AgentPlan.md` and
`Plans/ProjectStellar-BSP-GameplayLoop-AgentPlan.md`; do not treat them as the next active scope.

Branch start policy:

- Create this branch from updated `main` after `collision-movement` has merged.
- Do not restart completed collision, movement, Lua scripting, object-collider, BSP migration, or BSP
  hardening work.

Completed branch scope:

- Gameplay loop expansion over BSP maps.
- Final status: Phases 0-8 complete as recorded in `docs/ImplementationStatus.md`.

Recommended next scope:

- Presentation and networking polish over the completed BSP gameplay loop.
- Suggested first slices: server snapshot/delta expansion, remote transport integration, sprite and
  interaction presentation polish, UI/HUD or inventory feedback for pickups, audio/event presentation,
  and BSP editor/toolchain workflow polish.

Archived completed plans:

- Collision, movement, trigger, object-collider, Lua scripting, and removable-complexity work:
  `Plans/Archived/`
- BSP hardening: `Plans/Archived/project_stellar_bsp_hardening_plan/`
- BSP canonical migration: `Plans/Archived/project_stellar_bsp_canonical_plan/`

## Completed scope

BSP maps are the canonical playable level format. The completed branch added gameplay-loop expansion
over BSP maps, building on existing BSP metadata, server-authoritative runtime world, client
presentation, and the sandboxed Lua command path.

Completed focus areas:

- ECS/entity spawn from BSP metadata.
- Player presentation from authoritative snapshots.
- Sprite, animation, and interaction loop.
- Item pickup and scripted doors/gates using the existing Lua command path.

Branch gameplay units are now explicitly locked for implementation planning: 1 Stellar gameplay
world unit equals 1 inch, Y is up, and BSP authored coordinates import without scale conversion.

## Invariants

- BSP remains the canonical playable level format.
- Server authority remains mandatory.
- Lua scripting remains mandatory and sandboxed.
- Runtime collision, movement, triggers, object colliders, and scripting remain backend-neutral.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable through the shared graphics abstraction.
- Do not add Source/VBSP, dynamic rigid bodies, full PBR, client-side gameplay scripting, or retired
  importer functionality unless explicitly requested.
