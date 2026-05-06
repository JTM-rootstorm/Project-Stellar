---
phase: MC-3
title: macOS Audio Runtime Sink
depends_on: [MC-1]
can_parallelize: true
---

# MC-3 — macOS Audio Runtime Sink

## Goal

Add optional audible playback while preserving the current display/audio-device-free validation path.

## Files likely changed

```text
include/stellar/audio/MiniaudioSink.hpp
src/audio/MiniaudioSink.cpp
include/stellar/audio/AudioEventRouter.hpp        # only if a small registry type is needed
src/client/Application.cpp
CMakeLists.txt
thirdparty/miniaudio/CMakeLists.txt
tests/audio/MiniaudioSink.cpp                     # optional, no-device or temp decode only
tests/cmake/AudioPlatformTests.cmake
docs/ImplementationStatus.md
```

## Tasks

1. Keep `AudioEventRouter` unchanged unless a clean sink registry interface is needed.
2. Add a presentation-only sink:
   ```cpp
   class MiniaudioRequestSink final : public AudioRequestSink {
   public:
       std::expected<void, stellar::platform::Error> initialize(...);
       AudioRequestResult play_one_shot(const AudioPlaybackRequest& request) override;
       void shutdown() noexcept;
   };
   ```
3. Use a sound registry mapping sound IDs to generated WAV paths:
   ```text
   footstep_wood_0 -> assets/audio/footsteps/generated/footstep_wood_0.wav
   ```
4. Missing assets must return `AudioPresentationDiagnostic`, not gameplay failure.
5. Add a client option/env gate:
   ```text
   STELLAR_ENABLE_AUDIO=1
   ```
   Default may remain no-op if audio device init fails, but diagnostics should be printed.
6. CMake:
   - Do not link miniaudio into authority/server/protocol.
   - Link miniaudio only into client/audio presentation target.
   - On macOS command-line builds, link `pthread` and `m` if needed.
   - Add optional notarization-safe mode:
     ```cmake
     option(STELLAR_MINIAUDIO_NO_RUNTIME_LINKING "Use explicit CoreAudio framework links" OFF)
     ```
     If enabled on Apple:
     ```cmake
     target_compile_definitions(miniaudio PUBLIC MA_NO_RUNTIME_LINKING)
     target_link_libraries(miniaudio PUBLIC
       "-framework CoreFoundation"
       "-framework CoreAudio"
       "-framework AudioToolbox"
     )
     ```
7. Default tests must not require an audio device.

## Acceptance criteria

- Default build and tests pass without audio hardware.
- `NoOpAudioRequestSink` remains available and is still the default fallback.
- Optional local macOS audible smoke can play at least one generated footstep sound.
- No server/authority target links miniaudio.

## Validation

```bash
cmake --build build --target stellar_audio_event_router_test stellar_generated_footstep_assets_test -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
ctest --test-dir build -R '^(audio_event_router|generated_footstep_assets|miniaudio_sink)$' --output-on-failure
tools/dev/check_target_boundaries.sh .
```

Optional local audible smoke:

```bash
STELLAR_ENABLE_AUDIO=1 build/stellar-client --map <known-good-bsp>
```
