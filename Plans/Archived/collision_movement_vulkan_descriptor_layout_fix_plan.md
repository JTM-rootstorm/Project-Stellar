---
title: "AI Agent Patch Plan: Vulkan Descriptor Layout Mismatch Fix"
repo: "JTM-rootstorm/Project-Stellar"
branch: "collision-movement"
target_file: "src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp"
status: "ready-for-implementation"
priority: "high"
issue: "Vulkan renderer presents without console error, but the default spinning debug cube is still invisible."
---

# AI Agent Patch Plan: Vulkan Descriptor Layout Mismatch Fix

## 0. Mission

Fix the next likely Vulkan rendering blocker after resolving the `VK_SUBOPTIMAL_KHR` present-path issue.

Current symptom:

```text
OpenGL: spinning debug cube visible
Vulkan: no present error, but cube still invisible
```

The likely cause is a descriptor layout mismatch in `VulkanGraphicsDeviceResources.cpp`:

1. Material descriptor layout declares binding 5 as `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`, but material code creates/writes it as a uniform buffer.
2. Skin draw descriptor layout declares binding 0 as `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`, but skin draw code creates/writes it as a storage buffer.

Because the Vulkan draw path binds the skin draw descriptor set for every primitive, including the unskinned debug cube, this mismatch can make all geometry fail to render or transform incorrectly even when presentation succeeds.

## 1. Scope

Modify only:

```text
src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp
```

Do not change:

- `SceneRenderer.cpp`
- Vulkan present/swapchain code
- debug cube mesh data
- embedded SPIR-V blobs
- pipeline culling/front-face state
- material or skin uniform struct layouts

This is a focused descriptor-layout correction.

## 2. Evidence

In `create_descriptor_resources()`:

### 2.1 Material binding mismatch

Current material descriptor layout:

```cpp
bindings[kMaterialTextureSlotCount] = VkDescriptorSetLayoutBinding{
    .binding = kMaterialTextureSlotCount,
    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
};
```

But material buffer creation uses:

```cpp
create_buffer(sizeof(VulkanMaterialUniform),
              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              ...);
```

And material descriptor writes use:

```cpp
.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
```

Therefore material binding 5 must be:

```cpp
VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
```

### 2.2 Skin draw binding mismatch

Current skin draw descriptor layout:

```cpp
const VkDescriptorSetLayoutBinding skin_draw_binding{
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
};
```

But skin draw buffer creation uses:

```cpp
create_buffer(buffer_size,
              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
              ...);
```

And skin draw descriptor writes use:

```cpp
.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
```

Therefore skin draw binding 0 must be:

```cpp
VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
```

## 3. Patch Instructions

## Task T1: Correct material descriptor layout binding 5

### File

```text
src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp
```

### Locate

Inside:

```cpp
std::expected<void, stellar::platform::Error> VulkanGraphicsDevice::create_descriptor_resources()
```

Find:

```cpp
bindings[kMaterialTextureSlotCount] = VkDescriptorSetLayoutBinding{
    .binding = kMaterialTextureSlotCount,
    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
};
```

### Replace with

```cpp
bindings[kMaterialTextureSlotCount] = VkDescriptorSetLayoutBinding{
    .binding = kMaterialTextureSlotCount,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
};
```

### Rationale

Binding 5 is the material uniform buffer binding. It is backed by `VulkanMaterialUniform` and written as `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`, so the descriptor set layout must match.

---

## Task T2: Correct skin draw descriptor layout binding 0

### File

```text
src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp
```

### Locate

Inside the same function:

```cpp
const VkDescriptorSetLayoutBinding skin_draw_binding{
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
};
```

### Replace with

```cpp
const VkDescriptorSetLayoutBinding skin_draw_binding{
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
};
```

### Rationale

The skin draw descriptor set is backed by `VulkanSkinDrawUniform`, but the buffer is created with `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT` and written as `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`. The descriptor set layout must match the pool, write, and shader binding type.

---

## 4. Do Not Change These Matching Sites

After T1/T2, these existing sites should already be correct. Verify but do not alter unless local code differs.

### Material descriptor pool should remain:

```cpp
VkDescriptorPoolSize{
    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = kMaterialDescriptorSetCapacity,
}
```

### Material descriptor write should remain:

```cpp
descriptor_writes[kMaterialTextureSlotCount] = VkWriteDescriptorSet{
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    ...
};
```

### Skin draw descriptor pool should remain:

```cpp
const VkDescriptorPoolSize skin_draw_pool_size{
    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    .descriptorCount = kSkinDrawDescriptorSetCapacity,
};
```

### Skin draw descriptor write should remain:

```cpp
writes[slot] = VkWriteDescriptorSet{
    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    ...
};
```

## 5. Expected Final Descriptor Type Matrix

```yaml
material_descriptor_set_layout:
  binding_0: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
  binding_1: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
  binding_2: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
  binding_3: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
  binding_4: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
  binding_5: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER

material_descriptor_pool:
  combined_image_sampler: true
  uniform_buffer: true

material_descriptor_writes:
  binding_0_to_4: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
  binding_5: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER

skin_draw_descriptor_set_layout:
  binding_0: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER

skin_draw_descriptor_pool:
  storage_buffer: true

skin_draw_descriptor_writes:
  binding_0: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
```

## 6. Validation Steps

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

### 6.2 Test suite

```bash
ctest --test-dir build --output-on-failure
```

### 6.3 Vulkan validation-layer run

Run the Vulkan client with validation layers enabled:

```bash
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/stellar-client --renderer vulkan
```

Expected:

- No descriptor type mismatch warnings.
- No descriptor set layout/write mismatch errors.
- No pipeline layout descriptor compatibility errors.
- Debug cube should become visible.

### 6.4 Manual visual regression check

Run both backends:

```bash
./build/stellar-client --renderer opengl
./build/stellar-client --renderer vulkan
```

Expected:

```yaml
opengl:
  debug_cube_visible: true
  cube_spins: true

vulkan:
  debug_cube_visible: true
  cube_spins: true
  no_vkQueuePresentKHR_failed_suboptimal_spam: true
```

## 7. Acceptance Criteria

Accept this patch if:

```yaml
acceptance_criteria:
  - "Project builds successfully."
  - "Tests pass or failures are documented as unrelated/pre-existing."
  - "Vulkan validation layers no longer report descriptor type/layout mismatch involving material binding 5 or skin draw binding 0."
  - "OpenGL still shows the spinning debug cube."
  - "Vulkan shows the spinning debug cube."
```

## 8. If Cube Still Does Not Render

If validation layers are clean and the cube is still invisible, proceed in this order.

### Diagnostic D1: Verify draw path is not skipping primitives

Temporarily add debug logs around early returns/continues in:

```cpp
VulkanGraphicsDevice::draw_mesh(...)
```

Check:

- `graphics_pipeline_ != VK_NULL_HANDLE`
- `pipeline_layout_ != VK_NULL_HANDLE`
- mesh handle exists in `meshes_`
- primitive vertex/index buffers are non-null
- `primitive.index_count > 0`
- descriptor set is non-null
- `upload_skin_draw_uniform(...)` succeeds
- `vkCmdDrawIndexed(...)` is reached

Remove logs after diagnosis.

### Diagnostic D2: Temporarily disable culling

In `VulkanGraphicsDevicePipeline.cpp`, temporarily force the opaque culled pipeline rasterizer to:

```cpp
.cullMode = VK_CULL_MODE_NONE
```

Do not keep this as the final fix unless no better cause is found.

Interpretation:

```yaml
cube_visible_after_disabling_cull:
  likely_cause: "front-face / winding mismatch"
  next_step: "audit VK_FRONT_FACE_CLOCKWISE vs VK_FRONT_FACE_COUNTER_CLOCKWISE after projection Y flip"

cube_still_invisible_after_disabling_cull:
  likely_cause: "vertex transform, viewport/scissor, descriptor, or shader input issue"
  next_step: "inspect MVP data and vertex input layout"
```

### Diagnostic D3: Verify projection correction

Confirm the previous Vulkan projection fix is present in `SceneRenderer.cpp`.

Vulkan should not receive raw OpenGL clip-space projection. Expected helper shape:

```cpp
if (backend == GraphicsBackend::kVulkan) {
    glm::mat4 gl_to_vk_clip(1.0f);
    gl_to_vk_clip[1][1] = -1.0f;
    gl_to_vk_clip[2][2] = 0.5f;
    gl_to_vk_clip[3][2] = 0.5f;
    projection = gl_to_vk_clip * projection;
}
```

### Diagnostic D4: Inspect vertex input layout

If draw calls are reached and culling is not the issue, compare:

- `stellar::assets::StaticVertex` layout
- Vulkan vertex binding stride
- Vulkan vertex attribute locations
- SPIR-V vertex shader input locations

The embedded SPIR-V declares position at location 0 and several optional attributes. The Vulkan vertex input state must match `StaticVertex`.

## 9. Rollback Plan

To rollback this patch only:

```bash
git checkout -- src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp
```

Or manually restore:

```cpp
// not recommended except for comparison
material binding 5: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
skin draw binding 0: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
```

Rollback should only be used if the descriptor layout correction introduces a build failure that cannot be resolved locally. The original state is internally inconsistent and should not be preserved long-term.

## 10. Final Agent Summary

This is a two-token semantic Vulkan descriptor fix:

```diff
- .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
+ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
```

for material binding 5, and:

```diff
- .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
+ .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
```

for skin draw binding 0.

The goal is to make each descriptor set layout match its buffer usage, descriptor pool type, and descriptor write type. This should remove validation-layer descriptor mismatch errors and may restore the invisible Vulkan debug cube.
