# Phase 6A: Static Level Collision Extraction From glTF

## Objective

Add backend-neutral static level collision data extracted from imported glTF scene geometry.

This phase moves the project from "glTF renders" toward "glTF can define playable world geometry" for the intended 3D world with 2D billboard entities.

## Primary Agent

@carmack primary.

Consult @miyamoto only if importer asset wiring or render-scene assumptions become blockers.

Do not modify AGENTS.md.

## Current Assumptions

- glTF rendering support is already sufficient for static level visuals.
- The engine design wants 3D static environments with 2D billboard entities/objects.
- glTF should be usable as the level/world source format.
- glTF has no standard collision schema, so this phase uses simple node-name conventions.
- Collision data must be usable by game/server/world code without OpenGL or Vulkan.
- Do not integrate a third-party physics engine in this phase.
- Do not implement character movement in this phase except small tests/helpers if unavoidable.

## Required Reading

Read before editing:

- `AGENTS.md`
- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `include/stellar/assets/SceneAsset.hpp`
- `include/stellar/assets/MeshAsset.hpp`
- `include/stellar/scene/SceneGraph.hpp`
- `src/import/gltf/SceneImport.cpp`
- `src/import/gltf/NodeImport.cpp`
- `src/import/gltf/MeshImport.cpp`
- `tests/import/gltf/ImporterRegression.cpp`
- `tests/graphics/RenderSceneInspection.cpp`
- `CMakeLists.txt`

## Scope

Implement data extraction only:

- Add backend-neutral collision asset types.
- Extract collision triangles from selected glTF nodes/meshes.
- Transform collision triangles into scene/world space using node transforms.
- Attach extracted collision data to imported `SceneAsset` or another backend-neutral asset container.
- Add deterministic tests.

## Non-Goals

Do not implement:

- Player controller.
- Runtime physics simulation.
- Broadphase acceleration beyond simple bounds if not needed for tests.
- Dynamic collision objects.
- Triggers, spawn points, portals, or gameplay metadata. Those belong to Phase 6D.
- Navigation mesh/pathfinding.
- glTF extension support for collision.
- Renderer changes.

## Collision Node Convention

Use these conventions for initial implementation:

- Node names starting with `COL_` are collision-only.
- Node names starting with `Collision_` are collision-only.
- A node named exactly `Collision` marks its descendant mesh nodes as collision-only.
- Ordinary render nodes are not collision unless they match one of the conventions.

Collision-only means:

- Triangles are extracted into the collision asset.
- Rendering behavior should not change in this phase unless current render import already renders those nodes. If hiding collision-only render nodes requires broad renderer changes, document and defer the render-filtering step.

Optional if simple:

- Add a boolean helper such as `is_collision_node_name(std::string_view)` in importer or asset code.

## Data Model

Preferred new public header:

- `include/stellar/assets/CollisionAsset.hpp`

Suggested structures:

```cpp
namespace stellar::assets {

struct CollisionTriangle {
    std::array<float, 3> a{};
    std::array<float, 3> b{};
    std::array<float, 3> c{};
    std::array<float, 3> normal{};
};

struct CollisionMesh {
    std::string name;
    std::vector<CollisionTriangle> triangles;
    std::array<float, 3> bounds_min{};
    std::array<float, 3> bounds_max{};
};

struct LevelCollisionAsset {
    std::vector<CollisionMesh> meshes;
    std::array<float, 3> bounds_min{};
    std::array<float, 3> bounds_max{};
};

}
```

Add to `SceneAsset`:

```cpp
std::optional<LevelCollisionAsset> level_collision;
```

Alternative acceptable shape:

- `LevelCollisionAsset level_collision;` with empty vector meaning no collision.

Keep Doxygen `@brief` on public structs.

## Extraction Rules

- Only triangle primitives are supported.
- Use already-imported `MeshAsset` primitive vertex/index data when possible.
- Resolve node mesh instances through current scene/node data.
- Apply node world transform to vertex positions before writing collision triangles.
- Compute per-triangle normal from transformed vertices.
- Compute per-collision-mesh bounds and aggregate collision bounds.
- Skip non-mesh collision nodes with no error.
- Fail clearly if a collision-marked mesh primitive cannot produce triangle geometry.
- Do not duplicate renderer-specific handles or graphics data.

## Transform Rules

- Collision extraction must respect parent-child node transforms.
- If the existing scene graph stores local transforms only, add a small import/runtime helper to compute node world matrices for extraction.
- Use the same transform interpretation as render import/runtime code.
- Add tests for translated parent + translated child.

## Tests

Add display-free tests. Prefer importer regression tests or a new focused test target.

Required tests:

1. Empty scene imports with no collision asset or empty collision asset.
2. Node named `COL_floor` extracts one triangle.
3. Node named `Collision_wall` extracts one triangle.
4. Parent node named `Collision` causes descendant mesh extraction.
5. Ordinary render mesh is not extracted as collision.
6. Node transforms are applied to collision vertices.
7. Collision bounds are correct.
8. Collision triangle normal is finite and points according to winding.

Suggested test location:

- `tests/import/gltf/ImporterRegression.cpp` if generated glTF helpers already exist there.
- Or `tests/import/gltf/CollisionExtraction.cpp` with a new target if the regression file becomes too large.

## Build System

If a new test target is added, update `CMakeLists.txt`.

Default CTest must remain display-free.

No OpenGL/Vulkan context may be required.

## Validation Commands

Run at minimum:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_import_gltf_regression -j$(nproc)
ctest --test-dir build -R '^gltf_importer_regression$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

If a new test target is added, include it in validation and completion notes.

## Acceptance Criteria

- Static collision data is represented backend-neutrally.
- glTF collision nodes can be identified by documented name conventions.
- Collision triangles are extracted in world space.
- Bounds are computed.
- Tests cover naming, transforms, exclusion of ordinary render meshes, and bounds.
- No renderer behavior regresses.
- Default tests remain display-free.
- Public APIs touched have Doxygen `@brief` comments.
- Completion notes are appended to this file.

## Completion Notes Template

Append after implementation:

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 6A static level collision extraction from glTF.
- Data model: <describe new headers/types and SceneAsset integration>.
- Node conventions: <describe implemented collision node rules>.
- Transform behavior: <describe local/world transform handling>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Runtime collision queries remain Phase 6B.
  - Player movement/controller remains Phase 6B or later.
  - Gameplay metadata/spawns/triggers remain Phase 6D.
```
