# Stellar Engine — Current Branch Handoff

Branch target: `bsp-integration`

## Current entry point

Use `docs/ImplementationStatus.md` as the current branch-facing source of truth.

## Post-BSP state

BSP maps are now the canonical playable level format. Retired importer functionality has been
removed from active code, build configuration, tests, and active docs. Runtime world assembly,
server-authoritative movement/collision, triggers, object-collider sensors, Lua hooks, client map
validation, and BSP level rendering operate from BSP-backed `LevelAsset` data.

The completed BSP migration plan is archived at
`Plans/Archived/project_stellar_bsp_canonical_plan/`.

## Next Recommended Active Scope

Recommended next scope: BSP authoring and presentation hardening.

Focus areas:

- BSP PVS/leaf visibility culling if not fully active.
- BSP lightmap/material/texture fallback hardening.
- Sprite/entity authoring conventions for BSP entity keys.
- Map validation diagnostics for malformed or unsupported BSP content.

## Invariants

- Server authority remains mandatory.
- Lua scripting remains mandatory and sandboxed.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable.
- No retired importer functionality, Source/VBSP, third-party physics, dynamic rigid bodies,
  full PBR, or client-side gameplay scripting unless explicitly requested.
