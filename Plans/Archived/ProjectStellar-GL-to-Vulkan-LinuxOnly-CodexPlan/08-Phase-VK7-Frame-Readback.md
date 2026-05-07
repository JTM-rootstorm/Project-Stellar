---
phase: "VK-7"
title: "Vulkan Frame Readback"
repo: "JTM-rootstorm/Project-Stellar"
branch: "GL-to-vulkan"
platform_scope: "Linux Vulkan only"
status: "ready"
---

# Phase VK-7 — Vulkan Frame Readback

## Objective

Implement `FrameReadbackDevice` for the Linux Vulkan backend so display-attached validation can compare rendered output without manual screenshots.

## Interface

Make `VulkanGraphicsDevice` inherit:

```cpp
public stellar::graphics::FrameReadbackDevice
```

Implement:

```cpp
void request_frame_readback() noexcept override;

[[nodiscard]] std::expected<std::optional<stellar::graphics::FrameReadback>,
                            stellar::platform::Error>
take_frame_readback() noexcept override;
```

## Readback Flow

On requested frame:

1. Render as usual.
2. Transition the swapchain image to transfer source if supported.
3. Copy to CPU-readable staging buffer.
4. Restore swapchain layout for present.
5. Wait or fence safely.
6. Convert staging data to tightly packed RGBA8.
7. Store `FrameReadback`.

If swapchain image cannot be transfer source, render into an offscreen color image that supports transfer source, then blit/copy to swapchain and staging.

## Pixel Format Handling

Support common swapchain formats:

- `VK_FORMAT_B8G8R8A8_UNORM`
- `VK_FORMAT_B8G8R8A8_SRGB`
- `VK_FORMAT_R8G8B8A8_UNORM`
- `VK_FORMAT_R8G8B8A8_SRGB`

Convert to `FrameReadback::rgba_pixels`.

Ensure top-to-bottom row order matches the existing readback contract.

## Client JSON Schema

Update the readback writer to use a backend-neutral schema:

```json
"schema": "stellar.frame_readback.v1"
```

Include:

```json
"backend": "vulkan"
```

Projection should report:

```json
"projection": "vulkan_ndc_z_zero_to_one"
```

## Validation

```bash
build-linux-vulkan/stellar-client \
  --validate-display \
  --renderer vulkan \
  --map tests/fixtures/trenchbroom/out/lit_zup_room.bsp \
  --readback-output /tmp/stellar-vulkan-lit.json
```

If comparison helper exists:

```bash
python3 tools/graphics/compare_readback.py \
  /tmp/stellar-vulkan-lit.json \
  /tmp/stellar-vulkan-lit.json
```

CTest:

```bash
STELLAR_RUN_VULKAN_CONTEXT_TESTS=1 \
ctest --test-dir build-linux-vulkan -R '^vulkan_render_readback$' --output-on-failure
```

## Acceptance Criteria

- Vulkan readback produces valid `FrameReadback`.
- Readback path is opt-in only.
- Normal runtime frames avoid readback overhead.
- JSON report is backend-neutral.
- Metal readback remains functional on macOS.
