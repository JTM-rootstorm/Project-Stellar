# Phase 6C: Billboard Sprite Rendering For Entities

## Objective

Add a backend-neutral and renderer-parity path for 2D billboard sprites in the 3D world.

This phase supports the core engine goal: 3D world/level geometry with 2D sprites representing entities, objects, pickups, projectiles, and decorative props.

## Primary Agent

@miyamoto primary.

Consult @carmack only if entity/world data integration becomes necessary.

Do not modify AGENTS.md.

## Prerequisites

No hard dependency on Phase 6A or Phase 6B.

Recommended order:

- Implement after or alongside Phase 6A if the engine needs visible entities in imported levels.
- Keep sprite rendering independent of static collision.

## Required Reading

Read before editing:

- `AGENTS.md`
- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `include/stellar/graphics/GraphicsDevice.hpp`
- `include/stellar/graphics/RenderScene.hpp`
- `include/stellar/graphics/SceneRenderer.hpp`
- `src/graphics/RenderScene.cpp`
- `src/graphics/SceneRenderer.cpp`
- `src/graphics/opengl/OpenGLGraphicsDevice.cpp`
- `src/graphics/vulkan/VulkanGraphicsDevice*.cpp`
- `tests/graphics/RecordingGraphicsDevice.hpp`
- `tests/graphics/RenderSceneInspection.cpp`
- `tests/graphics/RenderSceneUpload.cpp`
- `CMakeLists.txt`

## Scope

Implement renderer support for billboard sprite draw data.

Required capabilities:

- Sprite quad generated from world position, size, and camera basis.
- Texture + sampler support reusing existing texture/material upload paths where reasonable.
- Alpha mask/blend support.
- OpenGL and Vulkan behavior parity.
- Display-free render inspection tests.
- Optional OpenGL/Vulkan context smoke coverage if small.

## Non-Goals

Do not implement:

- ECS entity system.
- Networking replication.
- Sprite atlas packing tool.
- Sprite sheet animation system.
- Particle system.
- Editor UI.
- Gameplay spawn metadata from glTF. That belongs to Phase 6D.

## Data Model

Preferred new public header:

- `include/stellar/graphics/BillboardSprite.hpp`

Suggested structure:

```cpp
namespace stellar::graphics {

struct BillboardSprite {
    std::array<float, 3> position{};
    std::array<float, 2> size{1.0f, 1.0f};
    std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
    TextureHandle texture{};
    SamplerHandle sampler{};
    bool alpha_blend = true;
    bool alpha_mask = false;
    float alpha_cutoff = 0.5f;
};

}
```

Exact handles may differ based on current `GraphicsDevice` API.

Preferred API addition:

```cpp
virtual void draw_billboards(std::span<const BillboardSprite> sprites,
                             const BillboardView& view) noexcept = 0;
```

Alternative acceptable:

- Convert billboards into transient mesh draw commands inside `RenderScene` or `SceneRenderer`.
- Prefer direct backend API only if it keeps parity and tests simpler.

## Billboard Behavior

Implement camera-facing quads:

- Use camera right/up vectors for spherical billboarding.
- Sprite center is `position`.
- Size is world-space width/height.
- Quad should face camera consistently in both OpenGL and Vulkan.
- Preserve depth testing so sprites can be occluded by level geometry.
- Support alpha blend sorting if needed.

Sorting policy:

- For alpha blend sprites, sort back-to-front by camera-space depth.
- Alpha mask sprites can be drawn with opaque/mask group.
- Keep sorting deterministic in tests.

## Texture Behavior

- Reuse existing texture upload and sampler state if possible.
- Base behavior: sample sprite texture and multiply by vertex/per-sprite color.
- Missing texture fallback may use existing white texture behavior if available.
- Do not require texture atlas support in this phase.

## OpenGL Requirements

- Add shader or extend existing shader path for billboard quads.
- Preserve static glTF render path.
- Preserve alpha mask/blend behavior.
- Keep optional context tests opt-in.

## Vulkan Requirements

- Add matching pipeline/shader support or reuse existing material path if practical.
- Preserve static glTF render path.
- Preserve swapchain/lifetime hardening from prior phases.
- Keep optional Vulkan context tests opt-in.

## Tests

Add display-free tests first.

Required tests:

1. Recording graphics device receives expected billboard draw data.
2. Billboard sorting orders alpha blend sprites back-to-front.
3. Alpha mask and alpha blend sprites are classified separately.
4. Sprite size/color/texture handle are preserved.
5. Static glTF render inspection tests still pass.

Optional context smoke tests:

- One textured alpha-mask billboard.
- One alpha-blend billboard.
- Keep OpenGL/Vulkan context tests opt-in.

## Build System

Update `CMakeLists.txt` for new source files/tests.

Default CTest must remain display-free.

## Validation Commands

Run at minimum:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_render_scene_inspection_test stellar_render_scene_upload_test -j$(nproc)
ctest --test-dir build -R '^(render_scene_inspection|render_scene_upload)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Optional if context tests are touched:

```bash
cmake -S . -B build-vulkan-tests -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON
cmake --build build-vulkan-tests --target stellar_vulkan_context_smoke_test -j$(nproc)
ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure
```

If OpenGL context tests are touched, also run the existing OpenGL opt-in path.

## Acceptance Criteria

- Engine can submit 2D billboard sprites in 3D world space.
- Billboard quads face the camera using a deterministic camera basis.
- Alpha mask/blend behavior is supported.
- OpenGL and Vulkan paths remain parity targets.
- Display-free tests cover draw data and sorting.
- Default tests remain display-free.
- Public APIs touched have Doxygen `@brief` comments.
- Completion notes are appended to this file.

## Completion Notes Template

Append after implementation:

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 6C billboard sprite rendering.
- Public API/data model: <describe structs/functions>.
- OpenGL behavior: <describe implementation>.
- Vulkan behavior: <describe implementation>.
- Sorting/alpha behavior: <describe policy>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Sprite atlas/sheet animation remains future work.
  - ECS/entity integration remains future work.
  - glTF-authored sprite spawn metadata remains Phase 6D.
```
## Completion Notes (2026-04-29)

- Implemented: Phase 6C billboard sprite rendering through backend-neutral sprite data,
  deterministic quad expansion, and transient submission through the existing mesh/material path.
- Public API/data model: added `BillboardSprite`, `BillboardView`, `BillboardQuad`,
  `build_billboard_quads`, and a `RenderScene::render` overload that accepts billboard sprites.
- OpenGL behavior: billboard quads reuse the existing OpenGL mesh/material renderer, including
  texture binding, sampler state, vertex color modulation, alpha mask, alpha blend, depth testing,
  and double-sided unlit rendering.
- Vulkan behavior: billboard quads reuse the existing Vulkan mesh/material renderer and descriptor
  path, preserving the same texture/sampler/material behavior as OpenGL with existing pipeline
  alpha parity.
- Sorting/alpha behavior: alpha-mask sprites are classified with the non-blend group; alpha-blend
  sprites are sorted back-to-front by view-space Z with submission index as the deterministic
  tie-breaker.
- Tests added/updated: `RenderSceneInspection.cpp` now covers billboard quad generation, field
  preservation, alpha mask/blend classification, blend sorting, and recording-device mesh/material
  submission order.
- Validation:
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON`
  - `cmake --build build --target stellar_render_scene_inspection_test stellar_render_scene_upload_test -j$(nproc)`
  - `ctest --test-dir build -R '^(render_scene_inspection|render_scene_upload)$' --output-on-failure`
  - `cmake --build build -j$(nproc)`
  - `ctest --test-dir build --output-on-failure`
  - Result: pass.
- Deferred follow-up:
  - Sprite atlas/sheet animation remains future work.
  - ECS/entity integration remains future work.
  - glTF-authored sprite spawn metadata remains Phase 6D.
  - A dedicated persistent billboard batch path can replace transient mesh/material submission when
    sprite counts become performance-critical.
