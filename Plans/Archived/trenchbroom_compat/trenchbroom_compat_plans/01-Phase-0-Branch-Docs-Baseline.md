# Phase 0 — Branch, Handoff, and Documentation Baseline

Branch target: `trenchbroom-compat`

## Goal

Establish the branch as the active implementation scope without changing behavior yet. Update the active handoff so later agents do not restart completed socket/BSP/collision/Lua work.

## Non-negotiable invariants

- Do not restart completed collision, Lua scripting, BSP gameplay-loop, BSP presentation/networking polish, or socket transport work.
- Do not claim TrenchBroom compatibility is complete in Phase 0.
- Do not change runtime behavior in this phase.
- Keep default validation display-free.

## Tasks

### 0.1 Create branch

```bash
git checkout main
git pull --ff-only
git checkout -b trenchbroom-compat
```

### 0.2 Add active plan files to repo

Recommended repo layout:

```text
Plans/
  TrenchBroomCompat-AgentPlan.md
  trenchbroom_compat/
    00-MASTER-TrenchBroomCompat-AgentPlan.md
    01-Phase-0-Branch-Docs-Baseline.md
    02-Phase-0_5-Remove-Prototype-Cube-Fallback.md
    03-Phase-1-ZUp-World-Axis-Contract.md
    04-Phase-2-ZUp-Runtime-Collision-Movement.md
    05-Phase-3-ZUp-Presentation-Network-Tests.md
    06-Phase-4-BSP30-Import-Validation.md
    07-Phase-5-TrenchBroom-Toolchain-FGD.md
    08-Phase-6-Fixtures-EndToEnd-Validation.md
    09-Phase-7-Final-Docs-Audits-Handoff.md
```

The root `Plans/TrenchBroomCompat-AgentPlan.md` may be the master plan or a short pointer to `Plans/trenchbroom_compat/00-MASTER-TrenchBroomCompat-AgentPlan.md`.

### 0.3 Update `Plans/NEXT.md`

While this branch is active, update `Plans/NEXT.md` to say:

- Current branch target is `trenchbroom-compat`.
- Current scope is:
  - convert active runtime/authoring convention from Y-up to Z-up;
  - target TrenchBroom-authored BSP30 maps;
  - fully remove prototype cube renderer/debug cube fallback so no-map rendering becomes static-less/blank;
  - add TrenchBroom game config, FGD, material/editor assets, BSP30 compile wrapper, and validation fixtures.
- Completed socket transport and prior BSP/collision/Lua scopes remain archived and must not be restarted.

Keep the previous post-socket options as deferred follow-ups, not active work.

### 0.4 Update `docs/ImplementationStatus.md`

Add a new top section:

```markdown
# Project Stellar: Implementation Status

Branch target: `trenchbroom-compat`

## Active Scope — TrenchBroom BSP30 Compatibility and Z-up Migration

Status: in progress.

Current branch goals:
- Convert active gameplay/runtime/world authoring convention from Y-up to Z-up.
- Preserve 1 Stellar unit = 1 inch.
- Target TrenchBroom-authored BSP30 maps.
- Add/validate TrenchBroom game configuration, FGD, material/editor assets, BSP30 compile wrapper, and end-to-end map fixtures.
- Preserve server authority, sandboxed Lua, BSP canonical runtime, and display-free validation.
```

Leave completed socket and historical sections below it.

### 0.5 Add initial docs placeholders

Add or reserve:

```text
docs/TrenchBroom.md
```

Initial content should state this is a work-in-progress branch and the final workflow is not stable until Phase 7.

### 0.6 Baseline audit commands

Run and save findings in Phase 0 notes:

```bash
git grep -n -i 'Y is up\|Y-up\|0 36 0\|floor at `y = 0`\|floor at y = 0' -- docs include src tests tools Plans ':!Plans/Archived/**' ':!build*/**'
git grep -n '0\.0F, 1\.0F, 0\.0F\|0, 1, 0' -- include src tests ':!build*/**'
git grep -n -i 'BSP29/BSP30\|BSP29\|BSP30' -- docs include src tests tools Plans ':!Plans/Archived/**' ':!build*/**'
git grep -n 'stellar_script\|stellar_extents\|stellar_sprite\|stellar_collision' -- tools docs tests include src ':!build*/**'
git grep -n 'CubeRenderer\|DebugCubeMesh\|create_debug_cube_mesh\|debug:cube\|debug_cube\|debug_cube_surface\|debug_red\|debug_green\|debug_blue' -- include src tests CMakeLists.txt docs Plans ':!Plans/Archived/**' ':!build*/**'
```

Do not fix all results in Phase 0; record them as Phase 1+ work.

## Expected files changed

- `Plans/NEXT.md`
- `Plans/TrenchBroomCompat-AgentPlan.md`
- `Plans/trenchbroom_compat/*.md`
- `docs/ImplementationStatus.md`
- optionally `docs/TrenchBroom.md`

## Validation

```bash
git status --short
git diff -- Plans docs
```

No build should be required if Phase 0 is docs/plans only.

## Acceptance criteria

- [ ] Branch exists.
- [ ] Active handoff plan exists.
- [ ] `Plans/NEXT.md` points to `trenchbroom-compat`.
- [ ] `docs/ImplementationStatus.md` has a top active scope for TrenchBroom BSP30 compatibility and Z-up migration.
- [ ] Baseline grep audit has been run and summarized in notes, including cube prototype references for Phase 0.5 removal.
- [ ] No runtime behavior changed.

## Parallelization

Safe to run in parallel:

- One docs agent updates `Plans/NEXT.md` and `docs/ImplementationStatus.md`.
- One planning agent adds the phase plan package.
- One audit agent runs grep and records findings.

Do not run multiple writers on the same file concurrently.
