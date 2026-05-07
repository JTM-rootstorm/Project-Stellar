---
phase: "VK-3"
title: "Linux Vulkan Device Scaffold"
repo: "JTM-rootstorm/Project-Stellar"
branch: "GL-to-vulkan"
platform_scope: "Linux Vulkan only"
status: "ready"
---

# Phase VK-3 — Linux Vulkan Device Scaffold

## Objective

Implement a real Linux Vulkan `GraphicsDevice` that initializes, clears, presents, and destroys resources safely. This phase does not need full mesh/material rendering yet.

## Files To Add

```text
include/stellar/graphics/vulkan/VulkanGraphicsDevice.hpp
src/graphics/vulkan/VulkanGraphicsDevice.cpp
```

## Public Header Requirements

`VulkanGraphicsDevice` must:

- inherit `stellar::graphics::GraphicsDevice`
- provide Doxygen `@brief` comments
- use PIMPL or private implementation details to avoid exposing Vulkan headers through broad public API if practical
- keep fallible setup returning `std::expected<void, stellar::platform::Error>`

Suggested shape:

```cpp
namespace stellar::graphics::vulkan {

class VulkanGraphicsDevice final : public stellar::graphics::GraphicsDevice {
public:
    VulkanGraphicsDevice();
    ~VulkanGraphicsDevice() noexcept override;

    VulkanGraphicsDevice(const VulkanGraphicsDevice&) = delete;
    VulkanGraphicsDevice& operator=(const VulkanGraphicsDevice&) = delete;

    [[nodiscard]] std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& window) override;

    // Implement all GraphicsDevice methods, with upload/draw stubs returning
    // clear errors until VK-5/VK-6.
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace stellar::graphics::vulkan
```

## Scaffold Responsibilities

Initialize and own:

- `VkInstance`
- optional debug messenger when validation is enabled and layer is available
- `VkSurfaceKHR` created from SDL
- physical device
- logical device
- graphics queue
- present queue
- swapchain
- swapchain images
- swapchain image views
- command pool
- command buffers
- render pass or dynamic rendering setup
- framebuffer/depth resources if needed for clear path
- semaphores/fences

## Linux-Only Guard

The `.cpp` may include:

```cpp
#if defined(__APPLE__)
#error "VulkanGraphicsDevice is Linux-only in Project Stellar; use Metal on macOS"
#endif
```

This should be unreachable if CMake gates correctly.

## SDL Integration

Use SDL Vulkan APIs only in this Linux backend:

- `SDL_Vulkan_GetInstanceExtensions`
- `SDL_Vulkan_CreateSurface`
- `SDL_Vulkan_GetDrawableSize` if needed by available SDL2 version

The window should already have been created with `SDL_WINDOW_VULKAN`.

## Explicit Non-Goals

- No MoltenVK.
- No portability enumeration.
- No macOS surface path.
- No mesh/material rendering yet.
- No readback yet.
- No default CTest GPU requirement.

## Initial Upload Methods

Until VK-5:

- `create_mesh` returns a clear error such as `"Vulkan mesh upload is not implemented yet"`.
- `create_texture` returns a clear error.
- `create_material` may store placeholder handles only if needed for early tests, but do not pretend parity exists.
- `draw_mesh` safely no-ops if resources are not implemented.

## Clear/Present

`begin_frame(width, height)` should:

- acquire swapchain image
- begin command buffer
- clear color/depth target
- handle out-of-date/suboptimal swapchain by requesting recreation

`end_frame()` should:

- end command buffer
- submit
- present
- handle out-of-date/suboptimal swapchain
- never throw

## Validation

Build only:

```bash
cmake --preset linux-vulkan
cmake --build --preset linux-vulkan --parallel $(nproc)
```

Opt-in smoke:

```bash
STELLAR_RUN_VULKAN_CONTEXT_TESTS=1 \
ctest --test-dir build-linux-vulkan -R '^vulkan_context_smoke$' --output-on-failure
```

Manual smoke:

```bash
build-linux-vulkan/stellar-client --validate-display --renderer vulkan
```

## Acceptance Criteria

- Vulkan backend creates a window surface and swapchain on Linux.
- Clear/present path succeeds in opt-in display environment.
- Missing Vulkan runtime/display returns clear skip/error behavior, not crashes.
- macOS builds do not compile or link Vulkan backend.
- Metal display validation remains unaffected.
