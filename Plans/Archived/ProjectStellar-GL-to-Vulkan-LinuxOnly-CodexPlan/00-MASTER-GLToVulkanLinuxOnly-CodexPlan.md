---
repo: "JTM-rootstorm/Project-Stellar"
branch: "GL-to-vulkan"
created_for: "Codex implementation handoff"
created_on: "2026-05-06"
status: "ready-for-implementation"
scope: "Linux Vulkan renderer replacement; macOS remains Metal"
---

# MASTER — Replace Linux OpenGL With Vulkan, Keep macOS On Metal

## 0. Mission

Implement Vulkan as the Linux renderer path for Project Stellar on branch `GL-to-vulkan`, then retire OpenGL once Vulkan-on-Linux and Metal-on-macOS validation are both stable.

This is a Linux-only Vulkan plan.

macOS must continue using the existing Metal backend. Do not add MoltenVK, do not add macOS Vulkan presets, and do not require a Vulkan SDK for macOS builds.

## 1. Current Branch Context To Preserve

Observed current architecture on `GL-to-vulkan`:

- `GraphicsDevice` is already backend-neutral and owns mesh, texture, material, frame, draw, readback-adjacent, and destruction operations.
- `LevelRenderer` already selects projection conventions by backend.
- `Application.cpp` already routes SDL window flags by backend.
- `CMakeLists.txt` already gates OpenGL and Metal backends independently.
- Metal exists behind `STELLAR_ENABLE_METAL=ON` on Apple platforms.
- OpenGL is still present and currently default where enabled.
- Completed historical docs say Vulkan was removed. This branch intentionally reverses that for Linux only.

## 2. Desired Final Renderer Policy

Final supported renderer matrix:

| Platform | Default Renderer | Optional Renderer | Explicitly Unsupported |
|---|---|---|---|
| Linux | Vulkan | none after OpenGL retirement | Metal, macOS Vulkan/MoltenVK |
| macOS | Metal | none after OpenGL retirement | Vulkan, MoltenVK, OpenGL production support |
| Other platforms | Not in scope | none | Vulkan until explicitly scoped |

During migration:

- Linux may temporarily build both Vulkan and OpenGL.
- macOS may temporarily build Metal and/or legacy OpenGL if existing presets require it, but final macOS direction is Metal.
- Vulkan must be compile-gated off on Apple platforms.

## 3. Hard Guardrails

Codex must preserve these:

- Do not add Vulkan support to macOS.
- Do not add MoltenVK setup, docs, presets, runtime paths, or portability-enumeration code.
- Do not call `find_package(Vulkan)` on Apple platforms.
- Do not require Vulkan SDK, Vulkan loader, Vulkan headers, or Vulkan validation layers for macOS configure/build/test.
- Do not remove or regress the Metal backend.
- Do not move gameplay, scripting, collision, networking, audio, or server code toward renderer dependency.
- Do not make default tests require a GPU/display.
- Keep display/GPU Vulkan tests opt-in and skip cleanly with CTest return code `77`.
- Preserve backend-neutral upload contracts.
- Keep public APIs documented with Doxygen `@brief`.
- Use `std::expected<T, stellar::platform::Error>` for fallible setup and upload paths.
- Keep frame/destroy methods `noexcept` where they currently are.
- Do not commit unless the user explicitly asks.

## 4. Phase Map

| Phase | Name | Purpose | Parallelism |
|---|---|---|---|
| VK-0 | Baseline and branch truth | Record current state and active docs that must change. | Do first. |
| VK-1 | Linux Vulkan build/dependency/presets | Add Linux-only CMake gates and presets without touching macOS Metal. | Can overlap with docs audit after VK-0. |
| VK-2 | Backend selection/platform routing | Add `kVulkan`, parser aliases, factory, and Linux-only SDL Vulkan window flags. | After VK-1 interface decisions. |
| VK-3 | Linux Vulkan device scaffold | Real instance/surface/device/swapchain clear-present path. | No, core bring-up. |
| VK-4 | SPIR-V shader pipeline | Add GLSL-to-SPIR-V build flow and pipeline modules. | Can overlap late VK-3 once ABI is agreed. |
| VK-5 | Resource upload and descriptors | Mesh buffers, textures, samplers, material descriptors. | After VK-3. |
| VK-6 | Draw path and material parity | Match current OpenGL/Metal material contract. | After VK-5. |
| VK-7 | Frame readback | Implement backend-neutral readback for Vulkan validation. | After VK-6. |
| VK-8 | Tests and validation matrix | Default display-free tests plus opt-in Linux Vulkan GPU tests. | After VK-3, expands through VK-7. |
| VK-9 | Docs, handoff, and OpenGL retirement | Make docs true; retire OpenGL once parity passes. | Last. |

## 5. Final Acceptance State

Final build behavior:

- Linux default preset builds Vulkan by default.
- Linux OpenGL dependency is gone after retirement.
- macOS default/Metal presets do not require Vulkan.
- macOS renderer path remains Metal.
- `stellar-client --renderer vulkan` works on Linux when Vulkan is enabled.
- `stellar-client --renderer vulkan` fails clearly on macOS.
- `stellar-client --renderer metal` works on macOS Metal builds.
- Server/dedicated-server targets remain graphics-free.

Final validation commands:

```bash
cmake --preset linux-vulkan
cmake --build --preset linux-vulkan --parallel $(nproc)
ctest --preset linux-vulkan --output-on-failure
tools/dev/check_target_boundaries.sh

ctest --test-dir build-linux-vulkan -R \
'^(graphics_backend_selection|render_level_upload|render_level_inspection|bsp_materials|bsp_lightmaps|client_cli|client_map_validation|target_boundary)' \
--output-on-failure
```

Opt-in Linux Vulkan display/GPU validation:

```bash
STELLAR_RUN_VULKAN_CONTEXT_TESTS=1 \
ctest --test-dir build-linux-vulkan -R '^vulkan_' --output-on-failure

build-linux-vulkan/stellar-client --validate-display --renderer vulkan
```

macOS Metal validation remains separate:

```bash
cmake --preset macos-metal
cmake --build --preset macos-metal --parallel $(sysctl -n hw.ncpu)
ctest --preset macos-metal --output-on-failure
build-macos-metal/stellar-client --validate-display --renderer metal
```

Final no-macOS-Vulkan audit:

```bash
git grep -n -i \
  -e 'moltenvk' \
  -e 'VK_KHR_portability' \
  -e 'SDL_WINDOW_VULKAN' \
  -e 'STELLAR_ENABLE_VULKAN_BACKEND' \
  -e 'find_package(Vulkan' \
  -- CMakeLists.txt CMakePresets.json include src tests docs README.md Plans/NEXT.md ':!Plans/Archived/**'
```

Expected result:

- Vulkan references are Linux-gated.
- No MoltenVK references.
- No macOS Vulkan presets or runbooks.
- No Apple-side Vulkan package discovery.
