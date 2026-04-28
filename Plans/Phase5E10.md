# Phase 5E.10: Vulkan Parity Test Matrix And Completion Gate

## Goal

Declare Phase 5E complete only when the Vulkan glTF renderer’s core behavior is exercised, documented, and validated against the existing OpenGL lightweight material path without making GPU/display tests part of default CTest.

This phase is primarily a test-matrix, documentation, and completion-gate slice. Avoid broad renderer rewrites unless a test exposes a blocker in the Phase 5E.0–5E.9 implementation.

## Design Constraints To Preserve

- Target platform remains Linux-first.
- Keep C++23 and CMake 3.20+.
- Preserve runtime-selectable OpenGL and Vulkan behavior behind the shared graphics abstraction.
- Keep the client presentation-focused; do not introduce server/gameplay responsibilities into graphics tests.
- Preserve OpenGL/Vulkan behavior parity for the current glTF lightweight path.
- Keep GPU/display-dependent Vulkan tests opt-in.
- Keep default `ctest` display-free and suitable for CI without a Vulkan-capable display.
- Public APIs must retain Doxygen `@brief` documentation where touched.
- Use `std::expected<T, stellar::platform::Error>` for fallible setup/upload paths; do not introduce exceptions.
- Keep frame functions `noexcept`.

## Agent Routing

Primary agent: `@miyamoto`

Use `@miyamoto` for all Vulkan, OpenGL comparison, graphics test, shader, fixture, and documentation work.

Only involve `@carmack` if a backend-neutral `GraphicsDevice`, asset, scene, or test infrastructure API must change. Avoid such changes unless absolutely necessary.

Do not delegate work from non-director agents. Do not modify `AGENTS.md`.

## Required Reading Before Editing

Read these files first:

- `AGENTS.md`
- `docs/Design.md`
- `.kilo/plans/1777257747393-clever-tiger.md`
- `CMakeLists.txt`
- `include/stellar/graphics/GraphicsDevice.hpp`
- `include/stellar/graphics/RenderScene.hpp`
- `include/stellar/graphics/SceneRenderer.hpp`
- `include/stellar/graphics/vulkan/VulkanGraphicsDevice.hpp`
- `src/graphics/RenderScene.cpp`
- `src/graphics/SceneRenderer.cpp`
- `src/graphics/opengl/OpenGLGraphicsDevice.cpp`
- `src/graphics/vulkan/VulkanGraphicsDevice.cpp`
- `src/graphics/vulkan/VulkanGraphicsDevicePrivate.hpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceCommon.cpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceDevice.cpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceSwapchain.cpp`
- `src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp`
- `tests/graphics/RenderSceneUpload.cpp`
- `tests/graphics/RenderSceneInspection.cpp`
- `tests/graphics/VulkanContextSmoke.cpp`
- Any Phase 5D generated representative fixture tests/assets that already exist in the repo.

## Current Phase 5E.10 Scope

The existing plan defines Phase 5E.10 as:

- Keep default CTest display-free.
- Add opt-in Vulkan tests for initialization, clear/present, static triangle/cube, textured material, alpha mask/blend, and skinned draw where feasible.
- Reuse generated representative fixtures from Phase 5D to avoid external asset dependencies.
- Add a manual validation checklist for static and skinned glTF assets.
- Update the Phase 5E plan only after completed implementation slices.

This task should turn that into concrete, maintainable test coverage and completion documentation.

## Implementation Tasks

### 1. Audit Existing Phase 5E Coverage

Inspect the current default tests and optional Vulkan tests.

Document what is already covered by:

- `render_scene_upload`
- `render_scene_inspection`
- `graphics_backend_selection`
- `animation_runtime`
- `client_asset_validation_smoke`
- `vulkan_context_smoke` when `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON`

Confirm whether `vulkan_context_smoke` currently covers:

- Vulkan initialization.
- Clear/present frames.
- Static mesh upload and draw.
- Texture upload.
- Material descriptor routing.
- Alpha mask.
- Alpha blend.
- Double-sided material/culling path.
- Skinned primitive draw.
- Over-limit or invalid skin palette safety behavior.
- Resize/recreate path.
- Resource destruction after submitted frames.

Do not assume coverage from names alone. Read the tests and add missing cases.

### 2. Keep Default Tests Display-Free

Verify that the default build does not require a GPU, Vulkan runtime surface, X11/Wayland display, or SDL Vulkan window.

Required behavior:

- `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS` remains default `OFF`.
- `vulkan_context_smoke` is built and registered only when the opt-in CMake flag is enabled.
- Default `ctest --test-dir build --output-on-failure` does not try to create a Vulkan window.
- Default CPU-only graphics tests continue to validate backend-neutral upload/inspection behavior.

Do not reintroduce the old runtime environment gate unless it is needed in addition to the CMake flag for platform stability.

### 3. Expand The Opt-In Vulkan Test Matrix

Update `tests/graphics/VulkanContextSmoke.cpp` or split it into additional opt-in Vulkan smoke files if that improves readability.

Keep all Vulkan runtime tests under `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS`.

The opt-in matrix should exercise, where feasible in the hidden SDL Vulkan window environment:

#### Initialization and clear/present

- Create a hidden `SDL_WINDOW_VULKAN` window.
- Initialize `VulkanGraphicsDevice`.
- Render multiple frames.
- Confirm teardown does not crash.

#### Static triangle/cube draw

- Create at least one generated static mesh with positions, normals, UV0, and indices.
- Create material and texture fixtures.
- Submit draw commands through the normal renderer/device path, not by bypassing public/internal seams unnecessarily.
- Render multiple frames.

#### Textured material path

Exercise:

- Base color texture.
- Vertex color multiplication if supported by generated fixture.
- Normal texture with tangents.
- Metallic-roughness texture and scalar factors.
- Occlusion texture and strength.
- Emissive texture/factor.
- Missing optional texture fallback to default white/normal textures.

The test does not need GPU readback unless the existing repo already has a reliable readback harness. The acceptance level for this phase is smoke coverage that records and submits the expected paths without crashing or logging fatal errors.

#### Alpha mask and alpha blend

Exercise:

- `AlphaMode::kMask` with alpha cutoff.
- `AlphaMode::kBlend` with standard source-alpha blending.
- Opaque/mask and blend paths should both be represented.
- Preserve existing `RenderScene` draw ordering assumptions rather than adding Vulkan-specific ordering.

#### Double-sided and culling variants

Exercise:

- One back-face-culled material.
- One `double_sided` material.
- Confirm all current pipeline variants are reachable in the smoke path.

#### Skinned draw

Exercise:

- A generated skinned primitive with `joints0` and `weights0`.
- A valid palette no larger than 96 joints.
- An empty/static palette path where applicable.
- An over-limit palette or undersized palette safety case that skips safely and logs clearly, without crashing.

Do not expand the 96-joint Phase 5E limit. Larger skin palettes remain future work unless already implemented before this phase.

#### Resize/recreate

Exercise resize/recreate if feasible:

- Render at an initial hidden-window size such as 16x16 or 32x32.
- Resize the SDL window or invoke frame dimensions that trigger swapchain recreation.
- Render again after the resize trigger.
- If the platform’s hidden Vulkan window cannot reliably trigger swapchain recreation, add a clear test comment and preserve the manual validation checklist.

#### Resource lifetime after submitted frames

Exercise:

- Create mesh, texture, material.
- Draw and submit at least one frame.
- Destroy material, texture, and mesh.
- Render another frame or safely no-op with destroyed handles unavailable.
- Confirm no crash or validation-visible use-after-free in normal execution.

If Phase 5E.9 introduced deferred deletion queues, add a frame/drain exercise to confirm queued resources are eventually released safely.

### 4. Prefer Generated Fixtures

Use generated in-test fixtures whenever possible.

Avoid depending on external artist assets, local paths, or display-specific content. Reuse existing generated Phase 5D representative fixtures if present.

Suggested generated fixtures:

- Static textured quad or cube.
- Vertex-colored triangle.
- Double-sided alpha-mask quad.
- Alpha-blend quad.
- Tangent-bearing normal-map triangle/quad.
- Minimal skinned triangle with one or two joints.

Keep generated data small and deterministic.

### 5. Add A Manual Validation Checklist

Add or update a manual validation section in an appropriate plan or docs location. Prefer keeping it in the Phase 5E.10 draft plan and in `.kilo/plans/1777257747393-clever-tiger.md` completion notes rather than creating broad new documentation unless the repo already has a validation checklist file.

Manual validation should include:

```bash
./build/stellar-client --renderer vulkan
```

Where representative assets are available, validate:

- Static glTF scene.
- Textured glTF scene.
- Vertex color material.
- Alpha mask material.
- Alpha blend material.
- Double-sided material.
- Normal map material with tangents.
- Metallic/roughness, occlusion, and emissive factors/textures.
- Skinned glTF scene within the 96-joint cap.
- Window resize while rendering.
- Shutdown after rendering.
- OpenGL still renders the same representative scenes.

If exact asset paths exist in the repo, include them. If they do not, write the checklist with placeholders and note that asset-specific paths were not available.

### 6. Avoid Over-Engineering The Completion Gate

Do not add:

- Mandatory GPU readback infrastructure unless the repo already supports it cleanly.
- Mandatory validation-layer dependency.
- Mandatory shader compiler dependency for default builds.
- New asset pipeline requirements.
- New public graphics API solely for testing unless unavoidable.

Phase 5E.10 is allowed to add test-only helpers in `tests/graphics/` if they reduce duplication.

### 7. Build And Validation Commands

Run and record results for the default path:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Run and record results for the opt-in Vulkan path:

```bash
cmake -S . -B build-vulkan-tests -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON
cmake --build build-vulkan-tests --target stellar_vulkan_context_smoke_test -j$(nproc)
ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure
```

If the environment lacks a Vulkan-capable display/device, do not fake success. Record the failure or skip reason clearly in completion notes.

Optional manual command:

```bash
./build/stellar-client --renderer vulkan
```

If a glTF asset flag is supported by the current client, include the actual command used. Otherwise document the current invocation limitation.

## Files Likely To Change

Likely:

- `tests/graphics/VulkanContextSmoke.cpp`
- `tests/graphics/` fixture helper files, if needed
- `CMakeLists.txt`, only if adding new opt-in Vulkan test targets
- `.kilo/plans/1777257747393-clever-tiger.md`
- `Phase5E10.md` or the local draft plan for this task

Possibly:

- `src/graphics/vulkan/*`, only for bugs discovered by the test matrix
- `include/stellar/graphics/vulkan/VulkanGraphicsDevice.hpp`, only if Doxygen updates are needed for touched public APIs
- `docs/`, only if the repo already has a validation checklist location that should be updated

Avoid:

- `AGENTS.md`
- Backend-neutral API churn
- New mandatory dependencies

## Completion Notes Required

After implementation and validation, update both:

1. This draft plan file.
2. `.kilo/plans/1777257747393-clever-tiger.md`

In `.kilo/plans/1777257747393-clever-tiger.md`, append completion notes under:

```md
### Phase 5E.10: Vulkan Parity Test Matrix And Completion Gate
```

Use this format:

```md
Completion notes (YYYY-MM-DD):

- Audited Phase 5E Vulkan coverage and confirmed the default test suite remains display-free.
- Expanded opt-in Vulkan context coverage for initialization, clear/present, static draw, textured material routing, alpha mask/blend, double-sided pipeline selection, skinned draw, resize/recreate behavior where feasible, and resource destruction after submitted frames.
- Reused generated representative fixtures rather than external assets for automated Vulkan smoke coverage.
- Preserved the `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS` opt-in gate; no Vulkan display/device requirement was added to default CTest.
- Added or updated the manual validation checklist for static glTF, textured materials, alpha modes, double-sided materials, normal/metallic/roughness/occlusion/emissive paths, skinned glTF within the 96-joint cap, resize, shutdown, and OpenGL comparison.
- Validated default build/tests with:
  - `<commands>`
  - Result: `<pass/fail and important details>`
- Validated opt-in Vulkan build/tests with:
  - `<commands>`
  - Result: `<pass/fail/skip and important details>`
- Manual validation:
  - `<commands/assets/results, or not run with reason>`
- Phase 5E completion status:
  - `<complete / blocked>`
  - Remaining follow-up, if any: `<specific Phase 5F or bug items>`
```

Do not mark Phase 5E complete if:

- Default CTest requires a GPU/display.
- Optional Vulkan tests do not build when enabled.
- The Vulkan path cannot initialize on a Vulkan-capable environment due to a Phase 5E regression.
- Static/textured/alpha/skinned smoke coverage is missing without a documented technical reason.
- The manual validation checklist is absent.
- Completion notes are not added to both the draft plan and the main clever-tiger plan.

## Acceptance Criteria

Phase 5E.10 is complete only when:

- Default `cmake --build` and `ctest` pass without requiring a GPU/display.
- `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS` remains default `OFF`.
- Opt-in Vulkan test target builds when `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON`.
- Opt-in Vulkan smoke coverage exercises:
  - initialization,
  - clear/present,
  - static draw,
  - texture/material descriptors,
  - alpha mask,
  - alpha blend,
  - double-sided/culling variant,
  - skinned draw within 96-joint cap,
  - safe invalid/over-limit skin handling,
  - resize/recreate where feasible,
  - destroy-after-submit lifetime behavior.
- Any infeasible hidden-window or environment-specific coverage is documented honestly.
- Manual validation checklist exists and is updated with actual commands/results where run.
- OpenGL behavior is not regressed by any test scaffolding changes.
- `.kilo/plans/1777257747393-clever-tiger.md` and this draft plan include completion notes.
- Any remaining non-blocking Vulkan parity work is explicitly moved to Phase 5F or a follow-up bug note.
