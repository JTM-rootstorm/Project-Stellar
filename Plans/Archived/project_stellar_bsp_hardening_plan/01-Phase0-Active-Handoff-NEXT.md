Prepared: 2026-05-01  
Branch target: `bsp-integration`  
Audience: Kilo Code agents / Codex-style implementation agents  
Objective: implement the next required BSP support hardening after the canonical BSP migration.

# Phase 0 — Active Handoff and NEXT.md Lock-In

## Purpose

Create an active implementation handoff for BSP authoring and presentation hardening. Do not start
code changes until the active docs clearly point at this plan.

## Primary agent

`@director`

## Context

The BSP canonical migration is complete. `Plans/NEXT.md` currently points to BSP authoring and
presentation hardening. Phase 0 turns that broad handoff into the current active work entry point.

## Tasks

1. Add the plan package under a new active plan folder, for example:
   - `Plans/BSP-Hardening-Authoring-Presentation/00-MASTER-KILO-BSP-HARDENING-PLAN.md`
   - phase files from this package.
2. Update `Plans/NEXT.md` to:
   - identify this plan package as the active implementation entry point;
   - state that BSP is already canonical;
   - state that this work is hardening, not migration;
   - preserve invariants: server authority, mandatory Lua, display-free tests, OpenGL/Vulkan parity.
3. Add a new top section to `docs/ImplementationStatus.md`:
   - “BSP Authoring and Presentation Hardening — Active”
   - list planned phases and current status as “not started.”
4. Confirm `docs/Design.md` already matches the broad direction. Only make minimal updates if it
   still implies that the BSP canonical migration itself is active work.
5. Do not modify `AGENTS.md` unless it still routes active level work through retired importer
   assumptions.

## Acceptance criteria

- `Plans/NEXT.md` points to this hardening plan rather than just a broad one-line scope.
- `docs/ImplementationStatus.md` has a current active section for this work.
- No source/build behavior changes are introduced in this phase.
- Documentation review confirms no active plan tells agents to restart BSP migration.

## Validation

```bash
git diff -- Plans/NEXT.md docs/ImplementationStatus.md docs/Design.md AGENTS.md
git grep -n -i 'STELLAR_ENABLE_GLTF\|cgltf\|SceneAsset' -- . ':!Plans/Archived/**' ':!build*/**'
```

Expected: only intentional compatibility or historical archived hits, if any.

## NEXT.md policy

This phase updates `NEXT.md` at the start. Later phases should update `ImplementationStatus.md` with
completion notes. Do not repeatedly rewrite `NEXT.md` unless the active plan location changes.
