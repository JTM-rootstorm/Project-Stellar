# Phase 0 — Archive Completed BSP Gameplay-Loop Plans and Publish Active Follow-up Handoff

Optimized for Kilo Code intake.

## Owner

- Primary: `@director`
- No specialist implementation needed unless docs reveal stale architecture conflicts.

## Goal

Convert the current completed `bsp-gameplay-loop` plan files from active-looking root-level plans into archived historical plans, then create a new active handoff for the follow-up implementation run.

This phase is documentation-only. It must happen before code phases so agents do not restart completed gameplay-loop work.

## Required reading

- `AGENTS.md`
- `.kilo/agents/director.md`
- `Plans/NEXT.md`
- `docs/ImplementationStatus.md`
- `docs/Design.md`
- `docs/BspAuthoring.md`
- `Plans/BspGameplayLoop-AgentPlan.md`
- `Plans/ProjectStellar-BSP-GameplayLoop-AgentPlan.md`

## Inputs from current branch

Current branch docs say the BSP gameplay-loop Phases 0-8 are complete and that next scope should focus on presentation/networking polish over the completed BSP gameplay loop.

Root-level completed plans still exist:

- `Plans/BspGameplayLoop-AgentPlan.md`
- `Plans/ProjectStellar-BSP-GameplayLoop-AgentPlan.md`

## Required repo edits

### 1. Create archive directory

Create:

```text
Plans/Archived/bsp_gameplay_loop/
```

### 2. Move completed plan files

Move without content loss:

```text
Plans/BspGameplayLoop-AgentPlan.md
    -> Plans/Archived/bsp_gameplay_loop/BspGameplayLoop-AgentPlan.md

Plans/ProjectStellar-BSP-GameplayLoop-AgentPlan.md
    -> Plans/Archived/bsp_gameplay_loop/ProjectStellar-BSP-GameplayLoop-AgentPlan.md
```

Do not delete their content. These files remain historical evidence.

### 3. Create new active plan files

Create these new root-level active plans:

```text
Plans/BspPresentationNetworkingPolish-AgentPlan.md
Plans/ProjectStellar-BSP-PresentationNetworkingPolish-AgentPlan.md
```

Minimum content for `Plans/BspPresentationNetworkingPolish-AgentPlan.md`:

- Branch target: `bsp-gameplay-loop`
- Active scope: presentation and networking polish over completed BSP gameplay loop
- First required phase: live scripted authoritative runtime integration
- Then: authoritative gameplay snapshot presentation
- Then: snapshot/delta/event contracts
- Then: local/remote transport bridge
- Then: HUD/audio/toolchain polish
- Then: final docs/archive validation
- Explicitly state that old gameplay-loop plans are archived and must not be restarted

Minimum content for `Plans/ProjectStellar-BSP-PresentationNetworkingPolish-AgentPlan.md`:

- Detailed phase plan matching this downloadable plan package
- Agent routing
- File paths
- Acceptance criteria
- Display-free validation commands
- Risks and failure protocol

### 4. Update `Plans/NEXT.md`

Replace "completed branch source of truth" style with new active handoff.

Required content:

```markdown
# Stellar Engine - Next Scope Handoff

Branch target: `bsp-gameplay-loop`

## Current entry point

Use `Plans/BspPresentationNetworkingPolish-AgentPlan.md` for the concise active handoff and
`Plans/ProjectStellar-BSP-PresentationNetworkingPolish-AgentPlan.md` for the detailed implementation plan.

## Completed historical scope

The previous BSP gameplay-loop plan package is complete and archived at:
`Plans/Archived/bsp_gameplay_loop/`

Do not restart completed collision, movement, Lua scripting, object-collider, BSP migration, BSP
hardening, or BSP gameplay-loop foundation work.

## Active recommended scope

Presentation and networking polish over the completed BSP gameplay loop.

First slices:
- Live scripted authoritative runtime integration for the client loop.
- Gameplay snapshot presentation for sprites, pickups, and door/gate state.
- Snapshot/delta/event contracts.
- Local/remote transport bridge.
- HUD/audio/toolchain polish.
```

Preserve invariants from the current `Plans/NEXT.md`.

### 5. Update `docs/ImplementationStatus.md`

Add a new top section above the completed gameplay-loop status:

```markdown
## Active Follow-up Scope — BSP Presentation and Networking Polish

Status: active / in progress.

This run builds on the completed BSP gameplay-loop branch. The completed plan package was archived
under `Plans/Archived/bsp_gameplay_loop/`.

Active phases:
- Phase PN-0 — Plan archival and follow-up handoff: in progress or complete.
- Phase PN-1 — Live scripted authoritative runtime integration.
- Phase PN-2 — Authoritative gameplay snapshot presentation.
- Phase PN-3 — Snapshot/delta/event transport contracts.
- Phase PN-4 — Local/remote transport bridge.
- Phase PN-5 — HUD/audio/toolchain polish.
- Phase PN-6 — Final docs, validation, and handoff.
```

Keep the existing completed BSP gameplay-loop notes below this new active section. Do not erase validation history.

### 6. Update `docs/Design.md`

Update only current branch direction / roadmap language needed to reflect:

- Completed BSP gameplay-loop is foundation.
- Current follow-up is presentation/networking polish.
- Live client must run authoritative scripts through server-side runtime when maps declare server script bindings.
- Gameplay snapshot presentation is allowed only as client presentation.
- Snapshot/delta/event contracts should remain serializable and server-authored.

Do not rewrite unrelated sections.

### 7. Update `docs/BspAuthoring.md`

Add a short note under scripts/interactions:

- Authored trigger/object-collider `stellar.script` ids are loaded by the authoritative runtime when the live client/session is started.
- Script ids remain asset-relative; absolute paths and `..` escapes remain invalid.
- Suggested script source location defaults to relative to the map directory unless the implementation adds a stricter configured script root.
- Presentation of pickups and doors is driven by server-owned snapshots, not by BSP import or client script.

## Validation

Run:

```bash
git status --short
git diff -- Plans docs
git grep -n 'BspGameplayLoop-AgentPlan\|ProjectStellar-BSP-GameplayLoop-AgentPlan' -- Plans docs ':!Plans/Archived/**'
```

Expected:

- Old plan filenames appear only as archive references, not as active handoff entry points.
- No code changes.
- `AGENTS.md` unchanged unless the user explicitly asked to edit it.

## Acceptance criteria

- Root `Plans/` contains the new active follow-up plan files.
- Completed gameplay-loop plans are under `Plans/Archived/bsp_gameplay_loop/`.
- `Plans/NEXT.md` points to the new active follow-up plan.
- `docs/ImplementationStatus.md` has a new active follow-up status section.
- `docs/Design.md` and `docs/BspAuthoring.md` reflect the new implementation run without changing old completion history.
