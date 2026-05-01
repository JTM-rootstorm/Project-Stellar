# Stellar Engine — Current Branch Handoff

Branch target: `collision-movement`

## Current entry point

Use `docs/ImplementationStatus.md` as the current branch-facing source of truth. This file is a short
handoff pointer for the next implementation agent.

## Post-cleanup state

The earlier Phase 6A-D world-authoring work has first-pass implementations. Do not restart those
plans as active work.

The removable-complexity cleanup is complete and the detailed agent plan has been correctly archived
at `Plans/Archived/ProjectStellar-RemovableComplexity-Cleanup-AgentPlan.md`. Treat that file as
historical context, not the next active implementation plan.

Current branch assumptions to preserve:

- Lua scripting is mandatory server-authoritative infrastructure.
- Gameplay behavior, event ordering, and server-authority boundaries should remain stable.
- Shared geometry helpers, sensor overlap tracking, and Lua hook dispatch are now common
  infrastructure and should receive direct regression coverage.
- New implementation slices should be introduced by updating this handoff and
  `docs/ImplementationStatus.md` with the next active scope.

## Validation posture

Prefer display-free tests by default. glTF scripted smokes remain gated by `STELLAR_ENABLE_GLTF`.
OpenGL/Vulkan context tests remain opt-in.
