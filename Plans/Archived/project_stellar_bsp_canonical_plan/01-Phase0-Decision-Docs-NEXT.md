# Phase 0 — Decision Lock-In, Active Docs, and NEXT.md Reset

Branch target: `bsp-integration`  
Primary agent: `@director`  
Dependencies: none  
Parallelizable: no; this phase establishes the source-of-truth direction.

---

## Goal

Replace the active branch direction from retired model importer-authored level work to BSP-canonical level work before implementation agents modify core code.

This phase prevents Kilo agents from continuing obsolete collision/retired model importer assumptions.

---

## Required edits

### 1. `Plans/NEXT.md`

Replace the current cleanup-oriented handoff with a short BSP migration handoff.

Required content:

- Branch target: `bsp-integration`.
- Current active scope: BSP canonical level format migration and retired model importer removal.
- Entry point: `docs/ImplementationStatus.md` plus the active BSP plan file under `Plans/`.
- Preserve assumptions:
  - server authority,
  - Lua mandatory,
  - display-free default tests,
  - OpenGL/Vulkan runtime-selectable presentation,
  - no third-party physics,
  - no dynamic rigid bodies,
  - no retired model importer functionality.
- State that older retired model importer/collision phase plans are historical only.

Recommended replacement skeleton:

```markdown
# Stellar Engine — Current Branch Handoff

Branch target: `bsp-integration`

## Current entry point

Use `docs/ImplementationStatus.md` as the current branch-facing source of truth.
The active implementation plan is `Plans/ProjectStellar-BSPCanonical-AgentPlan.md`.

## Active scope

The current active scope is migration to BSP maps as the canonical playable level format and removal
of retired model importer functionality from active code, active build configuration, active tests, and active docs.

## Invariants

- BSP is the canonical level source.
- Lua scripting remains mandatory server-authoritative infrastructure.
- Server authority, runtime collision state, triggers, object-collider sensors, and script command
  validation remain native/server-owned.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable presentation backends.
- No third-party physics, dynamic rigid bodies, Source/VBSP, full PBR, or model/animation systems are
  part of this migration.

## NEXT.md lifecycle

After the BSP migration is complete, archive the detailed BSP plan under `Plans/Archived/` and rewrite
this file to the next active post-BSP scope. Do not leave this file pointing at completed migration work.
```

### 2. `docs/ImplementationStatus.md`

Add a new top section above existing status:

- “BSP Canonical Migration — Active”
- State that the user selected BSP as canonical level format.
- State that retired model importer is to be removed, not kept side-by-side.
- State current phase: Phase 0 in progress/complete.
- Link/reference `Plans/NEXT.md` and `Plans/ProjectStellar-BSPCanonical-AgentPlan.md`.
- Preserve older status as historical context below.

Do not delete useful completion history. Mark old retired model importer/collision authoring notes as historical, not active.

### 3. `docs/Design.md`

Make only direction-setting changes in Phase 0. Detailed API docs come in later phases.

Required changes:

- Project overview says: 3D BSP level geometry with 2D billboard entities/objects/props.
- Asset format says: BSP maps are canonical playable level format.
- Remove statements claiming retired model importer is the runtime scene/level source.
- Add a temporary migration note: active implementation is moving from retired model importer-shaped `retired scene asset` toward BSP-backed `LevelAsset`.
- Retain server-authoritative scripting and collision rules.

### 4. `AGENTS.md`

Because the user explicitly changed the project direction, update only coordination/routing references needed to avoid agents continuing retired model importer work.

Allowed edits:

- Replace retired model importer level-import routing references with BSP level-import routing references.
- Keep all existing agent names and delegation rules.
- Do not create new agents.
- Do not alter the rule that only `@director` delegates.

Required routing replacements:

- “retired model importer rendering/import” → “BSP level geometry import/rendering where applicable.”
- “retired model importer collision extraction” → “BSP level collision/entity metadata extraction.”
- “Current renderer goal is lightweight material parity, not full retired model importer PBR” → “Current renderer goal is lightweight BSP surface/material and billboard parity, not full PBR.”

---

## Acceptance criteria

- `Plans/NEXT.md` no longer points at removable-complexity cleanup as active work.
- `docs/ImplementationStatus.md` clearly says BSP migration is active.
- `docs/Design.md` no longer says retired model importer is the canonical playable world/level source.
- `AGENTS.md` no longer routes new level work through retired model importer terminology.
- No code changes are required in this phase unless docs build/lint requires them.

---

## Validation

Documentation review only is acceptable for Phase 0.

Recommended search after edits:

```bash
rg -n -i "retired model importer|retired parser dependency|retired importer feature gate" docs AGENTS.md Plans/NEXT.md
```

Expected:

- Active docs should not describe retired model importer as a current or future required level pipeline.
- Historical references may remain only if clearly labeled historical or archived.

---

## Completion note template

Add to `docs/ImplementationStatus.md`:

```markdown
Phase BSP-0 is complete as of YYYY-MM-DD:

- Locked active branch direction to BSP maps as canonical playable level format.
- Marked retired model importer functionality for removal rather than side-by-side support.
- Replaced `Plans/NEXT.md` with the BSP migration handoff.
- Updated active design/agent guidance so new work no longer follows retired model importer level assumptions.

Validation: documentation review and active-reference search.
```
