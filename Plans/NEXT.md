# Stellar Engine — Current Branch Handoff

Branch target: `bsp-integration`

## Current entry point

Use `docs/ImplementationStatus.md` as the current branch-facing source of truth.

Active implementation plan package:

- `Plans/ProjectStellar-BSP-NextSupport-KiloPlan/00-MASTER-KILO-BSP-HARDENING-PLAN.md`
- Phase files in `Plans/ProjectStellar-BSP-NextSupport-KiloPlan/`

## Active scope

BSP maps are already the canonical playable level format. The current scope is BSP authoring and
presentation hardening, not another migration and not a reintroduction of the retired importer.

The completed BSP migration plan remains archived at
`Plans/Archived/project_stellar_bsp_canonical_plan/`.

Work phases are defined by the active hardening plan:

1. BSP diagnostics and `LevelAsset` data-contract foundation.
2. BSP PVS/leaf visibility and optional render culling.
3. BSP lightmap, texture, WAD/material fallback hardening.
4. BSP entity/sprite/object authoring conventions.
5. BSP validation tooling and generated-map regression fixtures.
6. Final hardening, documentation, archive, and next-scope handoff.

## Invariants

- Server authority remains mandatory.
- Lua scripting remains mandatory and sandboxed.
- Runtime collision, movement, triggers, object colliders, and scripting remain backend-neutral.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable through the shared graphics abstraction.
- No retired importer functionality, Source/VBSP, third-party physics, dynamic rigid bodies,
  full PBR, or client-side gameplay scripting unless explicitly requested.
