# SNT-5 — RenderLevel Material Upload for Sidecar Materials

**Depends on:** SNT-1, SNT-3, SNT-4  
**Must not commit:** yes  
**Purpose:** Make `RenderLevel` upload sidecar-resolved material textures and factors through the backend-neutral `GraphicsDevice` path.

---

## 1. Current State

`RenderLevel` currently:

- Uploads `geometry.meshes`.
- Uploads `geometry.textures`.
- Uploads `geometry.lightmaps`.
- Builds material uploads from `LevelSurfaceMaterial`.
- Resolves base color and lightmap texture bindings.
- Uses a default BSP material path with `unlit=true` and `double_sided=true`.

This phase should preserve all of that and add sidecar-aware material upload.

---

## 2. Upload Design

Add a helper in `src/graphics/RenderLevel.cpp`:

```cpp
std::optional<MaterialTextureBinding> resolve_material_slot_binding(
    const stellar::assets::MaterialTextureSlot& slot,
    const stellar::assets::LevelGeometryAsset& geometry,
    const std::vector<TextureHandle>& texture_handles);
```

Rules:

- If `slot.texture_index` is out of range, return `std::nullopt`.
- If texture handle is missing, return `std::nullopt`.
- Resolve sampler from `geometry.samplers` if present.
- Use `slot.texcoord_set`, defaulting to 0.
- Preserve `slot.transform`.

Material selection:

```cpp
stellar::assets::MaterialAsset material =
    surface_material.resolved_material.has_value()
        ? *surface_material.resolved_material
        : material_asset_for_level_surface(surface_material);
```

Then:

- Apply existing base texture fallback when `material.base_color_texture` is empty and `surface_material.texture_index` exists.
- Always apply lightmap binding from `surface_material.lightmap_index` as today.
- Apply normal/specular/metallic-roughness/occlusion/emissive slots from the resolved material.
- Apply level lighting data as today.

---

## 3. Camera Data for Specular

If SNT-1 added camera fields to `MeshDrawTransforms`, update `RenderLevel` to populate them.

Preferred behavior:

- Derive camera position from the incoming view matrix when `RenderLevelFrame::camera_world_position` is absent.
- Use explicit `camera_world_position` when provided.
- Set `has_camera_world_position=true` for normal level draws when derivation succeeds.
- For billboard paths, no special specular requirement.

Helper:

```cpp
std::optional<std::array<float,3>> camera_position_from_view(const glm::mat4& view);
```

If view matrix inversion fails, leave `has_camera_world_position=false`.

---

## 4. Debugging

Add optional material debug logging behind an environment flag:

```text
STELLAR_DEBUG_MATERIALS=1
```

Debug output should include:

- material index
- source material name
- resolved material name
- base/normal/specular/lightmap texture presence
- `has_tangents` for primitive draw, if available in inspection tests
- normal/specular factor values

Avoid spamming by using the existing frame/debug limits where possible.

---

## 5. Tests

Use fake/capturing graphics device tests.

Existing likely targets:

```text
stellar_render_level_upload_test
stellar_render_level_inspection_test
```

Add coverage:

1. Sidecar-resolved material uploads base + normal + specular + lightmap.
2. Material without sidecar still uploads old base/lightmap only.
3. Normal map binding is present in material upload even if a primitive later lacks tangents; backend decides whether to sample.
4. Specular texture binding is carried to `MaterialUpload`.
5. Material factors are preserved:
   - `normal_scale`
   - `normal_light_strength`
   - `specular_factor`
   - `specular_power`
6. Camera world position is carried in draw transforms when specular path requires it.

---

## 6. Validation Commands

```bash
cmake --build build --target stellar_render_level_upload_test stellar_render_level_inspection_test -j$(nproc)
ctest --test-dir build -R '^(render_level_upload|render_level_inspection)$' --output-on-failure
tools/dev/check_target_boundaries.sh
```

Phase-close:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

---

## 7. Acceptance Criteria

- `RenderLevel` uploads sidecar materials through backend-neutral `MaterialUpload`.
- Old maps without sidecars render/upload exactly as before.
- Lightmap material path remains intact.
- No OpenGL/Vulkan-specific code leaks into import or asset structures.
- Display-free upload/inspection tests cover the new behavior.
