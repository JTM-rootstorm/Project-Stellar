---
project: Project Stellar
branch: audio-impl
plan_family: audio_footsteps
agent_optimized_for: Codex-style implementation agents
status: proposed_handoff
generated: 2026-05-04
---

# Project Stellar — Audio Footsteps Plan Bundle

## Intake summary

Implement floor-material/texture-dependent footstep sounds on `audio-impl`.

The smallest correct architecture is:

```text
BSP face texture/source material
  -> collision triangle surface metadata
  -> authoritative grounded movement/cadence check
  -> protocol GameplayEvent::kFootstep
  -> client presentation event queue
  -> AudioEventRouter maps surface id to generated retro one-shot sound id
  -> no-op sink in headless tests, optional miniaudio sink/manual audible smoke
```

Do not make render material sidecars authoritative. Use source texture names and a tiny dedicated surface-audio resolver.

## Current evidence snapshot

- `include/stellar/assets/CollisionAsset.hpp` currently stores triangle vertices and normals but not material/surface identity.
- `include/stellar/assets/LevelAsset.hpp` already preserves source-neutral surface materials, including original `source_name`, and surfaces point to optional material indices.
- `include/stellar/protocol/Types.hpp` currently has gameplay events for pickup, door state, script command, and script error; no footstep event exists yet.
- `include/stellar/audio/AudioEventRouter.hpp` and `src/audio/AudioEventRouter.cpp` already route server-approved events to `AudioRequestSink` with a no-op sink available.
- `CMakeLists.txt` already vendors/adds miniaudio, but `stellar_audio` currently only builds `AudioEventRouter.cpp`.
- `docs/Design.md` states rendering/audio are never gameplay truth and default tests must be display-free.

## Phase dependency graph

```text
AF-0 Branch/docs guardrails
  └─ AF-1 Collision surface identity + resolver
       ├─ AF-2 Authoritative cadence + kFootstep protocol event
       │    └─ AF-4 Integration + docs + hardening
       └─ AF-3 Audio routing + generated retro footstep assets
            └─ AF-4 Integration + docs + hardening
```

## Parallelization map

```text
Serial foundation:
  AF-0 -> AF-1

Parallel after AF-1:
  AF-2 authority/protocol lane
  AF-3 presentation/audio-assets lane

Final:
  AF-4 waits for AF-2 and AF-3
```

### Safe parallel work

- AF-1 tests/docs can proceed in parallel with AF-1 implementation after the metadata field names are decided.
- AF-2 protocol codec tests can proceed in parallel with AF-2 `WorldSession` tracker work after `GameplayEventKind::kFootstep` numeric value is decided.
- AF-3 generated audio asset creation can proceed in parallel with AF-3 router mapping; they only need agreed sound ids.
- AF-3 miniaudio sink is optional and can be skipped if the branch only needs event routing plus generated assets now.

### Do not parallelize

- Do not run AF-1 collision data contract and AF-2 ground-material querying in parallel unless the AF-1 API is already merged locally.
- Do not edit `GameplayEventKind`, `SnapshotCodec`, and `AudioEventRouter` in separate uncoordinated agents without first agreeing on the enum value and sound-id contract.
- Do not integrate live client playback until protocol, receiver, and router tests pass.

## Recommended generated sound ids

Keep sound ids simple and stable:

```text
footstep_generic_0
footstep_generic_1
footstep_concrete_0
footstep_concrete_1
footstep_metal_0
footstep_metal_1
footstep_wood_0
footstep_wood_1
footstep_stone_0
footstep_stone_1
footstep_dirt_0
footstep_dirt_1
footstep_grass_0
footstep_grass_1
footstep_water_0
footstep_water_1
```

Use an 8-bit/11.025 kHz or 16-bit/22.05 kHz mono WAV style for retro scope. Short one-shots around 80-180 ms are enough.

## Files in this bundle

1. `00-MASTER-AudioFootsteps-AgentPlan.md`
2. `01-Phase-AF0-Branch-Guardrails.md`
3. `02-Phase-AF1-Collision-Surface-Metadata.md`
4. `03-Phase-AF2-Authoritative-Footstep-Events.md`
5. `04-Phase-AF3-Audio-Routing-And-Generated-Sounds.md`
6. `05-Phase-AF4-End-To-End-Hardening.md`
7. `06-Codex-Agent-Task-Cards.md`

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
