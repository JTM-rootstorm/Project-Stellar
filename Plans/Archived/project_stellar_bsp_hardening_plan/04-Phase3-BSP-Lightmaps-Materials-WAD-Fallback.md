Prepared: 2026-05-01  
Branch target: `bsp-integration`  
Audience: Kilo Code agents / Codex-style implementation agents  
Objective: implement the next required BSP support hardening after the canonical BSP migration.

# Phase 3 — BSP Lightmaps, Textures, Materials, and WAD Fallback

## Purpose

Harden BSP visual presentation by importing or representing classic BSP texture/lightmap data and
providing deterministic fallback behavior for missing or unsupported material resources.

## Primary agents

- `@miyamoto`: renderer/material upload and graphics tests.
- `@carmack`: BSP texture/light lump parsing and source-neutral asset population.
- `@director`: resolves data-contract conflicts.

## Parallelism

Can run in parallel with Phase 2 and Phase 4 after Phase 1. Coordinate any `LevelAsset` changes
through `@director`.

## Files likely touched

- `include/stellar/assets/LevelAsset.hpp`
- `include/stellar/assets/TextureAsset.hpp`
- `include/stellar/assets/ImageAsset.hpp`
- `src/import/bsp/BspBinary.hpp`
- `src/import/bsp/BspParser.cpp`
- `src/import/bsp/BspLevelBuilder.cpp`
- `include/stellar/graphics/RenderLevel.hpp`
- `src/graphics/RenderLevel.cpp`
- `tests/import/bsp/BspImporter.cpp`
- new `tests/import/bsp/BspMaterials.cpp`
- new `tests/import/bsp/BspLightmaps.cpp`
- extended `tests/graphics/RenderSceneUpload.cpp`
- `CMakeLists.txt`
- `docs/ImplementationStatus.md`

## Scope

In scope:

- Embedded miptex texture-name parsing and basic embedded texture extraction when available.
- External texture/WAD reference recording.
- Deterministic fallback material for missing external textures.
- Per-surface lightmap metadata and image upload when source data is valid.
- Use `uv1` or equivalent secondary texture coordinates for lightmaps.
- Display-free tests for imported material/lightmap metadata and renderer upload commands.

Out of scope:

- Full Quake/Half-Life material system emulation.
- Full PBR.
- Source/VBSP lightmaps.
- Runtime lightmap baking.
- GPU pixel-readback validation.

## Import requirements

### Textures/materials

1. Preserve BSP texture names on `LevelSurfaceMaterial::source_name`.
2. Decode embedded miptex image data when present and safe.
3. For external textures:
   - record a material with no texture index;
   - assign a deterministic fallback material name and base color;
   - emit a warning diagnostic, not a fatal error.
4. Optional WAD resolver:
   - add a small resolver interface if low-risk;
   - keep it separate from core parser;
   - do not require WAD files for default tests.

Suggested resolver shape:

```cpp
namespace stellar::import::bsp {

struct MaterialResolverOptions {
    std::vector<std::filesystem::path> wad_paths;
    bool allow_external_wads = true;
};

}
```

If WAD support is too large, explicitly defer WAD decoding but implement stable fallback and docs.

### Lightmaps

1. Read lighting lump bytes into import data.
2. Use each face's light offset and light styles to build per-surface lightmap metadata when valid.
3. Generate lightmap image payloads with deterministic dimensions.
4. Assign `LevelSurfaceMaterial::lightmap_index` where applicable.
5. Populate secondary UVs for lightmap lookup if the renderer uses them.
6. Missing, invalid, or out-of-range light offsets produce diagnostics and fallback to unlit surface
   rendering.

Version notes:

- Keep BSP29/BSP30 behavior explicit in code and tests.
- Do not silently assume Source/VBSP layout.

## Renderer requirements

- `RenderLevel` uploads lightmap textures when present.
- Material upload can represent base texture + lightmap or a clear fallback path.
- OpenGL/Vulkan remain behind `GraphicsDevice`; do not add BSP-specific backend calls.
- If the current `MaterialUpload` cannot represent lightmaps without broad churn, add the smallest
  source-neutral extension and tests.

## Required tests

- Embedded miptex creates an `ImageAsset` and `TextureAsset`.
- Missing external texture produces fallback material and warning.
- Surface material index resolves consistently.
- Valid lighting lump creates lightmap image metadata.
- Out-of-range light offset produces diagnostic and fallback.
- `RenderLevel` creates texture/material uploads for base texture and lightmap where represented.
- Existing billboard ordering tests still pass.

## Acceptance criteria

- BSP maps render with deterministic material fallback instead of relying only on gray default.
- Lightmap metadata is imported or explicitly diagnosed/fallback-safe.
- No full PBR or backend-specific renderer API is introduced.
- Default CTest passes.
- `docs/ImplementationStatus.md` records what is supported and what remains deferred.

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_materials|bsp_lightmaps|bsp_importer|render_level_upload|render_level_inspection)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

## Stop conditions

Stop if lightmaps require a full material-system rewrite. Implement stable data import and fallback
first, then escalate renderer expansion through `@director`.
