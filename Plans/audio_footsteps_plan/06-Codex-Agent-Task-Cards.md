---
project: Project Stellar
branch: audio-impl
plan_family: audio_footsteps
agent_optimized_for: Codex-style implementation agents
status: proposed_handoff
generated: 2026-05-04
---

# Codex Agent Task Cards — Audio Footsteps

This file is a compact task-card version of the phased plans. Give individual cards to Codex agents when splitting work.

## Card AF-0A — Active docs setup

Branch: `audio-impl`  
Parallel-safe: no source implementation in parallel  
Commit: yes, after validation  

Tasks:
1. Create `Plans/AudioFootsteps-AgentPlan.md`.
2. Update `Plans/NEXT.md`.
3. Update `docs/ImplementationStatus.md`.
4. Preserve global invariants and non-goals.
5. Validate full build/tests.

Commit message:

```bash
git commit -m "Plan audio footstep implementation"
```

## Card AF-1A — Collision metadata API

Branch: `audio-impl`  
Parallel-safe: no, API foundation  
Commit: with AF-1B/AF-1C only after tests pass  

Tasks:
1. Add `CollisionSurfaceMetadata`.
2. Add metadata field to `CollisionTriangle`.
3. Keep defaults generic/invalid.
4. Update synthetic collision tests.

Files likely touched:
- `include/stellar/assets/CollisionAsset.hpp`
- `tests/physics/CollisionWorld.cpp`
- `tests/world/CollisionValidation.cpp`

## Card AF-1B — Footstep surface resolver

Branch: `audio-impl`  
Parallel-safe: yes after names agreed  
Commit: with AF-1 phase  

Tasks:
1. Add resolver from source material/texture names to surface ids.
2. Add case/path normalization.
3. Add tests for generic/concrete/metal/wood/stone/dirt/grass/water.
4. Do not use render material sidecars as authority.

Files likely touched:
- `include/stellar/world/FootstepSurface.hpp`
- `src/world/FootstepSurface.cpp`
- `tests/world/FootstepSurface.cpp`
- `CMakeLists.txt`

## Card AF-1C — BSP importer metadata fill

Branch: `audio-impl`  
Parallel-safe: after AF-1A/AF-1B compile  
Commit: with AF-1 phase  

Tasks:
1. Find face-to-render-material and face-to-collision triangle emission.
2. Copy surface/material/source texture identity into collision triangles.
3. Resolve `footstep_surface_id`.
4. Add importer/material tests.

Files likely touched:
- `src/import/bsp/BspLevelBuilder.cpp`
- `src/import/bsp/BspGeometryBuilder.cpp` if face data flows there
- `tests/import/bsp/BspMaterials.cpp`
- `tests/import/bsp/BspImporter.cpp`

## Card AF-2A — Protocol event enum and codec

Branch: `audio-impl`  
Parallel-safe: yes after enum value is agreed  
Commit: with AF-2 phase  

Tasks:
1. Add `GameplayEventKind::kFootstep = 5`.
2. Update codec event-kind validation.
3. Add event round-trip test.
4. Update switch statements to handle footstep or intentionally ignore in HUD.

Files likely touched:
- `include/stellar/protocol/Types.hpp`
- `src/network/SnapshotCodec.cpp`
- `tests/network/SnapshotCodec.cpp`
- `src/client/HudPresentation.cpp`

## Card AF-2B — Footstep tracker

Branch: `audio-impl`  
Parallel-safe: yes after AF-1 metadata exists  
Commit: with AF-2 phase  

Tasks:
1. Add deterministic distance-based tracker.
2. Add grounded/speed/surface checks.
3. Add reset behavior.
4. Add unit tests.

Files likely touched:
- `include/stellar/server/FootstepTracker.hpp`
- `src/server/FootstepTracker.cpp`
- `tests/server/FootstepTracker.cpp`
- `CMakeLists.txt`

## Card AF-2C — World/session event emission

Branch: `audio-impl`  
Parallel-safe: no, integration-heavy  
Commit: with AF-2 phase  

Tasks:
1. Add tracker state to `WorldSession` or the nearest authority session owner.
2. Resolve current ground surface using filtered collision state.
3. Emit `kFootstep` through the existing gameplay event path.
4. Ensure single-player/server runtime forwards the event.
5. Add session/runtime/receiver tests.

Files likely touched:
- `include/stellar/server/WorldSession.hpp`
- `src/server/WorldSession.cpp`
- `src/server/ServerRuntime.cpp`
- `src/client/SinglePlayerRuntime.cpp`
- `src/authority/NetworkConversion.cpp`
- `tests/server/WorldSession.cpp`
- `tests/client/ClientWorldReceiver.cpp`
- `tests/client/SinglePlayerRuntime.cpp`

## Card AF-3A — Audio router mapping

Branch: `audio-impl`  
Parallel-safe: yes with AF-2 if enum is locally stubbed/coordinated  
Commit: with AF-3 phase  

Tasks:
1. Add footstep surface prefix mapping to `AudioEventRouterConfig`.
2. Route `kFootstep` deterministically to variant sound id.
3. Add fake/no-op sink tests.
4. Unknown/empty surface falls back to generic.

Files likely touched:
- `include/stellar/audio/AudioEventRouter.hpp`
- `src/audio/AudioEventRouter.cpp`
- `tests/audio/AudioEventRouter.cpp`

## Card AF-3B — Generated retro sound assets

Branch: `audio-impl`  
Parallel-safe: yes after sound ids are agreed  
Commit: with AF-3 phase  

Tasks:
1. Add generator script for 2 WAV variants per surface.
2. Generate generic/concrete/metal/wood/stone/dirt/grass/water.
3. Add README/manifest.
4. Add non-device tests to verify generation or file presence.

Files likely touched:
- `tools/audio/generate_retro_footsteps.py`
- `assets/audio/footsteps/generated/*.wav`
- `assets/audio/footsteps/generated/README.md`
- `tests/audio/GeneratedFootstepAssets.cpp`

## Card AF-3C — Optional miniaudio sink

Branch: `audio-impl`  
Parallel-safe: yes, if kept presentation-only  
Commit: separate optional commit or AF-3 phase  

Tasks:
1. Add presentation-only miniaudio sink if needed.
2. Keep default tests no-op/fake.
3. Ensure authority/server/protocol do not link miniaudio.
4. Add missing sound diagnostics.

Files likely touched:
- `include/stellar/audio/MiniaudioSink.hpp`
- `src/audio/MiniaudioSink.cpp`
- `tests/audio/MiniaudioSink.cpp`
- `CMakeLists.txt`

## Card AF-4A — Footstep pipeline test

Branch: `audio-impl`  
Parallel-safe: no, final integration  
Commit: with AF-4 final  

Tasks:
1. Add synthetic or BSP fixture with two floor materials.
2. Move player across both materials.
3. Assert concrete/metal events.
4. Route events through `AudioEventRouter`.
5. Assert sound prefixes.

Files likely touched:
- `tests/integration/FootstepAudioPipeline.cpp`
- `tests/fixtures/footsteps/*`
- `CMakeLists.txt`

## Card AF-4B — Final docs and validation

Branch: `audio-impl`  
Parallel-safe: yes while AF-4A is stabilizing, but final commit after tests  
Commit: final  

Tasks:
1. Update status/design/authoring docs.
2. Note generated sounds are placeholder retro assets.
3. Record validation commands and results.
4. Run focused and full test suite.
5. Commit locally only.

Commit message:

```bash
git commit -m "Complete texture-based footstep audio pipeline"
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
