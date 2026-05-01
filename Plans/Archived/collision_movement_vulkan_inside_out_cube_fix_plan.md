---
title: "AI Agent Patch Plan: Vulkan Inside-Out Debug Cube Fix"
repo: "JTM-rootstorm/Project-Stellar"
branch: "collision-movement"
target_file: "src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp"
status: "ready-for-implementation"
priority: "high"
issue: "Vulkan debug cube now renders, but appears inside-out: user sees interior/back faces instead of exterior surfaces."
---

# AI Agent Patch Plan: Vulkan Inside-Out Debug Cube Fix

## 0. Mission

Fix the Vulkan rendering state so the default spinning debug cube shows its outside surfaces, matching OpenGL.

Current status:

```yaml
previous_present_issue: fixed
previous_descriptor_layout_issue: fixed
current_result:
  vulkan_cube_visible: true
  vulkan_cube_orientation: inside_out
  visual_symptom: "Looks like viewing through the cube from inside a room"
```

This symptom strongly indicates a front-face/culling mismatch in the Vulkan graphics pipeline.

## 1. Scope

Primary target:

```text
src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
```

Do not modify:

```text
src/graphics/DebugCubeMesh.cpp
src/graphics/SceneRenderer.cpp
src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp
src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp
```

The cube mesh should not be flipped. The mesh currently builds outward normals and consistent face winding. Vulkan should adapt pipeline rasterization state to the shared mesh conventions, not fork the mesh data.

## 2. Branch Context

The current branch already includes:

```yaml
scene_renderer_vulkan_projection_fix:
  file: src/graphics/SceneRenderer.cpp
  state: present
  behavior:
    - Vulkan projection flips Y
    - Vulkan projection remaps OpenGL depth [-1, 1] to Vulkan depth [0, 1]

descriptor_layout_fix:
  file: src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp
  state: present
  behavior:
    - material descriptor binding 5 is VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    - skin draw descriptor binding 0 is VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
```

Because the cube now renders, the descriptor path and draw submission path are working. The remaining visual inversion should be handled in pipeline rasterization/depth state.

## 3. Root Cause Hypothesis

The Vulkan pipeline currently appears to use:

```cpp
.frontFace = VK_FRONT_FACE_CLOCKWISE
```

The user now sees what looks like the inside/back sides of the cube. That means the front faces are likely being classified incorrectly and culled, leaving the back/interior sides visible.

With the current branch's projection correction in `SceneRenderer.cpp`, Vulkan should use:

```cpp
.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
```

for normal culled pipelines.

Expected normal Vulkan rasterizer state:

```cpp
.cullMode = VK_CULL_MODE_BACK_BIT,
.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
```

Double-sided material variants should still use:

```cpp
.cullMode = VK_CULL_MODE_NONE,
```

## 4. Implementation Tasks

## Task T1: Locate Vulkan rasterization state

### File

```text
src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
```

### Search commands

```bash
grep -n "frontFace" src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
grep -n "cullMode" src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
grep -n "VkPipelineRasterizationStateCreateInfo" src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
grep -n "VK_FRONT_FACE" src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
grep -n "VK_CULL_MODE" src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
```

### Locate

Find the `VkPipelineRasterizationStateCreateInfo` used to create:

```yaml
pipelines_to_check:
  - graphics_pipeline_
  - graphics_pipeline_double_sided_
  - alpha_blend_pipeline_
  - alpha_blend_pipeline_double_sided_
```

The exact implementation may use a helper function that accepts `double_sided`, `alpha_blend`, or similar parameters. If so, patch the shared rasterization state builder rather than individual duplicated blocks.

---

## Task T2: Change front-face classification for culled Vulkan pipelines

### Primary patch

Change:

```cpp
.frontFace = VK_FRONT_FACE_CLOCKWISE,
```

to:

```cpp
.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
```

### Expected final rasterizer state

For opaque/alpha culled pipelines:

```cpp
const VkPipelineRasterizationStateCreateInfo rasterization_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
    .lineWidth = 1.0F,
};
```

If the code uses a `double_sided` flag:

```cpp
.cullMode = double_sided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT,
.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
```

### Notes

- Keep `VK_CULL_MODE_BACK_BIT` for normal one-sided rendering.
- Do not switch to `VK_CULL_MODE_FRONT_BIT`.
- Do not leave `VK_CULL_MODE_NONE` as the final fix unless explicitly required by material double-sidedness.
- Do not modify vertex order in `DebugCubeMesh.cpp`.

---

## Task T3: Confirm double-sided pipeline behavior

If the pipeline builder creates both single-sided and double-sided variants:

```yaml
single_sided_variants:
  cullMode: VK_CULL_MODE_BACK_BIT
  frontFace: VK_FRONT_FACE_COUNTER_CLOCKWISE

double_sided_variants:
  cullMode: VK_CULL_MODE_NONE
  frontFace: VK_FRONT_FACE_COUNTER_CLOCKWISE # harmless when culling disabled
```

Do not set all pipelines to `VK_CULL_MODE_NONE`; that hides the root cause and reduces rendering correctness for glTF materials.

---

## Task T4: Verify depth state is conventional

While editing the same file, inspect `VkPipelineDepthStencilStateCreateInfo`.

Expected:

```cpp
.depthTestEnable = VK_TRUE,
.depthWriteEnable = VK_TRUE,
.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
```

Acceptable alternative:

```cpp
.depthCompareOp = VK_COMPARE_OP_LESS,
```

Do not use these for normal depth unless the whole renderer is intentionally reversed-Z:

```cpp
VK_COMPARE_OP_GREATER
VK_COMPARE_OP_GREATER_OR_EQUAL
```

Also confirm the frame clear still uses depth `1.0F`. If depth clear is `1.0F`, a greater-than compare would be wrong for normal depth.

No depth change is required if current code already uses `LESS` or `LESS_OR_EQUAL`.

## 5. Fast Diagnostic Option

If there is uncertainty before changing `frontFace`, perform this temporary diagnostic:

```cpp
.cullMode = VK_CULL_MODE_NONE,
```

Run Vulkan.

Expected diagnostic outcomes:

```yaml
cube_looks_correct_with_culling_disabled:
  conclusion: "frontFace/culling mismatch confirmed"
  final_action:
    - revert temporary VK_CULL_MODE_NONE
    - set frontFace to VK_FRONT_FACE_COUNTER_CLOCKWISE
    - keep cullMode VK_CULL_MODE_BACK_BIT for single-sided pipelines

cube_still_inside_out_with_culling_disabled:
  conclusion: "not a culling problem; inspect normals/material/shader/depth"
  final_action:
    - revert temporary VK_CULL_MODE_NONE
    - inspect depth compare and fragment normal handling
```

Do not commit the temporary culling-disable diagnostic unless requested.

## 6. Validation

Run from repository root.

### 6.1 Build

```bash
cmake --build build -j"$(nproc)"
```

If no existing build directory exists:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
```

### 6.2 Tests

```bash
ctest --test-dir build --output-on-failure
```

### 6.3 Vulkan validation run

```bash
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/stellar-client --renderer vulkan
```

Expected:

```yaml
validation:
  descriptor_errors: none
  pipeline_layout_errors: none
  front_face_errors: none
  draw_errors: none
```

### 6.4 Visual comparison

Run:

```bash
./build/stellar-client --renderer opengl
./build/stellar-client --renderer vulkan
```

Expected:

```yaml
opengl:
  cube_visible: true
  cube_spins: true
  surfaces: exterior

vulkan:
  cube_visible: true
  cube_spins: true
  surfaces: exterior
  no_inside_out_room_effect: true
```

### 6.5 Rotation check

Observe the cube for several seconds while it rotates.

Acceptance visual cues:

```yaml
visible_behavior:
  - front/outside faces are visible
  - back/interior faces are not incorrectly favored
  - face colors remain stable and comparable to OpenGL
  - cube does not disappear at certain rotations
```

## 7. Acceptance Criteria

Accept the patch if:

```yaml
acceptance_criteria:
  - "Vulkan cube renders exterior surfaces rather than appearing inside-out."
  - "OpenGL cube remains unchanged."
  - "Vulkan single-sided pipelines use VK_CULL_MODE_BACK_BIT."
  - "Vulkan single-sided pipelines use VK_FRONT_FACE_COUNTER_CLOCKWISE."
  - "Double-sided pipelines keep VK_CULL_MODE_NONE."
  - "No validation-layer errors are introduced."
  - "Tests pass or failures are documented as unrelated/pre-existing."
```

## 8. Rollback Plan

Rollback only this patch if it makes the Vulkan cube disappear or makes culling clearly worse:

```bash
git checkout -- src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
```

Or manually restore:

```cpp
.frontFace = VK_FRONT_FACE_CLOCKWISE,
```

Do not rollback descriptor or swapchain/present fixes. Those solved earlier issues and are unrelated to this inside-out visual symptom.

## 9. If Counter-Clockwise Does Not Fix It

If changing `frontFace` to `VK_FRONT_FACE_COUNTER_CLOCKWISE` does not fix the cube:

### D1: Check cull mode

Ensure no normal pipeline uses:

```cpp
.cullMode = VK_CULL_MODE_FRONT_BIT
```

Normal pipeline should use:

```cpp
.cullMode = VK_CULL_MODE_BACK_BIT
```

### D2: Check depth state

Ensure:

```cpp
.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
```

or:

```cpp
.depthCompareOp = VK_COMPARE_OP_LESS
```

and depth clear is:

```cpp
.depthStencil = {1.0f, 0}
```

### D3: Check fragment shader normal handling

If culling and depth are correct but lighting/color still looks like interior faces, inspect whether normals are negated in the Vulkan shader path. Do not change mesh normals first.

### D4: Check projection double-flip

Current `SceneRenderer.cpp` already applies a Vulkan Y flip in projection. Ensure the Vulkan pipeline is not also using a negative viewport height or another Y flip that was added locally.

Search:

```bash
grep -R "height = -" -n src/graphics/vulkan
grep -R "vkCmdSetViewport" -n src/graphics/vulkan
grep -R "VkViewport" -n src/graphics/vulkan
```

If a negative-height viewport is also used, remove one of the Y flips rather than stacking both. Prefer keeping the centralized `SceneRenderer.cpp` projection correction unless there is a strong reason to move the convention into Vulkan viewport state.

## 10. Final Agent Summary

Make Vulkan front-face classification match the now-corrected projection and the shared debug cube mesh:

```diff
- .frontFace = VK_FRONT_FACE_CLOCKWISE,
+ .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
```

Keep one-sided pipelines back-face culled:

```cpp
.cullMode = VK_CULL_MODE_BACK_BIT,
```

Keep double-sided pipelines uncullled:

```cpp
.cullMode = VK_CULL_MODE_NONE,
```

Then validate against OpenGL visually. The cube should render as a normal exterior solid, not as an inside-out room.
