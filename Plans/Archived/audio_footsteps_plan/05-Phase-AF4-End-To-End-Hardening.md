---
project: Project Stellar
branch: audio-impl
plan_family: audio_footsteps
agent_optimized_for: Codex-style implementation agents
status: proposed_handoff
generated: 2026-05-04
---

# Phase AF-4 — End-to-End Integration, Documentation, and Hardening

## Goal

Prove the full footstep path from floor texture/material to queued/audio-routed footstep request, with safe documentation and final validation.

## Primary owner

`@director` / integration Codex agent.

## Depends on

AF-2 and AF-3.

## Can run in parallel with

No broad implementation. Only docs polishing may run while focused integration tests are being debugged.

## End-to-end target

A local/single-player or loopback runtime using a BSP room with at least two floor materials should:

1. Move the authoritative player over floor A.
2. Emit footstep event with surface id A.
3. Move the authoritative player over floor B.
4. Emit footstep event with surface id B.
5. Queue both events for client presentation.
6. Route both events through `AudioEventRouter` to the expected sound id prefixes.
7. Keep default tests display-free.

## Test fixture recommendation

Add a small deterministic fixture if existing BSP fixtures cannot express this quickly:

```text
tests/fixtures/footsteps/two_material_floor.map
tests/fixtures/footsteps/two_material_floor.bsp generated under build tree
```

Floor halves:

```text
left half:  dev/grid_32 or concrete/floor_01 -> concrete
right half: metal/grate_01                  -> metal
```

If creating a real BSP fixture is too much for AF-4, use a synthetic `LevelAsset` in tests:

- two floor collision triangles/meshes
- each triangle has `CollisionSurfaceMetadata`
- player starts on one side and moves to the other
- runtime world built display-free

Prefer real BSP fixture if importer coverage is stable; synthetic test is acceptable for initial engine-level behavior.

## Integration test targets

Add/extend:

```text
tests/server/WorldSession.cpp
tests/client/SinglePlayerRuntime.cpp
tests/client/ClientWorldReceiver.cpp
tests/audio/AudioEventRouter.cpp
tests/integration/FootstepAudioPipeline.cpp
```

Suggested new integration test:

```text
stellar_footstep_audio_pipeline_test
```

Test behavior:

1. Build/load a world with collision surfaces `concrete` and `metal`.
2. Tick movement enough to cross both floor areas.
3. Collect emitted `GameplayEventKind::kFootstep`.
4. Assert at least one `concrete` event and one `metal` event.
5. Encode/decode events if the test crosses protocol boundary.
6. Feed events to `AudioEventRouter` with a fake sink.
7. Assert routed sounds have prefixes:
   - `footstep_concrete_`
   - `footstep_metal_`
8. Assert no non-footstep event is required to make footsteps work.

## Runtime wiring

Check live runtime paths:

```text
src/client/Application.cpp
src/client/SinglePlayerRuntime.cpp
src/client/RemoteClientRuntime.cpp
src/client/ListenHostRuntime.cpp
src/client/ClientWorldReceiver.cpp
src/client/HudPresentation.cpp
src/audio/AudioEventRouter.cpp
```

Make sure:

- queued `kFootstep` events are passed to audio routing in the same place as pickup/door/script events.
- HUD can ignore footsteps unless you intentionally add a debug HUD message.
- Remote clients can receive footstep events through the same event codec path.
- Single-player can expose footstep events without requiring a network server/listener.
- Headless tests use `NoOpAudioRequestSink` or fake sink.
- Real audio playback, if added, is optional and gracefully disabled when no device/sound asset exists.

## Docs

Update:

```text
docs/ImplementationStatus.md
docs/Design.md
docs/BspAuthoring.md
optional docs/Audio.md
optional assets/audio/footsteps/generated/README.md
```

Docs should state:

- Footsteps are server-approved presentation events.
- Floor sound category is derived from BSP source material/texture name through a small resolver.
- `.stellar_material` render sidecars remain presentation material data, not authoritative footstep gameplay.
- Default tests remain display-free.
- Generated retro footstep WAVs are placeholder assets for the current scope.
- How to add a new surface id:
  1. update resolver mapping
  2. add generated sounds
  3. add router mapping/test
  4. add fixture/import test

## Manual smoke test

If a display/audio device is available:

```bash
cmake --build build --target stellar-client -j$(nproc)
./build/stellar-client --map ./build/tests/fixtures/bsp/<footstep_room>.bsp
```

Manual checklist:

- walking on dev/concrete floor produces dull/concrete steps
- walking on metal floor produces metallic steps
- stopping stops footsteps
- jumping/falling does not spam footsteps
- unknown materials fall back to generic
- no crash or hard failure when sound files are missing

If the environment lacks display/audio, record as skipped manual validation; do not fail default CTest for that.

## Acceptance criteria

- Full default CTest passes.
- Focused footstep pipeline tests pass.
- Target boundary check passes.
- Docs accurately describe scope and authoring.
- Local commits exist for each completed phase.
- Branch has not been pushed.

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build --target \
  stellar_footstep_audio_pipeline_test \
  stellar_audio_event_router_test \
  stellar_snapshot_codec_test \
  stellar_client_world_receiver_test \
  stellar_server_world_session_test \
  stellar_client_single_player_runtime_test \
  stellar_bsp_importer_test \
  stellar_bsp_materials_test \
  -j$(nproc)

ctest --test-dir build -R '^(footstep_audio_pipeline|audio_event_router|snapshot_codec|client_world_receiver|server_world_session|client_single_player_runtime|bsp_importer|bsp_materials|footstep_surface)$' --output-on-failure
tools/dev/check_target_boundaries.sh .
ctest --test-dir build --output-on-failure
git diff --check
```

## FINAL COMMIT CHECKPOINT

Suggested commit message:

```bash
git add docs tests src include assets tools CMakeLists.txt
git commit -m "Complete texture-based footstep audio pipeline"
```

Do not push.

## Final branch status note

After the local commit, paste this into the final implementation notes or PR body later:

```markdown
Footstep audio implementation is complete locally on `audio-impl`.

Implemented:
- BSP/source-material surface id resolution for collision ground hits.
- Authoritative footstep cadence and `GameplayEventKind::kFootstep`.
- Protocol/receiver event round-trip.
- AudioEventRouter mapping to generated retro footstep sound ids.
- Display-free tests for resolver, codec, server/session, receiver, router, and pipeline.

Validation:
- <paste focused validation result>
- <paste full ctest result>
- <paste target boundary result>

Not pushed.
```
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
