---
title: "AI Agent Implementation Plan: Vulkan Debug Cube Regression Fix"
repo: "JTM-rootstorm/Project-Stellar"
branch: "collision-movement"
created_for: "AI agent implementation handoff"
status: "ready-for-implementation"
priority: "high"
issue_summary: "Spinning 3D debug cube renders under OpenGL but is not visible under Vulkan. Runtime logs report vkQueuePresentKHR returned VK_SUBOPTIMAL_KHR."
---

# AI Agent Implementation Plan: Vulkan Debug Cube Regression Fix

## 0. Mission

Fix the Vulkan rendering regression on branch `collision-movement` where the default spinning 3D debug cube renders in OpenGL but does not appear in Vulkan.

The observed log is:

```text
vkQueuePresentKHR failed with VkResult 1000001003 (VK_SUBOPTIMAL_KHR)
```

Treat this message as a useful symptom, not the only root cause. `VK_SUBOPTIMAL_KHR` is recoverable and should not by itself blank the cube. The likely regression is a Vulkan/OpenGL projection-space mismatch, with a secondary swapchain sizing issue causing repeated `VK_SUBOPTIMAL_KHR` logs.

## 1. Constraints

- Work on branch: `collision-movement`.
- Do not rewrite the renderer architecture.
- Do not alter debug cube winding data unless all other fixes fail and tests justify it.
- Do not modify embedded SPIR-V shader blobs unless pipeline/layout validation proves shader incompatibility.
- Keep OpenGL output unchanged.
- Keep Vulkan present recovery behavior robust for `VK_SUBOPTIMAL_KHR` and `VK_ERROR_OUT_OF_DATE_KHR`.
- Prefer minimal, reviewable patches.

## 2. Current Evidence From Branch

### 2.1 `SceneRenderer.cpp`

Current render path builds a single GLM perspective matrix and sends it to both OpenGL and Vulkan:

```cpp
const glm::mat4 projection = glm::perspective(
    glm::radians(kDefaultFovDegrees), aspect, camera.near_plane, camera.far_plane);

scene_.render(width, height, to_array(projection * view), to_array(view));
```

The debug cube path is the fallback when no external scene asset is loaded:

```cpp
using_debug_cube_ = !source_scene_.has_value();
auto scene = source_scene_.has_value() ? std::move(*source_scene_) : create_cube_scene();
```

Implication: Vulkan receives an OpenGL-style projection by default.

### 2.2 `VulkanGraphicsDeviceFrame.cpp`

Current swapchain preferred extent uses logical SDL window size:

```cpp
SDL_GetWindowSize(window_, &window_width, &window_height);
```

Implication: On HiDPI, Wayland, scaled desktops, or some Vulkan surfaces, logical window size may not match drawable framebuffer size. This can create persistent `VK_SUBOPTIMAL_KHR` and rebuild churn.

Current present handling treats `VK_SUBOPTIMAL_KHR` as recoverable but logs it through `vulkan_error`, producing a scary `"failed"` message:

```cpp
result = vkQueuePresentKHR(graphics_queue_, &present_info);
frame_in_progress_ = false;
if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    log_vulkan_message(vulkan_error("vkQueuePresentKHR", result).message);
    mark_swapchain_rebuild_pending();
    current_frame_index_ = (current_frame_index_ + 1) % frames_.size();
    return;
}
```

Implication: the message is misleading. `VK_SUBOPTIMAL_KHR` should trigger swapchain rebuild but should not be presented as a fatal failure.

### 2.3 `VulkanGraphicsDevicePipeline.cpp`

Code search finds `VK_FRONT_FACE_CLOCKWISE` in the Vulkan pipeline file. This is likely intentional if Vulkan Y-flip correction is used. Verify during implementation, but do not assume it is wrong.

## 3. Root-Cause Hypotheses

### Primary Hypothesis: Vulkan clip-space mismatch

GLM's default `glm::perspective` emits an OpenGL-style projection. Vulkan requires different clip-space conventions:

- Vulkan framebuffer coordinate space has inverted Y relative to the common OpenGL path.
- Vulkan normalized device coordinate depth is `[0, 1]`, not OpenGL's `[-1, 1]`.

A closed cube rendered with culling/depth enabled can disappear or be clipped when a raw OpenGL projection is fed into Vulkan.

### Secondary Hypothesis: swapchain extent mismatch

Using `SDL_GetWindowSize()` for Vulkan swapchain sizing can mismatch actual drawable pixel size. This can make `vkQueuePresentKHR` repeatedly return `VK_SUBOPTIMAL_KHR`.

### Tertiary Hypothesis: front-face mismatch after projection correction

If a Vulkan Y flip is added, culled pipeline front-face settings must match the resulting winding. Existing search suggests `VK_FRONT_FACE_CLOCKWISE` is present, which is likely correct after Vulkan clip correction. Verify the actual rasterization state.

## 4. Implementation Overview

Implement three minimal changes:

1. Add a backend-aware projection helper in `src/graphics/SceneRenderer.cpp`.
2. Use `SDL_Vulkan_GetDrawableSize()` for Vulkan swapchain extent in `src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp`.
3. Change `VK_SUBOPTIMAL_KHR` present logging from "failed" semantics to recoverable/suboptimal semantics.

Optional verification-only task:

4. Confirm Vulkan rasterizer front-face remains compatible with the projection correction.

## 5. Task Graph

```yaml
tasks:
  T1_projection_clip_fix:
    file: src/graphics/SceneRenderer.cpp
    depends_on: []
    can_parallelize: false
    risk: medium
    purpose: "Make Vulkan receive Vulkan-compatible projection while preserving OpenGL output."

  T2_swapchain_drawable_extent:
    file: src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp
    depends_on: []
    can_parallelize: true
    risk: low
    purpose: "Use actual Vulkan drawable size for swapchain extent to reduce VK_SUBOPTIMAL_KHR churn."

  T3_present_logging_semantics:
    file: src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp
    depends_on: [T2_swapchain_drawable_extent]
    can_parallelize: false
    risk: low
    purpose: "Stop logging VK_SUBOPTIMAL_KHR as a fatal-looking failure."

  T4_front_face_audit:
    file: src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
    depends_on: [T1_projection_clip_fix]
    can_parallelize: true
    risk: low
    purpose: "Verify culled Vulkan pipelines use the expected front face after Y-flip correction."

  T5_validation:
    files: ["build/test outputs", "manual Vulkan visual check"]
    depends_on: [T1_projection_clip_fix, T2_swapchain_drawable_extent, T3_present_logging_semantics, T4_front_face_audit]
    can_parallelize: false
    risk: low
    purpose: "Confirm OpenGL parity and Vulkan debug cube visibility."
```

## 6. Detailed Patch Plan

---

## T1: Add Vulkan clip-space correction in `SceneRenderer.cpp`

### Target

`src/graphics/SceneRenderer.cpp`

### Change

Add a helper inside the anonymous namespace near `to_array()` / `to_glm_mat4()`.

```cpp
glm::mat4 make_projection_for_backend(GraphicsBackend backend,
                                      float vertical_fov_degrees,
                                      float aspect,
                                      float near_plane,
                                      float far_plane) {
    glm::mat4 projection =
        glm::perspective(glm::radians(vertical_fov_degrees), aspect, near_plane, far_plane);

    if (backend == GraphicsBackend::kVulkan) {
        // Convert GLM/OpenGL clip space to Vulkan clip space:
        // - flip Y for Vulkan framebuffer coordinates
        // - remap depth from OpenGL [-1, 1] to Vulkan [0, 1]
        glm::mat4 gl_to_vk_clip(1.0f);
        gl_to_vk_clip[1][1] = -1.0f;
        gl_to_vk_clip[2][2] = 0.5f;
        gl_to_vk_clip[3][2] = 0.5f;
        projection = gl_to_vk_clip * projection;
    }

    return projection;
}
```

Then replace the current projection construction in `SceneRenderer::render()`:

```cpp
const glm::mat4 projection = glm::perspective(
    glm::radians(kDefaultFovDegrees), aspect, camera.near_plane, camera.far_plane);
```

with:

```cpp
const glm::mat4 projection = make_projection_for_backend(
    backend_, kDefaultFovDegrees, aspect, camera.near_plane, camera.far_plane);
```

### Notes for Agent

- `GraphicsBackend` is already in namespace `stellar::graphics`; no extra namespace qualifier should be necessary inside this file.
- Keep `view` creation unchanged.
- Keep `scene_.render(... to_array(projection * view) ...)` unchanged.
- Do not change the debug cube mesh.
- This fix affects both debug cube and loaded scene rendering under Vulkan.

### Expected Result

- OpenGL remains unchanged.
- Vulkan receives corrected clip-space projection.
- Debug cube should become visible if culling/depth were the cause.

---

## T2: Use Vulkan drawable size for swapchain extent

### Target

`src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp`

### Change

Add the SDL Vulkan header:

```cpp
#include <SDL2/SDL_vulkan.h>
```

Current includes should become similar to:

```cpp
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
```

Replace `current_window_extent_or_pending_rebuild()` implementation with:

```cpp
VkExtent2D VulkanGraphicsDevice::current_window_extent_or_pending_rebuild(int width,
                                                                          int height) noexcept {
    (void)width;
    (void)height;

    int drawable_width = 0;
    int drawable_height = 0;

    if (window_ != nullptr) {
        SDL_Vulkan_GetDrawableSize(window_, &drawable_width, &drawable_height);

        // Conservative fallback for unusual platforms/drivers.
        if (drawable_width <= 0 || drawable_height <= 0) {
            SDL_GetWindowSize(window_, &drawable_width, &drawable_height);
        }
    }

    if (drawable_width <= 0 || drawable_height <= 0) {
        mark_swapchain_rebuild_pending();
        return VkExtent2D{0, 0};
    }

    return VkExtent2D{
        sanitize_dimension(drawable_width, swapchain_extent_.width),
        sanitize_dimension(drawable_height, swapchain_extent_.height),
    };
}
```

### Notes for Agent

- Keep `sanitize_dimension()` use.
- Keep `(void)width; (void)height;` unless wider call chain is refactored.
- Do not remove the zero-size guard.
- Do not force rebuild every frame; rely on existing extent comparison in `begin_frame()`.

### Expected Result

- Swapchain extent better matches actual Vulkan drawable pixels.
- Repeated `VK_SUBOPTIMAL_KHR` logs should reduce on scaled/HiDPI displays.

---

## T3: Treat `VK_SUBOPTIMAL_KHR` as recoverable in present logging

### Target

`src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp`

### Current Code

```cpp
result = vkQueuePresentKHR(graphics_queue_, &present_info);
frame_in_progress_ = false;
if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    log_vulkan_message(vulkan_error("vkQueuePresentKHR", result).message);
    mark_swapchain_rebuild_pending();
    current_frame_index_ = (current_frame_index_ + 1) % frames_.size();
    return;
}
if (result != VK_SUCCESS) {
    initialized_ = false;
    log_vulkan_message(vulkan_error("vkQueuePresentKHR", result).message);
    return;
}
```

### Replace With

```cpp
result = vkQueuePresentKHR(graphics_queue_, &present_info);
frame_in_progress_ = false;

if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    log_vulkan_message("Vulkan swapchain is out of date; scheduling swapchain rebuild");
    mark_swapchain_rebuild_pending();
    current_frame_index_ = (current_frame_index_ + 1) % frames_.size();
    return;
}

if (result == VK_SUBOPTIMAL_KHR) {
    log_vulkan_message("Vulkan swapchain is suboptimal; scheduling swapchain rebuild");
    mark_swapchain_rebuild_pending();
    current_frame_index_ = (current_frame_index_ + 1) % frames_.size();
    return;
}

if (result != VK_SUCCESS) {
    initialized_ = false;
    log_vulkan_message(vulkan_error("vkQueuePresentKHR", result).message);
    return;
}
```

### Optional Improvement

If logs remain noisy, add rate limiting or only log on transition into suboptimal state. Do not add this unless validation shows log spam.

### Expected Result

- `VK_SUBOPTIMAL_KHR` no longer appears as `"failed"`.
- Vulkan backend remains alive and schedules swapchain rebuild.

---

## T4: Audit Vulkan front-face state

### Target

`src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp`

### Goal

Verify rasterizer state for culled pipelines.

Search for:

```cpp
.frontFace =
.cullMode =
VK_FRONT_FACE_CLOCKWISE
VK_FRONT_FACE_COUNTER_CLOCKWISE
```

### Expected State After T1

For standard culled Vulkan pipelines with the Vulkan projection Y flip in T1:

```cpp
.cullMode = VK_CULL_MODE_BACK_BIT,
.frontFace = VK_FRONT_FACE_CLOCKWISE,
```

For double-sided pipelines:

```cpp
.cullMode = VK_CULL_MODE_NONE,
```

Front-face is not relevant when culling is disabled, but it can remain set.

### Decision Rules

- If culled Vulkan pipelines already use `VK_FRONT_FACE_CLOCKWISE`, do not change them.
- If culled Vulkan pipelines use `VK_FRONT_FACE_COUNTER_CLOCKWISE`, switch to `VK_FRONT_FACE_CLOCKWISE` and document why.
- Do not disable culling globally as the primary fix.
- Do not alter the debug cube mesh winding unless all validation still fails.

### Temporary Debug Option

If the cube is still invisible after T1/T2/T3, temporarily force `VK_CULL_MODE_NONE` for the opaque pipeline only to determine whether winding/culling is the remaining issue. Revert the temporary change after diagnosis.

---

## 7. Validation Plan

Run from repo root.

### 7.1 Format/build smoke

```bash
cmake --build build -j"$(nproc)"
```

If no existing build directory is present:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
```

### 7.2 Full tests

```bash
ctest --test-dir build --output-on-failure
```

### 7.3 Vulkan context smoke test

```bash
cmake -S . -B build-vulkan-tests \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON

cmake --build build-vulkan-tests --target stellar_vulkan_context_smoke_test -j"$(nproc)"

ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure
```

### 7.4 Manual visual validation

Run both renderers and compare:

```bash
./build/stellar-client --renderer opengl
./build/stellar-client --renderer vulkan
```

Expected:

- OpenGL still shows the spinning colored debug cube.
- Vulkan shows the same spinning debug cube.
- Vulkan may report swapchain suboptimal during resize/minimize/surface changes, but it should not continuously spam `"failed"` logs.
- The application should remain responsive after window resize.

### 7.5 Resize validation

While running Vulkan:

1. Resize the window larger.
2. Resize the window smaller.
3. Minimize and restore, if platform supports it.
4. Move between monitors with different scale factors, if available.

Expected:

- No crash.
- Swapchain rebuilds successfully.
- Cube remains visible after resize/restore.
- Logs mention recoverable rebuild scheduling, not fatal present failure.

## 8. Acceptance Criteria

The patch is accepted when all of these are true:

```yaml
acceptance_criteria:
  - "OpenGL debug cube remains visible and spinning."
  - "Vulkan debug cube is visible and spinning."
  - "Vulkan does not permanently blank after VK_SUBOPTIMAL_KHR."
  - "VK_SUBOPTIMAL_KHR is logged as recoverable/suboptimal, not as failed."
  - "Vulkan swapchain uses SDL_Vulkan_GetDrawableSize() with SDL_GetWindowSize() fallback."
  - "Full test suite passes or failures are documented as unrelated pre-existing failures."
  - "Vulkan context smoke test passes when enabled on a Vulkan-capable host."
```

## 9. Rollback Plan

If Vulkan still blanks after all tasks:

1. Revert only T1 projection helper.
2. Test Vulkan with temporary `VK_CULL_MODE_NONE`.
3. If disabling culling makes the cube visible, inspect front-face and cube winding.
4. If disabling culling does not help, inspect:
   - vertex input layout vs `MeshPrimitiveRecord` buffers
   - descriptor set validity
   - material fallback descriptor set
   - depth clear/load state
   - viewport/scissor state
   - shader MVP matrix layout expectations

Rollback commands, if using Git:

```bash
git diff
git checkout -- src/graphics/SceneRenderer.cpp
git checkout -- src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp
```

Do not run rollback blindly after partial success. Revert only the failing task.

## 10. Additional Diagnostic Commands

Search relevant code paths:

```bash
grep -R "glm::perspective" -n src include tests
grep -R "SDL_GetWindowSize" -n src include tests
grep -R "SDL_Vulkan_GetDrawableSize" -n src include tests
grep -R "VK_FRONT_FACE" -n src include tests
grep -R "VK_CULL_MODE" -n src include tests
```

Inspect current Vulkan frame/present flow:

```bash
grep -n "current_window_extent_or_pending_rebuild" -n src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp
grep -n "vkQueuePresentKHR" -n src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp
```

Optional validation layers:

```bash
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/stellar-client --renderer vulkan
```

## 11. Agent Notes

- The visible error string is misleading. `VK_SUBOPTIMAL_KHR` is not the same class of failure as `VK_ERROR_OUT_OF_DATE_KHR` or device-loss errors.
- The OpenGL renderer should be treated as the visual oracle for the default cube.
- Projection correction is intentionally centralized in `SceneRenderer` because both debug cube and glTF scene rendering use that path.
- Swapchain extent correction belongs in `VulkanGraphicsDeviceFrame.cpp` because this file already owns window extent detection and swapchain rebuild scheduling.
- Front-face state should be audited after projection correction because a Y flip changes apparent winding.
- Avoid broad rewrites. The plan is meant to be a focused regression fix.
