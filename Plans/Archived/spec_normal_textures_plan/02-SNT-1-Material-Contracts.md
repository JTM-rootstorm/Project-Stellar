# SNT-1 — Material Contracts for Sidecar-Resolved BSP Materials

**Depends on:** SNT-0  
**Must not commit:** yes  
**Purpose:** Extend source-neutral material data so BSP sidecars can express normal/specular lighting without introducing full PBR.

---

## 1. Design Goal

Introduce only the minimal contract needed for this branch:

- Normal map slot and `normal_scale`.
- Lightweight specular map/factor/power.
- Optional extra directional-normal lighting amount for lightmapped BSP surfaces.
- Preserve existing base texture + lightmap behavior.
- Keep all data backend-neutral.
- Keep gameplay/server authority independent from material decisions.

---

## 2. Recommended Contract Changes

### 2.1 `include/stellar/assets/MaterialAsset.hpp`

Add explicit lightweight specular fields.

Recommended additions:

```cpp
std::optional<MaterialTextureSlot> specular_texture;
float normal_scale = 1.0f;
float normal_light_strength = 0.0f;
float specular_factor = 0.0f;
float specular_power = 32.0f;
```

Rules:

- `normal_scale` must be finite and clamped during parsing to a safe range, for example `[0.0, 8.0]`.
- `normal_light_strength` is presentation-only. Use it to make normal maps visible on lightmapped BSP without claiming physically based lighting.
- `specular_factor` defaults to `0.0f`, preserving current behavior unless sidecars opt in.
- `specular_power` defaults to `32.0f`.
- Keep `metallic_factor` and `roughness_factor` for existing material compatibility; do not convert this branch into PBR.

### 2.2 `include/stellar/graphics/MaterialUpload.hpp`

Add a resolved GPU binding for the specular map:

```cpp
std::optional<MaterialTextureBinding> specular_texture;
```

Keep existing normal, metallic-roughness, occlusion, emissive, and lightmap slots.

### 2.3 `include/stellar/assets/LevelAsset.hpp`

Recommended minimal change:

```cpp
std::optional<stellar::assets::MaterialAsset> resolved_material;
```

on `LevelSurfaceMaterial`.

Rationale:

- `LevelSurfaceMaterial` remains the BSP surface material record.
- Existing `name`, `texture_index`, `lightmap_index`, and `source_name` remain useful for compatibility/debugging.
- `resolved_material` allows import-time sidecar data to be attached without replacing old fields.
- `RenderLevel` can use `resolved_material` when present and fall back to current `material_asset_for_level_surface()` behavior otherwise.

Alternative acceptable implementation:

- Add individual normal/specular/factor fields directly to `LevelSurfaceMaterial`.

Do not introduce a graphics handle, OpenGL/Vulkan object, or client-only cache into `LevelAsset`.

### 2.4 `include/stellar/graphics/GraphicsDevice.hpp`

Add camera information only if needed for specular. Recommended:

```cpp
struct MeshDrawTransforms {
    std::array<float, 16> mvp{};
    std::array<float, 16> world{};
    std::array<float, 9> normal{};
    std::array<float, 3> camera_world_position{};
    bool has_camera_world_position = false;
};
```

If the agent chooses a fixed half-vector specular approximation instead, document that decision and do not add camera fields. Preferred path is camera-aware specular.

---

## 3. API Documentation Requirements

All new public API fields must have Doxygen `@brief` comments.

Example:

```cpp
/** @brief Optional grayscale or RGB texture controlling lightweight specular highlight strength. */
std::optional<MaterialTextureSlot> specular_texture;
```

---

## 4. Contract Tests

Add or update display-free tests so the new fields are validated without GPU contexts.

Recommended test coverage:

- `MaterialAsset` defaults preserve old behavior:
  - no specular texture
  - `specular_factor == 0.0f`
  - `normal_scale == 1.0f`
  - `normal_light_strength == 0.0f`
- `LevelSurfaceMaterial` without `resolved_material` still falls back to old rendering material behavior.
- `MaterialUpload` can carry a `specular_texture` binding without breaking fake graphics devices.

Possible locations:

```text
tests/graphics/RenderSceneUpload.cpp
tests/graphics/RenderSceneInspection.cpp
tests/import/bsp/BspMaterials.cpp
```

---

## 5. Validation Commands

Focused validation:

```bash
cmake --build build --target stellar_render_level_upload_test stellar_render_level_inspection_test stellar_bsp_materials_test -j$(nproc)
ctest --test-dir build -R '^(render_level_upload|render_level_inspection|bsp_materials)$' --output-on-failure
tools/dev/check_target_boundaries.sh
```

Phase-close validation:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

---

## 6. Acceptance Criteria

- New material fields compile and are documented.
- Existing BSP material/lightmap tests still pass.
- No default behavior changes when no sidecar is present.
- No target-boundary violations.
- No GPU/display tests required by default.
