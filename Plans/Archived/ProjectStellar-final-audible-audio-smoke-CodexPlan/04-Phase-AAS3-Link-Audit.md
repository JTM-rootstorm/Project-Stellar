---
phase: AAS-3
title: Framework Link and Server Audit
depends_on: [AAS-2]
---

# AAS-3 — Framework Link and Server Audit

## Goal

Confirm audible smoke did not leak audio dependencies into server/headless targets.

## Commands

```bash
cmake -S . -B build-macos-audio-frameworks -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_MINIAUDIO_NO_RUNTIME_LINKING=ON
cmake --build build-macos-audio-frameworks --target stellar-audio-smoke stellar-client stellar-server -j$(sysctl -n hw.ncpu)

otool -L build-macos-audio-frameworks/stellar-audio-smoke
otool -L build-macos-audio-frameworks/stellar-client
otool -L build-macos-audio-frameworks/stellar-server

tools/dev/check_target_boundaries.sh .
```

## Expected

- `stellar-audio-smoke` and `stellar-client` may link CoreFoundation/CoreAudio/AudioToolbox.
- `stellar-server` must not link audio frameworks or miniaudio.
- Target boundary checks pass.

## Recorded Evidence

Completed on 2026-05-06:

- `cmake -S . -B build-macos-audio-frameworks -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_MINIAUDIO_NO_RUNTIME_LINKING=ON` configured successfully.
- `cmake --build build-macos-audio-frameworks --target stellar-audio-smoke stellar-client stellar-server -j8` built all three targets successfully.
- `otool -L build-macos-audio-frameworks/stellar-audio-smoke` listed CoreFoundation, CoreAudio, and AudioToolbox.
- `otool -L build-macos-audio-frameworks/stellar-client` listed CoreFoundation, CoreAudio, and AudioToolbox.
- `otool -L build-macos-audio-frameworks/stellar-server` listed only libc++ and libSystem.
- `tools/dev/check_target_boundaries.sh .` passed.
