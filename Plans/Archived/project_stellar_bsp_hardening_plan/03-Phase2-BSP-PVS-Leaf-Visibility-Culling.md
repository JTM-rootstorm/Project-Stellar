Prepared: 2026-05-01  
Branch target: `bsp-integration`  
Audience: Kilo Code agents / Codex-style implementation agents  
Objective: implement the next required BSP support hardening after the canonical BSP migration.

# Phase 2 — BSP PVS, Leaf Visibility, and Render Culling

## Purpose

Import classic BSP leaf/PVS data and use it as an optional render-culling path for static level
surfaces. This must not affect authoritative collision, movement, triggers, Lua, or object colliders.

## Primary agents

- `@carmack`: BSP parser and source-neutral visibility data.
- `@miyamoto`: `RenderLevel` visibility-culling path.
- `@director`: sequencing and conflict resolution.

## Parallelism

Can run in parallel with Phase 3 and Phase 4 after Phase 1 lands.

Recommended split:

- Workstream A (`@carmack`): parser/import tests for nodes, leaves, marksurfaces, PVS decompression.
- Workstream B (`@miyamoto`): renderer-side culling using synthetic `LevelAsset` visibility data.
- Merge only after both pass independently.

## Files likely touched

- `src/import/bsp/BspBinary.hpp`
- `src/import/bsp/BspParser.cpp`
- `src/import/bsp/BspLevelBuilder.cpp`
- `include/stellar/assets/LevelAsset.hpp`
- `include/stellar/graphics/RenderLevel.hpp`
- `src/graphics/RenderLevel.cpp`
- `tests/import/bsp/BspImporter.cpp`
- new `tests/import/bsp/BspVisibility.cpp`
- new or extended `tests/graphics/RenderSceneInspection.cpp`
- new or extended `tests/graphics/RenderSceneUpload.cpp`
- `CMakeLists.txt`
- `docs/ImplementationStatus.md`

## Import requirements

1. Parse classic BSP node, leaf, marksurface, and visibility lumps.
2. Preserve leaf bounds, contents, marksurface references, and visibility offsets.
3. Map marksurfaces to `LevelSurface` indices.
4. Decompress classic BSP PVS deterministically:
   - nonzero byte copies through;
   - zero byte followed by count emits `count * 8` zero visibility bits;
   - truncated zero-runs produce a diagnostic or fatal parse error, depending on severity.
5. Provide safe fallback:
   - if no visibility lump exists, `visibility.available = false` and all surfaces render;
   - if camera leaf cannot be determined, render all surfaces;
   - if PVS data is invalid, report diagnostics and render all surfaces.

## Runtime/render requirements

Add a small backend-neutral visibility query helper. Prefer pure functions over renderer-owned logic.

Suggested API location:

```cpp
namespace stellar::assets {
    std::optional<std::size_t> find_level_leaf_for_point(
        const LevelVisibilityAsset& visibility,
        std::array<float, 3> position) noexcept;

    std::vector<bool> visible_level_surfaces_from_leaf(
        const LevelAsset& level,
        std::size_t leaf_index);
}
```

Renderer behavior:

- `RenderLevel` may accept an optional camera/world position for visibility culling.
- When available, queue only visible surfaces.
- When unavailable or invalid, queue all surfaces exactly as today.
- Culling must be deterministic and display-free testable.
- Do not make PVS a gameplay/collision source of truth.

## Required tests

### Import tests

- BSP fixture with two leaves and PVS data.
- Marksurfaces map to expected `LevelSurface` indices.
- PVS decompression handles zero-run compression.
- Truncated PVS data is diagnosed.
- Missing PVS data falls back safely.

### Renderer tests

Using `RecordingGraphicsDevice` or equivalent:

- With no visibility data, all surfaces are submitted.
- With camera in leaf A, only leaf A/PVS-visible surfaces are submitted.
- Invalid camera position or missing leaf falls back to all surfaces.
- Billboard rendering still occurs after static level geometry.
- Vulkan/OpenGL context tests remain opt-in only.

## Acceptance criteria

- Static render-culling exists and is optional.
- Existing gameplay/collision tests are unchanged.
- Default tests remain display-free.
- `docs/ImplementationStatus.md` records Phase 2 completion notes and deferred PVS limitations.

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_visibility|bsp_importer|render_level_upload|render_level_inspection|bsp_playable_world_smoke)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

## Stop conditions

Stop if visibility culling would require renderer-specific APIs, client-side gameplay decisions, or
collision filtering. PVS is presentation-only.
