---
project: Project Stellar
branch: audio-impl
plan_family: audio_footsteps
agent_optimized_for: Codex-style implementation agents
status: proposed_handoff
generated: 2026-05-04
---

# Phase AF-3 — Audio Routing and Generated Retro Footstep Sounds

## Goal

Map server-approved `kFootstep` events to presentation sound ids and provide generated retro one-shot assets for the needed surface types.

This phase may remain no-op/headless by default. Audible miniaudio playback is optional unless the branch already has a runtime sink seam ready.

## Primary owner

`@suzuki` / audio Codex agent.

## Consult

`@director` for target boundaries.

## Depends on

- AF-1 for surface id list.
- AF-2 only for compiling against `GameplayEventKind::kFootstep`.
- Sound asset generation can start after AF-1 surface names are agreed.

## Can run in parallel with

AF-2, if the event enum/sound-id contract is agreed first.

## Must finish before

AF-4.

## Router config

Update:

```text
include/stellar/audio/AudioEventRouter.hpp
src/audio/AudioEventRouter.cpp
tests/audio/AudioEventRouter.cpp
```

Suggested config:

```cpp
struct AudioEventRouterConfig {
    std::string pickup_sound_id = "pickup";
    std::string door_open_sound_id = "door_open";
    std::string door_close_sound_id = "door_close";
    std::string script_error_sound_id;

    std::uint32_t footstep_variant_count = 2;
    std::string footstep_generic_prefix = "footstep_generic";
    std::vector<FootstepSurfaceSound> footstep_surface_sounds;
};

struct FootstepSurfaceSound {
    std::string surface_id;
    std::string sound_prefix;
};
```

Simpler acceptable config:

```cpp
std::map<std::string, std::string> footstep_surface_sound_prefixes;
```

But prefer `std::vector` if avoiding ordered associative containers in public config is consistent with project style.

Default mapping:

```text
generic  -> footstep_generic
concrete -> footstep_concrete
metal    -> footstep_metal
wood     -> footstep_wood
stone    -> footstep_stone
dirt     -> footstep_dirt
grass    -> footstep_grass
water    -> footstep_water
```

Variant selection must be deterministic:

```cpp
variant = (event.tick + event.player_id + event.entity_id) % footstep_variant_count;
sound_id = prefix + "_" + std::to_string(variant);
```

If `footstep_variant_count == 0`, treat as one unnumbered id or fallback to 1; document the behavior.

## Router behavior

Add to `AudioEventRouter::route_event`:

```cpp
case stellar::network::GameplayEventKind::kFootstep:
    route_footstep_event(...);
    break;
```

Rules:

- `event.code` is the surface id.
- Empty or unknown `event.code` uses generic.
- Route exactly one one-shot request.
- Missing loaded sound remains a presentation diagnostic from the sink.
- Router must not inspect collision/world/renderer state.

## Generated retro sound assets

Create generated or checked-in assets under a narrow location, for example:

```text
assets/audio/footsteps/generated/
  footstep_generic_0.wav
  footstep_generic_1.wav
  footstep_concrete_0.wav
  footstep_concrete_1.wav
  footstep_metal_0.wav
  footstep_metal_1.wav
  footstep_wood_0.wav
  footstep_wood_1.wav
  footstep_stone_0.wav
  footstep_stone_1.wav
  footstep_dirt_0.wav
  footstep_dirt_1.wav
  footstep_grass_0.wav
  footstep_grass_1.wav
  footstep_water_0.wav
  footstep_water_1.wav
```

Preferred: add a deterministic generation script and generated outputs if the repo allows binary assets.

```text
tools/audio/generate_retro_footsteps.py
assets/audio/footsteps/generated/README.md
```

Generation spec:

- WAV PCM mono.
- 22050 Hz or 11025 Hz.
- 80-180 ms.
- Procedural noise/transient only; no copyrighted samples.
- Each surface should be distinct:
  - concrete/generic: short filtered thud/click
  - metal: brighter clang/noisy ring
  - wood: hollow knock
  - stone: dry hard tap
  - dirt/grass: softer noise crunch
  - water: short splash/plop
- Keep file count small: 2 variants per surface.

If the repo should avoid binary WAV commits, commit the generator and generated-manifest only, then make runtime asset loading optional. The user said agents can generate sounds, so binary WAV output is acceptable if repo policy allows it.

## Optional miniaudio sink

If `audio-impl` already expects real playback now, add a presentation-only sink:

```text
include/stellar/audio/MiniaudioSink.hpp
src/audio/MiniaudioSink.cpp
tests/audio/MiniaudioSink.cpp
```

Rules:

- Do not require an audio device in default tests.
- Default tests use fake/no-op sinks.
- Miniaudio target may link miniaudio, but authority/server/protocol targets must not.
- Return diagnostics for missing sound ids.
- Allow a simple sound registry:
  ```cpp
  register_sound("footstep_wood_0", "assets/audio/footsteps/generated/footstep_wood_0.wav");
  ```
- If asset loading is too much for this slice, skip miniaudio sink and rely on router/fake sink tests.

## Tests

Add/extend:

```text
tests/audio/AudioEventRouter.cpp
optional tests/audio/GeneratedFootstepAssets.cpp
optional tests/audio/MiniaudioSink.cpp
```

Required coverage:

1. `kFootstep + wood + tick/player/entity` routes to deterministic variant.
2. `kFootstep + unknown` routes to generic.
3. Empty surface id routes to generic.
4. Variant selection is stable for identical events.
5. Missing sound remains a presentation diagnostic only.
6. No-op sink accepts footstep events.
7. If generated assets are committed, validate expected files exist and are non-empty.
8. If generator is committed, validate it can produce files in a temp/build directory.

## Acceptance criteria

- `AudioEventRouter` handles `kFootstep`.
- Tests prove deterministic surface-to-sound mapping.
- Generated retro sound plan/assets exist for all current surface ids.
- No server/collision/protocol target depends on miniaudio/audio presentation.
- Default tests do not require an audio device.

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build --target \
  stellar_audio_event_router_test \
  -j$(nproc)
ctest --test-dir build -R '^(audio_event_router|generated_footstep_assets|miniaudio_sink)$' --output-on-failure
tools/dev/check_target_boundaries.sh .
```

If generated assets are checked in, also run:

```bash
python3 tools/audio/generate_retro_footsteps.py --out /tmp/stellar-footstep-gen-check
find /tmp/stellar-footstep-gen-check -type f -name '*.wav' -size +0c
```

## COMMIT CHECKPOINT

Suggested commit message:

```bash
git add include/stellar/audio src/audio tests/audio tools/audio assets/audio CMakeLists.txt docs/ImplementationStatus.md
git commit -m "Route footstep events to retro audio sounds"
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
