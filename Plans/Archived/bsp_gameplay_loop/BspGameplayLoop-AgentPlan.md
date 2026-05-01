# Project Stellar — Active BSP Gameplay Loop Plan

This file is the active implementation handoff for branch `bsp-gameplay-loop`. It is derived from
`Plans/ProjectStellar-BSP-GameplayLoop-AgentPlan.md`, which remains the detailed master plan. Agents
should use this file with `docs/ImplementationStatus.md` as the current branch entry point.

Do not commit directly except when explicitly instructed by the user. For this implementation run,
the user requested a commit after each completed phase. Do not push or pull.

## Branch goal

Expand the gameplay loop over BSP maps while preserving server authority, display-free default
tests, mandatory sandboxed Lua scripting, and OpenGL/Vulkan runtime-selectable presentation.

The completed collision, movement, trigger, object-collider, Lua scripting, BSP canonical migration,
BSP rendering, and BSP hardening foundations are prerequisites. Do not restart or replace them.

## Active focus areas

1. ECS/entity spawn from BSP metadata.
2. Player presentation from authoritative snapshots.
3. Sprite, animation, and interaction loop.
4. Item pickup and scripted doors/gates through the existing sandboxed Lua command path.

## Unit convention

The branch gameplay convention is:

```text
1 Stellar gameplay world unit = 1 inch
Y is up
BSP authored coordinates are imported without scale conversion
Player capsule center spawn y should be half the capsule height above the floor
```

Initial defaults should use a 72 inch tall player, 16 inch radius, 64 inch eye height, 18 inch step
height, 0.5 inch skin width, 4 inch ground snap, 160 inches/second walk speed, 1200
inches/second² acceleration, 800 inches/second² gravity, and 2400 inches/second terminal fall speed.

## Phase order

```text
Phase 0 — Active gameplay-loop handoff lock-in
Phase 1 — Inch-based world scale and gameplay tuning
Phase 2 — Procedural developer textures for inch-scale BSP authoring
Phase 3 — Load the configured BSP map into the live client path
Phase 4 — Authoritative player camera drives level rendering
Phase 5 — Minimal ECS/entity spawn from BSP metadata
Phase 6 — Single-room controllable player loop
Phase 7 — First interaction loop: pickup and scripted door/gate
Phase 8 — Final branch hardening and documentation
```

Safe parallelization:

- Phase 1 and Phase 2 may run in parallel after the inch-unit policy is locked.
- Phase 3 may run in parallel with Phase 2 if renderer/application files are not shared.
- Phase 4 waits for Phase 3.
- Phase 5 may begin after Phase 1, but Phase 6 waits for Phases 3, 4, and 5 when practical.
- Phase 7 waits for the controllable room loop from Phase 6.
- Phase 8 runs last.

## Non-goals

- Source/VBSP support.
- Dynamic rigid bodies or third-party physics.
- Full PBR or a model/skinning/animation system.
- Client-side gameplay scripting.
- Renderer-owned or audio-owned gameplay authority.
- Retired importer functionality.
- Moving brush simulation unless explicitly requested later.

## Phase completion requirements

After each implemented phase:

1. Update `docs/ImplementationStatus.md` with completion notes and validation results.
2. Update `docs/Design.md` and `docs/BspAuthoring.md` when the unit policy, live loop, authoring
   conventions, entity spawn, or interaction semantics change.
3. Run the narrowest useful validation from the master plan, plus broader validation when practical.
4. Commit the phase to the active branch without pushing or pulling.
