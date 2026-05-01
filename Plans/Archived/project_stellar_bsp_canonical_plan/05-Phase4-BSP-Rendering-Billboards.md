# Phase 4 — BSP Rendering and Billboard Preservation

Branch target: `bsp-integration`  
Primary agent: `@miyamoto`  
Coordinator: `@director`  
Dependencies: Phase 1 complete; Phase 2/3 helpful for real fixtures  
Parallelizable: yes after LevelAsset contract is stable. Rendering work can proceed against synthetic LevelAsset while BSP parser matures.

---

## Goal

Render BSP static level geometry and existing billboard sprites through the shared OpenGL/Vulkan graphics abstraction without retired model importer scene graph, skinning, animation, or retired model importer material assumptions.

---

## Required direction

Replace `RenderScene` as a retired model importer-style scene presenter with a BSP/level presenter.

Recommended options:

1. Rename/refactor `RenderScene` → `RenderLevel` or `LevelRenderer`.
2. Keep `RenderScene` name temporarily but change it to consume `assets::LevelAsset` and remove pose/skinning APIs.

Preferred final API:

```cpp
class RenderLevel {
public:
    [[nodiscard]] std::expected<void, platform::Error>
    initialize(std::unique_ptr<GraphicsDevice> device,
               platform::Window& window,
               assets::LevelAsset level);

    void render(int width,
                int height,
                const std::array<float, 16>& view_projection,
                const std::array<float, 16>& view) noexcept;

    void render(int width,
                int height,
                const std::array<float, 16>& view_projection,
                const std::array<float, 16>& view,
                const BillboardView& billboard_view,
                std::span<const BillboardSprite> sprites) noexcept;
};
```

Remove retired model importer pose/skinning-specific render overloads.

---

## GraphicsDevice compatibility

The existing `GraphicsDevice::create_mesh`, `create_texture`, `create_material`, and `draw_mesh` API can remain if BSP geometry is represented as `MeshAsset` primitives.

Do not add BSP-specific raw OpenGL/Vulkan APIs.

Backend-neutral rendering data should include:

- mesh handles for BSP geometry,
- material handles keyed by BSP surface material/texture name,
- optional texture handles,
- optional lightmap texture handles if implemented,
- draw commands sorted by opaque/mask/blend only if relevant,
- billboard sprite draw path preserved.

---

## Material/texture behavior

Initial BSP material target is lightweight:

- Preserve BSP texture/material names.
- Render surfaces with a placeholder/default material if texture decode/resolution is incomplete.
- Use embedded mip texture data when available and straightforward.
- External WAD/material library lookup may be deferred, but missing textures must produce deterministic diagnostics or fallback materials.
- Lightmaps may be parsed and stored before being rendered. If not rendered initially, preserve data and document deferred presentation.

No PBR. No retired model importer metallic-roughness language.

---

## Visibility/PVS behavior

Required minimum:

- Render all BSP surfaces correctly.
- Do not require visibility/PVS for correctness.

Recommended if feasible in this phase:

- Store camera leaf lookup and PVS data in `LevelVisibilityAsset`.
- Add optional surface culling using parsed leaves/PVS.
- Tests should validate culling logic display-free using synthetic data, not GPU readback.

---

## Billboard preservation

Existing billboard APIs should remain source-neutral:

- `BillboardSprite`
- `BillboardView`
- `build_billboard_quads`

Required behavior:

- Billboard sprites render after static opaque BSP geometry.
- Depth testing allows BSP level geometry to occlude sprites.
- Alpha blend sprites remain sorted back-to-front.
- Display-free tests continue to validate quad expansion and sorting.

Do not bind gameplay object state directly into renderer. Client presentation still receives server-approved snapshot/metadata data.

---

## Tests

Update/add display-free graphics tests:

1. `render_level_uploads_bsp_geometry`
2. `render_level_handles_missing_material_with_default`
3. `render_level_preserves_surface_material_indices`
4. `render_level_submits_billboards_after_static_geometry`
5. `billboard_quad_tests_continue_to_pass`
6. Optional: `bsp_visibility_culling_selects_visible_surfaces`

OpenGL/Vulkan context tests remain opt-in.

---

## Files likely touched

- `include/stellar/graphics/RenderScene.hpp` → rename/refactor
- `src/graphics/RenderScene.cpp` → rename/refactor
- `src/graphics/SceneRenderer.cpp` if still used
- `include/stellar/graphics/GraphicsDevice.hpp` only if absolutely needed
- OpenGL backend implementation
- Vulkan backend implementation
- `tests/graphics/RenderSceneUpload.cpp`
- `tests/graphics/RenderSceneInspection.cpp`
- CMake target/test names

Do not modify server/runtime gameplay code except through `@director` coordination.

---

## Acceptance criteria

- BSP/LevelAsset geometry can be uploaded and submitted to both graphics backends through shared abstractions.
- retired model importer scene graph, animation pose, skinning, and material assumptions are no longer part of active renderer APIs.
- Billboard rendering remains intact.
- Default graphics tests are display-free.
- OpenGL/Vulkan context tests remain opt-in.

---

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(render_level|render_scene|billboard|graphics_backend_selection)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

If old `render_scene_*` names are retained temporarily, document that they now refer to level rendering, not retired model importer scenes.

---

## Completion note template

Add to `docs/ImplementationStatus.md`:

```markdown
Phase BSP-4 is complete as of YYYY-MM-DD:

- Refactored rendering to consume BSP/`LevelAsset` static geometry instead of retired model importer scene data.
- Preserved OpenGL/Vulkan backend parity through the shared graphics abstraction.
- Preserved billboard sprite rendering and display-free billboard/render submission tests.
- Removed active renderer assumptions around retired model importer scene graph pose, skinning, animation, and PBR-style materials.

Validation run:

<commands and result>
```
