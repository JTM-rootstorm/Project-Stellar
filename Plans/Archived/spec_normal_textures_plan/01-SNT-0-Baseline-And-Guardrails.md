# SNT-0 — Baseline, Branch Guardrails, and Audit

**Branch:** `spec-normal-textures`  
**Must not commit:** yes  
**Purpose:** Establish a clean, reproducible baseline before material sidecar work starts.

---

## 1. Required Starting State

1. Checkout the target branch:

```bash
git checkout spec-normal-textures
git status --short
```

2. Confirm the branch exists and compare it to main:

```bash
git fetch origin
git rev-parse --abbrev-ref HEAD
git rev-parse HEAD
git diff --stat main...HEAD
```

Observed during plan creation: `spec-normal-textures` exists and was identical to `main` at commit `e999282ebd294b7ed2a6bc454aba2909aaf56559`.

3. Do not commit. The user explicitly requested downloadable plans and no branch commits.

---

## 2. Baseline Build and Test

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh
```

Record the results in implementation notes before editing source. If baseline fails, stop implementation work and document failures first.

---

## 3. Architecture Guardrails

The current branch has strict module boundaries. Material sidecars and normal/specular lighting must follow these constraints:

- `stellar_import_bsp` may parse material sidecars and load texture image data into source-neutral `LevelAsset`.
- `stellar_graphics` may upload and render material data.
- `stellar_protocol`, `stellar_transport`, `stellar_client_net`, and network/session DTOs must not learn about sidecar materials.
- `stellar_authority`, `stellar_server_runtime`, and gameplay systems must not make gameplay decisions from material fields.
- `stellar_client_presentation` may remain presentation-only and should not own material parsing.
- Default tests must remain display-free. GPU context tests stay opt-in.

---

## 4. Audit Current Material/Lighting Entry Points

Before coding, inspect these files and record observations:

```bash
sed -n '1,240p' include/stellar/assets/MaterialAsset.hpp
sed -n '1,260p' include/stellar/assets/LevelAsset.hpp
sed -n '1,220p' include/stellar/assets/MeshAsset.hpp
sed -n '1,200p' include/stellar/graphics/MaterialUpload.hpp
sed -n '1,220p' include/stellar/graphics/GraphicsDevice.hpp
sed -n '1,260p' include/stellar/import/bsp/Loader.hpp
sed -n '1,260p' src/import/bsp/BspTextureResolver.cpp
sed -n '1,360p' src/import/bsp/BspLevelBuilder.cpp
sed -n '1,360p' src/graphics/RenderLevel.cpp
sed -n '1,360p' src/graphics/opengl/OpenGLGraphicsDevice.cpp
sed -n '1,360p' src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp
```

Expected current findings:

- `LevelSurfaceMaterial` has name, base texture index, lightmap index, and source name.
- `MaterialAsset` has normal/metallic-roughness/occlusion/emissive slots but lacks explicit lightweight specular fields.
- `MaterialUpload` already has normal/metallic-roughness/occlusion/emissive upload slots.
- `StaticVertex` has tangent storage, but BSP import does not populate tangents or mark `has_tangents`.
- `BspTextureResolver` keys material records by BSP texture name + texture index + lightmap index.
- `RenderLevel` currently builds simple BSP material assets from level surface records and sets BSP surfaces `unlit=true`, `double_sided=true`.

---

## 5. Naming and Scope Lock

Use these phase identifiers in commit messages or local notes if the user later asks for commits:

```text
SNT-0 baseline
SNT-1 material contracts
SNT-2 sidecar parser/discovery
SNT-3 material resolution/image loading
SNT-4 BSP tangent generation
SNT-5 RenderLevel upload
SNT-6 OpenGL normal/specular lighting
SNT-7 Vulkan parity
SNT-8 docs/fixtures/final validation
```

This branch implements "lightweight BSP normal/specular materials." Do not call it full PBR.

---

## 6. SNT-0 Acceptance Criteria

- Branch checked out locally.
- Baseline build/test/target-boundary results recorded.
- No source files modified.
- Any pre-existing failure is documented before work begins.
- Agent understands no-commit instruction.
