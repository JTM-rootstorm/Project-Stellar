# Project Stellar — Specular + Normal BSP Materials Master Plan

**Branch:** `spec-normal-textures`  
**Plan date:** 2026-05-03  
**Repository state observed:** `spec-normal-textures` currently exists and compares identical to `main` at commit `e999282ebd294b7ed2a6bc454aba2909aaf56559`.  
**Instruction:** Do **not** commit from this plan. Work locally on `spec-normal-textures` and leave changes for user review.

---

## 0. Purpose

Implement a lightweight BSP material sidecar system plus normal/specular presentation lighting for Stellar BSP maps.

This plan intentionally avoids "full PBR". The goal is:

- BSP texture names remain the canonical map-facing material keys.
- Sidecar material files enrich those texture names with optional texture maps and factors.
- Normal maps affect static BSP surface presentation when valid tangents are present.
- Specular maps/factors add lightweight highlights.
- Imported BSP lightmaps continue to work.
- OpenGL and Vulkan remain parity targets.
- Default validation remains display-free.

---

## 1. Branch Baseline Facts the Agent Must Respect

The current branch has already completed the client/server split and TrenchBroom BSP30 workflow. Do not restart those efforts.

Important current architecture facts:

- `stellar_import_bsp` owns BSP loading and currently builds `LevelAsset` geometry, materials, textures, lightmaps, collision, visibility, and metadata.
- `stellar_graphics` owns `RenderLevel`, `GraphicsDevice`, OpenGL, Vulkan, level material upload, and presentation-only draw behavior.
- `stellar_protocol`, `stellar_transport`, `stellar_authority`, `stellar_server_runtime`, `stellar_client_net`, and the client/server app modules have strict CMake boundary checks.
- Static world rendering is presentation behavior and must not become gameplay authority.
- `LevelAsset` already carries source-neutral geometry/material/texture/lightmap data.
- `MaterialAsset` and `MaterialUpload` already have base color, normal, metallic-roughness, occlusion, emissive, alpha, double-sided, and unlit surfaces. They do not yet have explicit lightweight specular map/factor fields.
- `StaticVertex` already has `normal`, `uv0`, `tangent`, `uv1`, and color data. BSP import currently fills position/normal/uv0/uv1 but does not mark tangents for BSP faces.
- `RenderLevel` currently creates simple BSP level materials from `LevelSurfaceMaterial`, defaulting BSP level surfaces to `unlit=true` and `double_sided=true`, then resolves base textures and lightmaps.

---

## 2. Non-Goals

Do not implement these in this branch:

- Full PBR, IBL, reflection probes, environment maps, tone mapping, HDR pipeline, clustered/forward+ lighting, or physically based BRDF validation.
- Gameplay/server-authoritative material effects.
- Network transfer/caching of maps or material assets.
- Client-side gameplay scripting.
- Source/VBSP material support.
- Dynamic lights, shadow maps, deferred rendering, or lightmap rebaking.
- Editor GUI automation.

---

## 3. Sidecar Format Decision

Use a deliberately small line-oriented sidecar format to avoid adding a JSON/TOML parser dependency.

Recommended extension: `.stellar_material`

Recommended path convention:

```text
<map-directory>/materials/<normalized-bsp-texture-name>.stellar_material
```

Example for BSP texture name `dev/wall_96`:

```text
materials/dev/wall_96.stellar_material
```

Initial format:

```ini
version = 1
name = "dev/wall_96"

# Optional. If omitted, keep the BSP/WAD/developer base texture.
base_color = "wall_96_albedo.png"

normal = "wall_96_normal.png"
normal_scale = 1.0
normal_light_strength = 0.25

specular = "wall_96_spec.png"
specular_factor = 0.35
specular_power = 48.0

roughness_factor = 0.75
metallic_factor = 0.0

emissive = ""
emissive_factor = 0.0 0.0 0.0

alpha_mode = opaque
alpha_cutoff = 0.5
double_sided = true
unlit = false
```

Rules:

- `version` is required and must be `1`.
- Texture paths are relative to the sidecar file directory.
- Absolute paths and `..` components are invalid.
- Unknown keys are warnings by default, errors only when strict mode is enabled.
- `base_color` is optional; if absent, retain the texture already imported from BSP/WAD/developer material fallback.
- Color-space defaults:
  - base color/emissive: sRGB
  - normal/specular/roughness/metallic/occlusion: linear
  - lightmaps: linear/clamp as currently implemented

---

## 4. Phase Index

| Phase | File | Goal | Depends On | Parallelization |
|---|---|---|---|---|
| SNT-0 | `01-SNT-0-Baseline-And-Guardrails.md` | Establish clean branch baseline, audits, and build/test constraints. | none | Must run first. |
| SNT-1 | `02-SNT-1-Material-Contracts.md` | Add source-neutral material contract fields for sidecar-resolved normal/specular data. | SNT-0 | Can be designed in parallel with SNT-2, but land before SNT-3+. |
| SNT-2 | `03-SNT-2-Sidecar-Parser-And-Discovery.md` | Add parser, discovery rules, diagnostics, and safe path handling. | SNT-1 interface decisions | Can run in parallel with SNT-4 tangent design. |
| SNT-3 | `04-SNT-3-BSP-Material-Resolution-And-Image-Loading.md` | Resolve sidecars into BSP `LevelAsset` materials/textures/images. | SNT-1, SNT-2 | Serial with SNT-2; parallel test authoring is safe. |
| SNT-4 | `05-SNT-4-BSP-Tangent-Generation.md` | Generate valid BSP tangents/handedness from texinfo/UV basis. | SNT-0 | Can run in parallel with SNT-2/SNT-3 if contract is stable. |
| SNT-5 | `06-SNT-5-RenderLevel-Material-Upload.md` | Upload sidecar-resolved materials through `RenderLevel` and fake-device tests. | SNT-1, SNT-3, SNT-4 | Serial after material resolution and tangents. |
| SNT-6 | `07-SNT-6-OpenGL-Normal-Specular-Lighting.md` | Implement OpenGL shader/material path for normal/specular lighting. | SNT-5 | Can begin after SNT-5 capture tests pass. |
| SNT-7 | `08-SNT-7-Vulkan-Parity.md` | Bring Vulkan descriptor/uniform/shader path to parity. | SNT-5, SNT-6 contract | Can run after OpenGL contract is frozen; shader work can be parallel. |
| SNT-8 | `09-SNT-8-Fixtures-Docs-Final-Validation.md` | Add fixtures, docs, final validation, status handoff. | SNT-0 through SNT-7 | Must run last. |

---

## 5. Target File/Module Map

Likely production files to add or modify:

```text
include/stellar/assets/MaterialAsset.hpp
include/stellar/assets/LevelAsset.hpp
include/stellar/assets/TextureAsset.hpp
include/stellar/graphics/MaterialUpload.hpp
include/stellar/graphics/GraphicsDevice.hpp
include/stellar/import/bsp/Loader.hpp

src/import/bsp/BspTextureResolver.hpp
src/import/bsp/BspTextureResolver.cpp
src/import/bsp/BspLevelBuilder.cpp
src/import/bsp/MaterialSidecar.hpp              # new, if internal
src/import/bsp/MaterialSidecar.cpp              # new
src/import/bsp/MaterialSidecarResolver.hpp      # new
src/import/bsp/MaterialSidecarResolver.cpp      # new
src/import/bsp/ImageFileLoader.hpp              # new, or use a more general assets/image_io name
src/import/bsp/ImageFileLoader.cpp              # new

src/graphics/RenderLevel.cpp
src/graphics/opengl/OpenGLGraphicsDevice.cpp
src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp
src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp
src/graphics/vulkan/VulkanGraphicsDevicePrivate.hpp
CMakeLists.txt
```

Likely tests to add or modify:

```text
tests/import/bsp/BspMaterials.cpp
tests/import/bsp/BspLightmaps.cpp
tests/graphics/RenderSceneUpload.cpp
tests/graphics/RenderSceneInspection.cpp
tests/cmake/BspTests.cmake
tests/cmake/GraphicsTests.cmake
tests/fixtures/materials/**                     # new sidecar/image fixtures
tests/fixtures/trenchbroom/src/**               # optional source fixture references
```

Likely docs to update in final phase:

```text
docs/BspAuthoring.md
docs/Design.md
docs/ImplementationStatus.md
tools/trenchbroom/Stellar/README.md            # if present/appropriate
Plans/Archived/spec_normal_textures/**          # only if user wants archival later; not required by this request
```

---

## 6. Required Validation Cadence

Every implementation phase should preserve:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh
```

Use focused tests inside each phase, but phase-close must not skip target-boundary validation.

Optional GPU/display validation stays opt-in:

```bash
cmake -S . -B build-opengl -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_OPENGL_CONTEXT_TESTS=ON
cmake --build build-opengl -j$(nproc)
ctest --test-dir build-opengl -R 'opengl_context_smoke|render_level' --output-on-failure

cmake -S . -B build-vulkan -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON
cmake --build build-vulkan -j$(nproc)
ctest --test-dir build-vulkan -R 'vulkan_context_smoke|render_level' --output-on-failure
```

---

## 7. Done Definition for the Whole Branch

The branch is complete when:

- A BSP material named `dev/wall_96` can resolve `materials/dev/wall_96.stellar_material`.
- A sidecar can bind normal/specular maps without changing the BSP binary format.
- Missing sidecars silently preserve current BSP texture/lightmap behavior.
- Invalid sidecars produce deterministic diagnostics.
- BSP import generates valid tangents for faces with valid texinfo basis.
- `RenderLevel` uploads base color + lightmap + normal + specular material bindings.
- OpenGL renders a normal/specular-influenced lightweight material path.
- Vulkan has descriptor/uniform/shader parity with OpenGL.
- Default CTest remains display-free and passes.
- Docs clearly state this is lightweight BSP normal/specular material support, not full PBR.
- No commits are made by the agent unless the user later explicitly asks for one.
