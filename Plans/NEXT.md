# Stellar Engine — Current Branch Handoff

Branch target: `bsp-integration`

## Current entry point

Use `docs/ImplementationStatus.md` as the current branch-facing source of truth. The active
implementation plan is
`Plans/project_stellar_bsp_canonical_plan/00-MASTER-KILO-BSP-CANONICAL-PLAN.md`.

## Active scope

The current active scope is migration to BSP maps as the canonical playable level format and removal
of retired pre-BSP importer functionality from active code, active build configuration, active tests,
and active docs.

Older collision/pre-BSP importer phase plans are historical context only and must not be restarted as
active work.

## Invariants

- BSP is the canonical level source.
- Lua scripting remains mandatory server-authoritative infrastructure.
- Server authority, runtime collision state, triggers, object-collider sensors, and script command
  validation remain native/server-owned.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable presentation backends.
- No third-party physics, dynamic rigid bodies, Source/VBSP, full PBR, model/animation systems, or
  retired pre-BSP importer functionality are part of this migration.

## NEXT.md lifecycle

After the BSP migration is complete, archive the detailed BSP plan under `Plans/Archived/` and
rewrite this file to the next active post-BSP scope. Do not leave this file pointing at completed
migration work.
