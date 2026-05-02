# Project Stellar — TrenchBroom Compatibility Plan Package

Branch target: `trenchbroom-compat`

Primary goal: make Project Stellar compatible with TrenchBroom-authored BSP30 maps while converting the codebase and documentation from the current Y-up world convention to a Z-up convention that aligns better with Quake/GoldSrc/TrenchBroom authoring workflows.

This package is written for AI-agent execution. Each phase contains goals, invariants, concrete tasks, parallelization notes, files to inspect or change, validation commands, and acceptance criteria.

---

## Current-main baseline observed before planning

These observations were used to shape the plan:

- `docs/BspAuthoring.md` currently documents:
  - BSP maps are Stellar's canonical playable level source.
  - gameplay coordinates are inch-scale.
  - the active convention is Y-up.
  - authored BSP coordinates import 1:1.
  - minimal workflow accepts classic BSP29/BSP30-style maps.
  - dotted Stellar keys must reach BSP entity text directly or through importer-supported aliases.
  - `tools/bsp/stellar_entities.fgd` is only a helper and has placeholder field names.
- `docs/Design.md` currently documents:
  - current branch direction as `socket-transport`.
  - BSP29 and BSP30 support.
  - inch-scale Y-up gameplay.
  - server authority, Lua sandboxing, and display-free validation as active invariants.
- `Plans/NEXT.md` currently points at post-socket-transport options, not TrenchBroom compatibility.
- `include/stellar/core/WorldUnits.hpp` centralizes inch-scale numeric constants but does not yet centralize axis vectors.
- `include/stellar/physics/CharacterController.hpp` defaults movement up and ground normals to `{0, 1, 0}`.
- `tools/bsp/stellar_entities.fgd` currently exposes plain editor placeholder names such as `stellar_script`; current docs say these are not importer aliases unless remapped.
- Active cube prototype remnants exist and should be removed before Z-up work:
  - `include/stellar/graphics/opengl/CubeRenderer.hpp`
  - `src/graphics/opengl/CubeRenderer.cpp`
  - `include/stellar/graphics/DebugCubeMesh.hpp`
  - `src/graphics/DebugCubeMesh.cpp`
  - cube fallback code in `LevelRenderer`
  - cube-specific coverage in `tests/graphics/RenderSceneInspection.cpp`
  - CMake entries in `stellar_graphics`.

---

## Branch-level decisions

### Required end state

- Branch name: `trenchbroom-compat`.
- Authoring target: **TrenchBroom + BSP30**.
- Runtime/import convention after migration: **Z-up**.
- Unit policy remains: **1 Stellar gameplay unit = 1 authored inch**.
- Imported BSP30 coordinates should remain **1:1** with no hidden scale conversion.
- Player capsule center for the default 72-inch capsule should be authored at **z = 36** when the floor is at **z = 0**.
- The practical first room should become **192 × 192 × 96** in X/Y/Z, with floor at `z = 0`, ceiling at `z = 96`, and player start at `origin = "0 0 36"`.
- The prototype cube renderer, debug cube mesh, and synthetic cube fallback should be fully removed. No-map and remote-without-presentation-map modes should render a blank/static-less frame instead of a hardcoded cube.

### BSP30 interpretation

All new TrenchBroom tooling, docs, fixtures, and acceptance tests should target **BSP30**.

Do not remove the existing BSP29 reader unless it blocks the branch. Keep legacy BSP29 support as a compatibility path if it remains low-cost, but the TrenchBroom workflow must not depend on BSP29.

### Explicit non-goals

Do not implement these in this branch unless a later instruction explicitly expands scope:

- Source/VBSP.
- Direct `.map` runtime loading.
- Moving `func_door`, `func_button`, plats, trains, or rotating brush simulation.
- Dynamic rigid bodies or third-party physics integration.
- Client-side gameplay scripting.
- Renderer-owned, audio-owned, or HUD-owned gameplay authority.
- Full PBR/material system.
- Remote map transfer/caching.
- Client prediction, reconciliation, or interpolation.
- True simultaneous multiplayer expansion beyond current completed socket scope.

---

## Global acceptance checklist

The branch is done when all of the following are true:

- [ ] Repository has a `trenchbroom-compat` branch handoff and active docs.
- [ ] Active source no longer contains `CubeRenderer`, `DebugCubeMesh`, `create_debug_cube_mesh`, `debug:cube`, `debug_cube`, `debug_cube_surface`, `debug_red`, `debug_green`, or `debug_blue`.
- [ ] No-map and remote-without-presentation-map modes use a blank/static-less fallback instead of a synthetic cube level.
- [ ] `docs/ImplementationStatus.md` identifies `trenchbroom-compat` as current branch target.
- [ ] `Plans/NEXT.md` points at this branch's active plan while implementation is in progress, then post-branch follow-up options when complete.
- [ ] `docs/Design.md` describes Z-up BSP30 authoring and no longer describes active Y-up gameplay.
- [ ] `docs/BspAuthoring.md` describes TrenchBroom + BSP30 + Z-up as the primary authoring workflow.
- [ ] `include/stellar/core/WorldUnits.hpp` or a companion core header centralizes world up/forward/right axis constants.
- [ ] Active source and non-archived docs no longer rely on hardcoded Y-up literals such as `{0, 1, 0}` for gameplay up.
- [ ] Character movement, gravity, ground probing, step-up, trigger/object-collider overlap, camera, billboards, snapshots, and generated test fixtures all use Z-up semantics.
- [ ] BSP30 import and validation are covered by tests.
- [ ] A TrenchBroom Stellar game configuration exists under `tools/trenchbroom/Stellar/`.
- [ ] The Stellar FGD used by TrenchBroom emits dotted keys or importer-supported aliases; it does not depend on plain placeholder names such as `stellar_script`.
- [ ] A BSP30 compile/validation wrapper exists and is documented.
- [ ] At least one TrenchBroom-authored or TrenchBroom-compatible BSP30 fixture validates display-free.
- [ ] `stellar-client --validate-map <fixture.bsp>` passes for BSP30 fixtures.
- [ ] `stellar-server --validate-config --map <fixture.bsp>` passes for BSP30 fixtures.
- [ ] Full default CTest passes.
- [ ] A final grep audit finds no active Y-up authoring guidance except historical/archive references.

---

## Recommended phase order

1. Phase 0 — branch, handoff, and docs baseline.
2. Phase 0.5 — remove prototype cube renderer and debug cube fallback.
3. Phase 1 — central Z-up world-axis contract.
4. Phase 2 — Z-up runtime, collision, movement, scripting metadata, and fixtures.
5. Phase 3 — Z-up presentation, camera, input mapping, snapshots, and network-adjacent tests.
6. Phase 4 — BSP30 import/validation lockdown.
7. Phase 5 — TrenchBroom package, FGD, material/editor assets, and compile wrappers.
8. Phase 6 — exported map fixtures and end-to-end validation.
9. Phase 7 — final documentation, audits, archival, and handoff.

Phase 0.5 should run before Z-up implementation so agents do not spend time converting cube-only fallback geometry that will be deleted. Phases 1, 2, and 3 are tightly coupled because axis conversion touches shared assumptions. Phase 4 and Phase 5 can begin in parallel after Phase 0/0.5 if they avoid committing old Y-up examples. Phase 6 depends on Phases 2, 4, and 5. Phase 7 is last.

---

## File index

- `00-MASTER-TrenchBroomCompat-AgentPlan.md` — this file.
- `01-Phase-0-Branch-Docs-Baseline.md`
- `02-Phase-0_5-Remove-Prototype-Cube-Fallback.md`
- `03-Phase-1-ZUp-World-Axis-Contract.md`
- `04-Phase-2-ZUp-Runtime-Collision-Movement.md`
- `05-Phase-3-ZUp-Presentation-Network-Tests.md`
- `06-Phase-4-BSP30-Import-Validation.md`
- `07-Phase-5-TrenchBroom-Toolchain-FGD.md`
- `08-Phase-6-Fixtures-EndToEnd-Validation.md`
- `09-Phase-7-Final-Docs-Audits-Handoff.md`
