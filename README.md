# Project Stellar

Just a fun little project where I make a few AI agents make a game for me to learn a few things.

My only involvement is to tell the Director what needs to be done and reign in hallucinations. Other than that, the AI agents will mostly do as they please with the project and its design.

Please refer to `docs/Design.md` for broad project specifications and scope.

For current branch implementation status, especially BSP/Vulkan/Metal rendering support, see
`docs/ImplementationStatus.md`. Historical roadmap files under `Plans/` and `.kilo/plans/` are
archival and may contain stale intermediate phase descriptions.

Linux development dependencies:

Install CMake, SDL2, GLM, and Vulkan loader/header/tooling packages for your distro. The Vulkan
shader build requires `glslc`; SDK installs usually provide it under `$VULKAN_SDK/bin`.

macOS development dependencies:

```bash
brew install cmake sdl2 glm
```

Default build and tests:

```bash
cmake --preset linux-vulkan-only
cmake --build --preset linux-vulkan-only -j$(nproc)
ctest --preset linux-vulkan-only --output-on-failure
```

Default macOS build and tests:

```bash
cmake --preset macos-metal-only
cmake --build --preset macos-metal-only -j$(sysctl -n hw.ncpu)
ctest --preset macos-metal-only --output-on-failure
```

Fallback manual build and tests:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

Opt-in Metal display smoke on macOS:

```bash
build-macos-metal-only/stellar-client --validate-display --renderer metal
```

Opt-in audible audio smoke on macOS:

```bash
STELLAR_ENABLE_AUDIO=1 STELLAR_AUDIO_SMOKE_CONFIRM=heard \
  build-macos-metal/stellar-audio-smoke \
  --sound footstep_concrete_0 \
  --sound footstep_metal_1 \
  --duration-ms 2500
```

To generate public C++ API reference docs when Doxygen is installed:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_docs_doxygen
```
