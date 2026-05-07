---
phase: "VK-2"
title: "Backend Selection And Platform Routing"
repo: "JTM-rootstorm/Project-Stellar"
branch: "GL-to-vulkan"
platform_scope: "Linux Vulkan only; macOS Metal preserved"
status: "ready"
---

# Phase VK-2 — Backend Selection And Platform Routing

## Objective

Add Vulkan to runtime backend selection for Linux builds only while preserving Metal as the macOS backend.

## Files To Edit

- `include/stellar/graphics/GraphicsBackend.hpp`
- `src/graphics/GraphicsBackend.cpp`
- `src/graphics/GraphicsDeviceFactory.cpp`
- `src/client/Application.cpp`
- `tests/graphics/BackendSelection.cpp`
- Related CLI tests under `tests/client/` if present

## Backend Enum

Add Vulkan only when compiled:

```cpp
enum class GraphicsBackend {
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
    kVulkan,
#endif
#if defined(STELLAR_ENABLE_OPENGL_BACKEND)
    kOpenGL,
#endif
#if defined(STELLAR_ENABLE_METAL_BACKEND)
    kMetal,
#endif
};
```

Do not expose `kVulkan` on Apple builds.

## Parser

Add aliases only in Vulkan-enabled builds:

- `vulkan`
- `vk`
- `Vulkan`

When Vulkan is requested on a build without `STELLAR_ENABLE_VULKAN_BACKEND`, return an unsupported-backend error.

On macOS, this should fail because Vulkan is not compiled.

## Default Backend Policy

During implementation:

```cpp
GraphicsBackend default_graphics_backend() noexcept {
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
    return GraphicsBackend::kVulkan;
#elif defined(STELLAR_ENABLE_METAL_BACKEND)
    return GraphicsBackend::kMetal;
#elif defined(STELLAR_ENABLE_OPENGL_BACKEND)
    return GraphicsBackend::kOpenGL;
#else
    // Preserve existing fallback style, but this case should be unreachable
    // in valid configured builds.
#endif
}
```

This makes Linux Vulkan builds default to Vulkan and macOS Metal-only builds default to Metal.

If OpenGL remains built in Linux migration builds, Vulkan still wins as default.

## Factory

Add:

```cpp
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"
#endif
```

Then:

```cpp
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
case GraphicsBackend::kVulkan:
    return std::make_unique<vulkan::VulkanGraphicsDevice>();
#endif
```

## SDL Window Flags

In `Application.cpp`, add Linux-only Vulkan window selection:

```cpp
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
case stellar::graphics::GraphicsBackend::kVulkan:
    return SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI;
#endif
```

Do not use `SDL_WINDOW_VULKAN` for Metal.

Do not add Apple-specific Vulkan routing.

## Tests

Update backend-selection tests to cover:

Linux Vulkan build:

- `default_graphics_backend() == kVulkan`
- `parse_graphics_backend("vulkan")` succeeds
- `parse_graphics_backend("vk")` succeeds
- `graphics_backend_available(kVulkan)` is true

Linux Vulkan-disabled/OpenGL build:

- `vulkan`/`vk` fail clearly

macOS Metal build:

- `metal` succeeds
- `vulkan`/`vk` fail clearly
- default is Metal when OpenGL is disabled and Metal is enabled

## Validation

```bash
cmake --preset linux-vulkan
cmake --build --preset linux-vulkan --parallel $(nproc)
ctest --preset linux-vulkan -R '^(graphics_backend_selection|client_cli)' --output-on-failure
```

macOS:

```bash
cmake --preset macos-metal-only
cmake --build --preset macos-metal-only --parallel $(sysctl -n hw.ncpu)
ctest --preset macos-metal-only -R '^(graphics_backend_selection|client_cli)' --output-on-failure
```

## Acceptance Criteria

- Linux Vulkan builds default to Vulkan.
- macOS Metal builds default to Metal.
- Vulkan CLI names fail clearly on macOS.
- No MoltenVK references.
- Existing Metal selection remains intact.
- OpenGL remains temporary fallback only where explicitly enabled.
