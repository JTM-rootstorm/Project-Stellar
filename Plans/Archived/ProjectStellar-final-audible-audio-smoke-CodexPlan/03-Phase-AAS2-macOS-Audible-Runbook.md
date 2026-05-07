---
phase: AAS-2
title: macOS Audible Runbook
depends_on: [AAS-1]
---

# AAS-2 — macOS Audible Runbook

## Preconditions

- macOS host.
- Output device selected and unmuted.
- `macos-compat` branch.
- `build-macos-metal` or `build-macos-metal-only` exists.
- Generated WAVs exist in `assets/audio/footsteps/generated/`.

## Commands

```bash
cmake --preset macos-metal
cmake --build --preset macos-metal --parallel $(sysctl -n hw.ncpu)

ctest --test-dir build-macos-metal -R '^(audio_event_router|miniaudio_sink|generated_footstep_assets|footstep_audio_pipeline)$' --output-on-failure

STELLAR_ENABLE_AUDIO=1 STELLAR_AUDIO_SMOKE_CONFIRM=heard \
  build-macos-metal/stellar-audio-smoke \
  --sound footstep_concrete_0 \
  --sound footstep_metal_1 \
  --duration-ms 2500
```

Metal-only variant:

```bash
cmake --preset macos-metal-only
cmake --build --preset macos-metal-only --parallel $(sysctl -n hw.ncpu)

STELLAR_ENABLE_AUDIO=1 STELLAR_AUDIO_SMOKE_CONFIRM=heard \
  build-macos-metal-only/stellar-audio-smoke \
  --sound footstep_wood_0 \
  --sound footstep_water_1 \
  --duration-ms 2500
```

## Evidence block

Recorded local evidence:

```text
Audible audio smoke completed on 2026-05-06.
Build: build-macos-metal
Command: STELLAR_ENABLE_AUDIO=1 STELLAR_AUDIO_SMOKE_CONFIRM=heard build-macos-metal/stellar-audio-smoke --sound footstep_concrete_0 --sound footstep_metal_1 --duration-ms 2500
Result: exit code 0, miniaudio_active=1, operator heard both sounds.

Audible audio smoke completed on 2026-05-06.
Build: build-macos-metal-only
Command: STELLAR_ENABLE_AUDIO=1 STELLAR_AUDIO_SMOKE_CONFIRM=heard build-macos-metal-only/stellar-audio-smoke --sound footstep_wood_0 --sound footstep_water_1 --duration-ms 2500
Result: exit code 0, miniaudio_active=1, operator heard both sounds.
```

## Stop conditions

- If miniaudio initialization fails, do not mark complete.
- If play requests return diagnostics, fix sink/assets first.
