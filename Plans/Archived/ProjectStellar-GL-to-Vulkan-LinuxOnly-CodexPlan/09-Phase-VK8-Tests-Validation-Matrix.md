---
phase: "VK-8"
title: "Tests And Validation Matrix"
repo: "JTM-rootstorm/Project-Stellar"
branch: "GL-to-vulkan"
platform_scope: "Linux Vulkan plus macOS Metal regression"
status: "ready"
---

# Phase VK-8 — Tests And Validation Matrix

## Objective

Add enough automated and manual validation to prove Linux Vulkan works while macOS Metal remains stable.

## Default Tests Must Stay Display-Free

Default CTest must not require:

- Vulkan runtime
- GPU
- X11/Wayland display
- Metal display
- audio device

Display/GPU tests must be opt-in and skip with code `77` when unavailable.

## Test Additions

### Display-Free

Update or add tests for:

- backend selection
- Vulkan compiled/uncompiled parser behavior
- Linux default backend is Vulkan when Vulkan is enabled
- macOS default backend is Metal in Metal-only builds
- `render_level_upload`
- `render_level_inspection`
- BSP material slot coverage
- BSP lightmap slot coverage

### Opt-In Linux Vulkan

Add:

```text
tests/graphics/VulkanContextSmoke.cpp
tests/graphics/VulkanRenderReadback.cpp
```

CTest names:

```text
vulkan_context_smoke
vulkan_render_readback
```

Environment gate:

```text
STELLAR_RUN_VULKAN_CONTEXT_TESTS=1
```

Skip behavior:

- no display: skip 77
- no Vulkan runtime/device: skip 77
- no suitable swapchain: skip 77
- validation/runtime failure after device selection: fail nonzero

### macOS Metal Regression

Keep existing Metal tests:

- `metal_shader_compile`
- `metal_context_smoke`
- `metal_render_readback`

Do not add Vulkan tests on macOS.

## Validation Matrix

| Row | Platform | Preset | Renderer | Required By Default | Notes |
|---|---|---|---|---:|---|
| Linux Vulkan | Linux | `linux-vulkan` | Vulkan default | yes | Main target |
| Linux Vulkan-only | Linux | `linux-vulkan-only` | Vulkan only | yes after VK-9 | Proves OpenGL/GLEW removal |
| macOS Metal | macOS | `macos-metal` | Metal | yes on macOS | Existing Metal path |
| macOS Metal-only | macOS | `macos-metal-only` | Metal only | yes on macOS | Proves no OpenGL/Vulkan |
| Linux Vulkan GPU smoke | Linux | `linux-vulkan` | Vulkan | no | Opt-in display/GPU |
| macOS Metal GPU smoke | macOS | `macos-metal` | Metal | no | Existing opt-in/attached display |

## Commands

Linux default:

```bash
cmake --preset linux-vulkan
cmake --build --preset linux-vulkan --parallel $(nproc)
ctest --preset linux-vulkan --output-on-failure
tools/dev/check_target_boundaries.sh
```

Linux Vulkan-only:

```bash
cmake --preset linux-vulkan-only
cmake --build --preset linux-vulkan-only --parallel $(nproc)
ctest --preset linux-vulkan-only --output-on-failure
```

Linux GPU/display opt-in:

```bash
STELLAR_RUN_VULKAN_CONTEXT_TESTS=1 \
ctest --test-dir build-linux-vulkan -R '^vulkan_' --output-on-failure
```

macOS Metal:

```bash
cmake --preset macos-metal
cmake --build --preset macos-metal --parallel $(sysctl -n hw.ncpu)
ctest --preset macos-metal --output-on-failure
build-macos-metal/stellar-client --validate-display --renderer metal
```

## Acceptance Criteria

- Linux Vulkan default CTest passes.
- Linux Vulkan-only build passes once OpenGL is retired.
- macOS Metal builds do not require Vulkan.
- Vulkan tests do not register on macOS.
- Vulkan opt-in tests skip cleanly without display/device.
- Target boundary checks pass.
