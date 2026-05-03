# Phase 0.5 — Remove Prototype Cube Renderer and Debug Cube Fallback

Branch target: `trenchbroom-compat`

## Goal

Fully remove the prototype spinning/debug cube code path so the branch has a clean BSP30/TrenchBroom-first runtime. After this phase, no-map and remote-without-presentation-map modes must initialize a static-less/blank presentation frame instead of creating or rendering a synthetic cube level.

This phase is intentionally early because the Z-up migration should not spend effort converting legacy cube-only fallback geometry that will be deleted.

## Non-negotiable invariants

- Do not replace the cube with another hardcoded prototype mesh.
- Do not remove `RenderLevel`, `LevelRenderer`, `GraphicsDevice`, OpenGL, Vulkan, billboard, BSP level, or material upload infrastructure.
- Do not remove no-map client startup capability; it should remain valid but static-less.
- Do not remove remote client startup without a local presentation map; it should remain valid and render dynamic snapshot billboards when available.
- Keep default validation display-free.
- Preserve server authority and gameplay/runtime behavior.

## Current-main evidence to verify before editing

Before making changes, confirm these active references still exist and adjust the task if the code has already changed:

```bash
git grep -n 'CubeRenderer\|DebugCubeMesh\|create_debug_cube_mesh\|debug:cube\|debug_cube\|debug_cube_surface\|debug_red\|debug_green\|debug_blue' -- include src tests CMakeLists.txt ':!Plans/Archived/**' ':!build*/**'
```

Expected current-main hit categories:

- `include/stellar/graphics/opengl/CubeRenderer.hpp`
- `src/graphics/opengl/CubeRenderer.cpp`
- `include/stellar/graphics/DebugCubeMesh.hpp`
- `src/graphics/DebugCubeMesh.cpp`
- `src/graphics/LevelRenderer.cpp`
- `include/stellar/graphics/LevelRenderer.hpp`
- `tests/graphics/RenderSceneInspection.cpp`
- `CMakeLists.txt`

## Tasks

### 0.5.1 Delete dedicated cube renderer files

Delete these files:

```text
include/stellar/graphics/opengl/CubeRenderer.hpp
src/graphics/opengl/CubeRenderer.cpp
```

Rationale: `RendererFactory` should create `LevelRenderer`; the dedicated OpenGL cube renderer is prototype-era code and should not remain as a public graphics path.

### 0.5.2 Delete debug cube mesh files

Delete these files:

```text
include/stellar/graphics/DebugCubeMesh.hpp
src/graphics/DebugCubeMesh.cpp
```

Rationale: `DebugCubeMesh` only supports the synthetic cube fallback and the cube-specific winding test. TrenchBroom/BSP30 should be the authored geometry path.

### 0.5.3 Update `CMakeLists.txt`

Remove these source files from `stellar_graphics`:

```cmake
src/graphics/DebugCubeMesh.cpp
src/graphics/opengl/CubeRenderer.cpp
```

Do not remove OpenGL/Vulkan graphics device sources, `RenderLevel.cpp`, or `LevelRenderer.cpp`.

### 0.5.4 Update `include/stellar/graphics/LevelRenderer.hpp`

Remove private declarations:

```cpp
static std::expected<stellar::assets::MeshAsset, stellar::platform::Error> create_cube_mesh();
static stellar::assets::LevelAsset create_cube_level();
```

Update comments:

- Replace “debug cube fallback” with “static-less fallback” or “optional level asset”.
- `initialize()` should say it initializes the provided level or an empty/static-less level when no level is provided.
- Do not mention cube, debug cube, or rotating cube in active comments.

### 0.5.5 Update `src/graphics/LevelRenderer.cpp`

Remove:

```cpp
#include "stellar/graphics/DebugCubeMesh.hpp"
```

Remove these functions entirely:

```cpp
LevelRenderer::create_cube_mesh()
LevelRenderer::create_cube_level()
```

Replace the initialization behavior:

```cpp
auto level = source_level_.has_value() ? std::move(*source_level_)
                                       : create_cube_level();
```

with a static-less level fallback:

```cpp
stellar::assets::LevelAsset level = source_level_.has_value()
    ? std::move(*source_level_)
    : stellar::assets::LevelAsset{};
```

Then compute bounds from that level as before:

```cpp
level_bounds_ = compute_level_bounds(level);
```

Expected behavior: empty geometry uploads no meshes and draws no static surfaces. `RenderLevel` should still initialize the selected graphics device, upload the default material, begin/end frames, and allow billboard presentation if provided later.

### 0.5.6 Update graphics tests

Modify `tests/graphics/RenderSceneInspection.cpp`:

- Remove `#include "stellar/graphics/DebugCubeMesh.hpp"`.
- Remove `verify_debug_cube_winding_matches_normals()`.
- Remove its call from `main()`.
- Add or strengthen tests that verify the desired replacement behavior:
  - empty `LevelAsset` bounds are finite and stable;
  - `RenderLevel` initialized with empty geometry renders zero static mesh draw calls;
  - `LevelRenderer` can retain/clear presentation state without relying on a fallback cube;
  - billboards can still submit after static-less rendering if a presentation state exists.

If there is already an empty-level behavior test, update its assertions to explicitly document that this is the no-map fallback policy.

### 0.5.7 Update client/runtime expectations if needed

Inspect `src/client/Application.cpp` and related tests. The current client can construct `LevelRenderer` without a map by passing `std::nullopt` or an empty optional `LevelAsset`. Keep that behavior.

Expected behavior after this phase:

- `stellar-client` with no `--map` initializes and renders a blank/static-less frame.
- remote mode without a local presentation map initializes and renders blank/static-less frames until authoritative dynamic snapshots arrive.
- if a remote snapshot includes billboards, they can still present without a static level.

Do not add map transfer, local presentation-map loading for remote mode, or new networking behavior in this phase.

### 0.5.8 Update active docs and plan language

Update active docs/plans where needed:

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `Plans/NEXT.md`
- this plan package if it has already been copied into the repo

Replace phrases like:

- “debug cube fallback”
- “fallback cube”
- “rotating cube”
- “no-map debug fallback renders cube”

with:

- “static-less fallback”
- “blank/no-static-level fallback”
- “no-map mode initializes without authored static geometry”

Recommended wording:

```markdown
No-map and remote-without-presentation-map modes render a blank/static-less presentation frame until a BSP30 presentation map or authoritative dynamic snapshot data is available. The old prototype cube renderer and debug cube mesh have been removed.
```

## Expected files changed

Deleted:

- `include/stellar/graphics/opengl/CubeRenderer.hpp`
- `src/graphics/opengl/CubeRenderer.cpp`
- `include/stellar/graphics/DebugCubeMesh.hpp`
- `src/graphics/DebugCubeMesh.cpp`

Modified:

- `CMakeLists.txt`
- `include/stellar/graphics/LevelRenderer.hpp`
- `src/graphics/LevelRenderer.cpp`
- `tests/graphics/RenderSceneInspection.cpp`
- `docs/ImplementationStatus.md`
- `docs/Design.md`
- `Plans/NEXT.md`
- possibly `Plans/trenchbroom_compat/*.md` if plans are already checked in

## Validation

Run focused build/tests:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_graphics stellar-client stellar_render_level_inspection_test stellar_graphics_backend_selection_test -j$(nproc)
ctest --test-dir build -R '^(render_level_inspection|graphics_backend_selection|gameplay_presentation|player_presentation|client_connect)$' --output-on-failure
```

Run full validation before moving to Phase 1:

```bash
ctest --test-dir build --output-on-failure
```

Run removal audit:

```bash
git grep -n 'CubeRenderer\|DebugCubeMesh\|create_debug_cube_mesh\|debug:cube\|debug_cube\|debug_cube_surface\|debug_red\|debug_green\|debug_blue' -- include src tests CMakeLists.txt docs Plans ':!Plans/Archived/**' ':!build*/**'
```

Expected audit result: no active hits, except possibly this archived/completed phase note after archival or explicitly historical documentation.

## Acceptance criteria

- [ ] Dedicated cube renderer files are deleted.
- [ ] Debug cube mesh files are deleted.
- [ ] `stellar_graphics` no longer compiles cube-specific source files.
- [ ] `LevelRenderer` initializes an empty/static-less `LevelAsset` when no source level is provided.
- [ ] No active source depends on `CubeRenderer`, `DebugCubeMesh`, `create_debug_cube_mesh`, or `debug:cube`.
- [ ] No-map client startup remains valid.
- [ ] Remote client startup without local presentation map remains valid.
- [ ] Empty/static-less rendering behavior is covered by display-free tests.
- [ ] Focused graphics/client tests pass.
- [ ] Full default CTest passes.

## Parallelization

Safe workstreams:

- Agent A: delete files and update CMake.
- Agent B: update `LevelRenderer` implementation/header.
- Agent C: update tests.
- Agent D: update docs and run grep audits.

Do not run multiple writers on `CMakeLists.txt`, `LevelRenderer.*`, or `RenderSceneInspection.cpp` concurrently.
