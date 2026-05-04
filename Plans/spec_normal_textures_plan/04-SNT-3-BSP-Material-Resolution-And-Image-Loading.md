# SNT-3 — BSP Material Resolution and Image Loading

**Depends on:** SNT-1, SNT-2  
**Must not commit:** yes  
**Purpose:** Resolve sidecar files into source-neutral BSP level material data and load referenced images.

---

## 1. Design Goal

BSP texture names become material keys.

When a BSP face uses texture `dev/wall_96`, import should:

1. Continue resolving the base BSP/WAD/developer texture exactly as today.
2. Search for `materials/dev/wall_96.stellar_material`.
3. If found and valid, load referenced images.
4. Append those images/textures/samplers to `LevelGeometryAsset`.
5. Attach a sidecar-resolved `MaterialAsset` to the matching `LevelSurfaceMaterial`.
6. Preserve lightmap indexes already attached to the BSP surface material.

---

## 2. Image Loading Strategy

The current branch does not expose a general image-file loader in CMake. Add one of these:

### Preferred Option A — Small Import-Scoped Image Loader

Add:

```text
src/import/bsp/ImageFileLoader.hpp
src/import/bsp/ImageFileLoader.cpp
```

Use a vendored single-header image decoder, preferably `stb_image.h`, under:

```text
thirdparty/stb/stb_image.h
```

CMake:

- Keep decoding implementation private to `stellar_import_bsp`.
- Do not make graphics depend on stb directly.
- Do not add runtime GPU/display requirements.

Supported initial formats:

- PNG
- JPG only if stb gives it for free, but tests should use PNG.
- Optional future: TGA/BMP. Do not scope it unless trivial.

### Acceptable Option B — General `stellar_asset_io`

Create a new `stellar_asset_io` library that depends only on `stellar_assets`, then link `stellar_import_bsp` to it.

Use this only if the agent expects image loading to be reused immediately outside BSP import.

---

## 3. Texture/Sampler Creation Rules

For every sidecar texture map:

- Load image into `LevelGeometryAsset::images`.
- Add `TextureAsset` to `LevelGeometryAsset::textures`.
- Add `SamplerAsset` to `LevelGeometryAsset::samplers`.

Sampler defaults:

```text
base_color: linear mipmap linear, repeat
normal: linear mipmap linear, repeat
specular: linear mipmap linear, repeat
metallic_roughness: linear mipmap linear, repeat
occlusion: linear mipmap linear, repeat
emissive: linear mipmap linear, repeat
lightmap: keep existing linear clamp path
```

Color spaces:

```text
base_color: sRGB
emissive: sRGB
normal: linear
specular: linear
metallic_roughness: linear
occlusion: linear
```

Image validation:

- Reject zero dimensions.
- Reject unsupported channel count.
- Normalize decoded images to `ImageFormat::kR8G8B8A8` unless the existing upload path has a better supported format.
- Ensure pixel buffer size matches dimensions.

---

## 4. BSP Import Integration

Modify `src/import/bsp/BspTextureResolver.cpp` and/or `src/import/bsp/BspLevelBuilder.cpp`.

Recommended flow:

1. Keep `texture_asset_index()` focused on existing BSP/WAD/developer base texture resolution.
2. Keep `material_index()` responsible for stable material indexing.
3. After all base materials are created, or during material creation if simpler:
   - resolve sidecar by `source_name`.
   - create/update `LevelSurfaceMaterial::resolved_material`.
   - populate `resolved_material->base_color_texture`:
     - sidecar `base_color` if present
     - otherwise existing `LevelSurfaceMaterial::texture_index`
   - populate `normal_texture`, `specular_texture`, `metallic_roughness_texture`, `occlusion_texture`, `emissive_texture` from loaded sidecar textures.
   - preserve `lightmap_index` on `LevelSurfaceMaterial`, not inside `MaterialAsset`.
4. Avoid duplicate image loads:
   - cache sidecar parse results by normalized material key.
   - cache texture path loads by canonical resolved path.
5. Preserve deterministic ordering:
   - material order remains tied to existing BSP face traversal.
   - images/textures/samplers are appended in stable key order or first-use order. Pick one and test it.

---

## 5. Fallback Behavior

No sidecar:

- Exact current behavior.

Valid sidecar without base_color:

- Use current BSP/WAD/developer base texture as `base_color_texture`.

Valid sidecar with base_color:

- Override base color texture only for that material.

Valid sidecar with normal but no tangents:

- Preserve material data.
- Renderer must skip normal mapping when primitive lacks tangents.
- Add debug/diagnostic coverage, but do not fail import.

Invalid sidecar:

- Non-strict mode: warn and preserve fallback material.
- Strict mode: fail import with stable error.

Missing referenced texture:

- Non-strict mode: warn and ignore that specific sidecar, preserving fallback material.
- Strict mode: fail import.

---

## 6. Tests

Add fixture files:

```text
tests/fixtures/materials/dev/wall_96.stellar_material
tests/fixtures/materials/dev/wall_96_normal.png
tests/fixtures/materials/dev/wall_96_spec.png
```

Use tiny images, for example 2x2 or 4x4:

- normal map: flat `{128, 128, 255, 255}`
- specular map: grayscale white/black pattern
- optional base override: simple colored albedo

Test cases:

1. A synthetic/imported BSP material resolves sidecar textures.
2. Sidecar without base color preserves original base texture.
3. Sidecar base color override creates a new sRGB texture.
4. Normal/specular images are linear textures.
5. Missing sidecar does not add diagnostics by default.
6. Missing sidecar texture warns and falls back.
7. Strict missing sidecar texture fails import.
8. Duplicate material references share/copy texture records deterministically according to chosen cache strategy.

Recommended tests:

```text
tests/import/bsp/BspMaterials.cpp
tests/import/bsp/BspLightmaps.cpp
```

---

## 7. Validation Commands

```bash
cmake --build build --target stellar_bsp_materials_test stellar_bsp_lightmaps_test -j$(nproc)
ctest --test-dir build -R '^(bsp_materials|bsp_lightmaps)$' --output-on-failure
tools/dev/check_target_boundaries.sh
```

Phase-close:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

---

## 8. Acceptance Criteria

- Sidecar material data appears in `LevelAsset` without graphics backends.
- Sidecar image textures are loaded with correct color spaces.
- Existing maps without sidecars are unchanged.
- Strict/non-strict behavior is deterministic and tested.
- No client/server/network target boundary is violated.
