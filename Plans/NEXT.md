# Stellar Engine — Current Branch Handoff

Branch target: `collision-movement`

## Current entry point

Use `docs/ImplementationStatus.md` as the current branch-facing source of truth. This file is a short
handoff pointer for the next implementation agent.

## Active cleanup direction

The earlier Phase 6A-D world-authoring work has first-pass implementations. Do not restart those
plans as active work.

Near-term work is the removable-complexity cleanup described by
`Plans/ProjectStellar-RemovableComplexity-Cleanup-AgentPlan.md`:

- Treat Lua scripting as mandatory server-authoritative infrastructure.
- Remove stale optional Lua build/documentation paths.
- Reduce duplicated low-level geometry helpers.
- Share internal sensor overlap transition tracking while keeping trigger and object-collider public
  concepts separate.
- Share internal Lua hook dispatch while preserving trigger/object-collider context-specific systems.
- Keep gameplay behavior, event ordering, and server-authority boundaries unchanged.

## Validation posture

Prefer display-free tests by default. glTF scripted smokes remain gated by `STELLAR_ENABLE_GLTF`.
OpenGL/Vulkan context tests remain opt-in.
