# SNT-7 — Vulkan Parity for Sidecar Normal/Specular Materials

**Depends on:** SNT-5 and stable SNT-6 material contract  
**Must not commit:** yes  
**Purpose:** Keep Vulkan backend behavior in parity with OpenGL for material sidecar upload, normal maps, and lightweight specular lighting.

---

## 1. Current State

The Vulkan backend already has material descriptor infrastructure and default material textures. It currently uses a fixed material texture slot count for:

```text
base_color
normal
metallic_roughness
occlusion
emissive
lightmap
```

Adding explicit specular texture support requires descriptor/uniform/shader parity changes.

---

## 2. Descriptor Layout Changes

Modify Vulkan material texture slot count from 6 to 7.

Recommended slot order:

```text
0 base_color
1 normal
2 metallic_roughness
3 occlusion
4 emissive
5 lightmap
6 specular
```

Alternative acceptable order:

```text
0 base_color
1 normal
2 metallic_roughness
3 occlusion
4 emissive
5 specular
6 lightmap
```

Pick one order and use it consistently in:

```text
src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp
src/graphics/vulkan/VulkanGraphicsDevicePrivate.hpp
src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp
```

Update:

- `kMaterialTextureSlotCount`
- descriptor set layout bindings
- descriptor writes
- fallback texture choice
- material descriptor invalidation
- material uniform transform arrays
- shader binding declarations

Fallback:

- normal slot uses default normal texture.
- all other missing slots use default white texture.
- specular missing slot may use white because `specular_factor=0` disables contribution by default.

---

## 3. Uniform Changes

Update Vulkan material uniform struct to carry new factors:

```text
normal_scale
normal_light_strength
specular_factor
specular_power
camera_world_position
has_camera_world_position
key_light_direction
```

Exact packing is implementation-specific, but must obey std140/std430 alignment rules used by current shader pipeline.

Recommended:

- Keep transform arrays as `vec4`.
- Add one or more `vec4` fields:
  - `lighting_params0 = (normal_scale, normal_light_strength, specular_factor, specular_power)`
  - `camera_params0 = (camera_x, camera_y, camera_z, has_camera ? 1 : 0)`
  - `light_params0 = (light_dir_x, light_dir_y, light_dir_z, 0)`

Update the C++ upload helper that creates `VulkanMaterialUniform`.

---

## 4. Shader Changes

Mirror the OpenGL logic:

- base color
- alpha handling
- normal map sampling with TBN
- baked lightmap + global ambient
- optional normal detail lighting
- optional specular
- emissive

Do not diverge in factor defaults or interpretation.

If Vulkan shaders are embedded as source strings, update source strings. If they are SPIR-V or generated at build time, update the generator/build path accordingly.

---

## 5. Pipeline/Vertex Input

Confirm Vulkan vertex input already consumes:

```text
position
normal
uv0
tangent
uv1
color
```

If not, add missing tangent/uv1/color attributes in parity with OpenGL.

Do not change `StaticVertex` layout in this phase unless SNT-1 already required it. Any layout change requires rebuilding both OpenGL and Vulkan assumptions.

---

## 6. Tests

Default display-free tests:

- Ensure Vulkan code compiles.
- Ensure material upload fake/inspection tests cover specular slot at the backend-neutral layer.
- Ensure descriptor slot constants or uniform packing are covered where feasible without a GPU.

Optional Vulkan context smoke:

```bash
cmake -S . -B build-vulkan -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON
cmake --build build-vulkan --target stellar_vulkan_context_smoke_test -j$(nproc)
ctest --test-dir build-vulkan -R 'vulkan_context_smoke' --output-on-failure
```

Do not make Vulkan context tests part of default CI unless the project already does.

---

## 7. Validation Commands

Default phase validation:

```bash
cmake --build build --target stellar_render_level_upload_test stellar_render_level_inspection_test stellar_graphics_backend_selection_test -j$(nproc)
ctest --test-dir build -R '^(render_level_upload|render_level_inspection|graphics_backend_selection)$' --output-on-failure
tools/dev/check_target_boundaries.sh
```

Phase-close:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Optional Vulkan context smoke as above.

---

## 8. Acceptance Criteria

- Vulkan compiles with the new material descriptor layout.
- Vulkan material descriptors include the specular slot.
- Vulkan uniform factors match OpenGL semantics.
- Vulkan shader logic preserves base/lightmap behavior and adds normal/specular contribution only when enabled.
- Default tests remain display-free.
