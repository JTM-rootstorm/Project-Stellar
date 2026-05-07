---
phase: AAS-1
title: Audible Smoke Harness
depends_on: [AAS-0]
---

# AAS-1 — Audible Smoke Harness

## Goal

Add a narrow opt-in executable that audibly plays generated footstep WAVs through `MiniaudioRequestSink`.

## Preferred files

```text
tools/audio/AudioSmokeMain.cpp
CMakeLists.txt
tests/cmake/AudioPlatformTests.cmake  # optional opt-in registration
```

## Executable

`stellar-audio-smoke` arguments:

```text
--sound <sound_id>       repeatable; default footstep_concrete_0 and footstep_metal_1
--duration-ms <ms>       default 2500
--gap-ms <ms>            default 250
--list-defaults
--asset-root <path>
```

## Behavior

1. Require `STELLAR_ENABLE_AUDIO=1`.
2. Initialize `MiniaudioRequestSink`.
3. Play each requested sound.
4. Sleep long enough for playback to be audible.
5. Print structured logs:
   ```text
   stellar-audio-smoke: audio_requested=1
   stellar-audio-smoke: miniaudio_active=1
   stellar-audio-smoke: play sound_id=footstep_concrete_0 accepted=1
   stellar-audio-smoke: operator_confirmation=heard
   ```
6. If `STELLAR_AUDIO_SMOKE_CONFIRM=heard`, return `0` after successful play requests.
7. If confirmation is missing, return `77`.

## Acceptance criteria

- Harness builds on macOS Metal and Metal-only.
- Missing device returns `77` with diagnostics.
- Successful device init plus operator confirmation returns `0`.
- Default CTest remains unchanged.

## Validation

```bash
cmake --build build-macos-metal --target stellar-audio-smoke -j$(sysctl -n hw.ncpu)

STELLAR_ENABLE_AUDIO=1 build-macos-metal/stellar-audio-smoke --duration-ms 1500 || test $? -eq 77

STELLAR_ENABLE_AUDIO=1 STELLAR_AUDIO_SMOKE_CONFIRM=heard \
  build-macos-metal/stellar-audio-smoke \
  --sound footstep_concrete_0 \
  --sound footstep_metal_1 \
  --duration-ms 2500
```
