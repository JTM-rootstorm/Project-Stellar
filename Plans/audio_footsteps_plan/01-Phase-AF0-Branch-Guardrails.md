---
project: Project Stellar
branch: audio-impl
plan_family: audio_footsteps
agent_optimized_for: Codex-style implementation agents
status: proposed_handoff
generated: 2026-05-04
---

# Phase AF-0 — Branch Guardrails and Active Handoff

## Goal

Prepare `audio-impl` for a focused footstep implementation without restarting broader audio, collision, networking, BSP, or client/server architecture work.

## Primary owner

`@director` or a single coordinating Codex session.

## Depends on

None.

## Can run in parallel with

Read-only code audit only. Do not run source changes in parallel with this phase.

## Required reading

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `Plans/NEXT.md`
- `include/stellar/audio/AudioEventRouter.hpp`
- `include/stellar/assets/CollisionAsset.hpp`
- `include/stellar/assets/LevelAsset.hpp`
- `include/stellar/protocol/Types.hpp`
- `src/network/SnapshotCodec.cpp`
- `src/server/WorldSession.cpp`
- `CMakeLists.txt`

## Tasks

1. Confirm local branch:
   ```bash
   git fetch --all --prune
   git checkout audio-impl
   git status --short
   ```
2. Create an active plan entry in the repo:
   - Preferred: `Plans/AudioFootsteps-AgentPlan.md`
   - Optional concise pointer: `Plans/NEXT.md`
3. Update `docs/ImplementationStatus.md` with a new active section:
   - target branch `audio-impl`
   - current objective: texture/material-dependent footsteps
   - phase checklist AF-0 through AF-4
   - statement that default tests remain display-free
   - statement that generated retro one-shots are acceptable for this slice
4. Record non-goals:
   - no full material gameplay system
   - no client-side footstep authority
   - no prediction/reconciliation
   - no full spatial audio engine unless already present
   - no dynamic terrain/material decals
5. Verify CMake target boundaries still make sense:
   - `stellar_protocol` must not link audio/server/client.
   - `stellar_server_core` must not link audio/miniaudio.
   - `stellar_audio` may depend on protocol.
   - real miniaudio sink, if added, should live in a presentation/runtime target, not in authority/server code.

## Acceptance criteria

- Active docs identify this work as the current `audio-impl` focus.
- The branch plan is narrow and footstep-specific.
- No source behavior changes are introduced in AF-0.
- Existing historical plans remain archived and are not rewritten.

## Focused validation

```bash
git diff -- Plans/NEXT.md docs/ImplementationStatus.md Plans/
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh .
```

## COMMIT CHECKPOINT

After validation passes:

```bash
git add Plans/AudioFootsteps-AgentPlan.md Plans/NEXT.md docs/ImplementationStatus.md
git commit -m "Plan audio footstep implementation"
```

Do not push.
## Global invariants for every phase

- Target branch: `audio-impl`.
- Do not push. Local commits are allowed only at the explicit checkpoints in each phase.
- Keep server authority intact. Footsteps are server-approved presentation events, not client guesses.
- Keep renderer, HUD, and audio presentation-only. They must not become sources of gameplay truth.
- Keep collision/runtime/audio contracts backend-neutral unless the phase explicitly works on a presentation sink.
- Keep default tests display-free. Real audio device checks must be opt-in/manual.
- BSP remains the canonical playable level format.
- Scope stays retro and practical: implement footsteps only, with generated one-shot sounds for now.
- Do not add full material gameplay, dynamic rigid bodies, animation systems, prediction/reconciliation, or a broad ECS rewrite.
- Public APIs need Doxygen `@brief` comments.
- Preserve deterministic ordering and deterministic event selection.
- Update `docs/ImplementationStatus.md` after each implemented phase.
- Update `docs/Design.md` or `docs/BspAuthoring.md` only when a broad architecture or authoring convention changes.
- Do not modify `AGENTS.md` or `.kilo/agents/*.md` without explicit user approval.

## Standard safe local commit checkpoint

At the end of any phase or subphase that says `COMMIT CHECKPOINT`, run:

```bash
git status --short
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh .
git diff --check
git status --short
```

If all relevant validation passes, make a local commit only:

```bash
git add <phase files>
git commit -m "<phase-specific message>"
```

Do **not** run `git push`.
