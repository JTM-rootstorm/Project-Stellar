---
phase: FMP-7
title: Audio Parity and macOS Runtime Smoke
depends_on: [FMP-1]
can_parallelize: true
---

# FMP-7 — Audio Parity and macOS Runtime Smoke

## Goal

Make audio behavior equivalent across Linux and macOS while keeping default tests audio-device-free.

## Tasks

1. Verify miniaudio sink behavior on macOS:
   - no `STELLAR_ENABLE_AUDIO`: no-op sink.
   - `STELLAR_ENABLE_AUDIO=1`: miniaudio sink attempts device init.
   - failure returns diagnostics and no gameplay failure.
2. Add optional audible smoke:
   ```bash
   STELLAR_ENABLE_AUDIO=1 build-macos-metal/stellar-client --map <walking_fixture> --renderer metal
   ```
3. Add no-device tests for registry entries, missing asset diagnostics, unknown sound ID diagnostics, and uninitialized sink diagnostics.
4. Add a decode-only WAV load test if practical.
5. Validate `STELLAR_MINIAUDIO_NO_RUNTIME_LINKING=ON` on macOS.
6. Ensure `stellar-server` does not link miniaudio.

## Acceptance criteria

- Linux and macOS default tests pass with no audio device.
- macOS optional audio smoke works or reports clean device diagnostics.
- Notarization-oriented miniaudio framework-link build configures and builds.
- Server/headless paths never require audio.

## Validation

```bash
ctest --test-dir build -R '^(audio_event_router|miniaudio_sink|generated_footstep_assets|footstep_audio_pipeline)$' --output-on-failure
ctest --test-dir build-macos-metal -R '^(audio_event_router|miniaudio_sink|generated_footstep_assets|footstep_audio_pipeline)$' --output-on-failure

cmake -S . -B build-macos-audio-frameworks -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_MINIAUDIO_NO_RUNTIME_LINKING=ON
cmake --build build-macos-audio-frameworks -j$(sysctl -n hw.ncpu)
```
