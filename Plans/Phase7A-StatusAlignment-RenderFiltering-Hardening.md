# Phase 7A: Status Alignment, Collision Render Filtering, and Regression Hardening

## Objective

Make the `collision-movement` branch internally consistent after the Phase 6A/6B/6C/6D first passes, then remove the biggest remaining ambiguity: whether collision-only glTF nodes render as visible geometry.

This phase is intentionally small and should be completed before larger gameplay/physics work.

## Primary agent recommendation

Use an agent optimized for repo cleanup and deterministic tests.

Suggested primary: `@carmack` for collision/import correctness.

Consult `@miyamoto` only for render-scene behavior.

## Current problem

`docs/ImplementationStatus.md` still describes Phase 6A as the active next implementation slice even though the Phase 6 plan files contain completion notes for Phase 6A, Phase 6B, Phase 6C, and Phase 6D.

Phase 6A also deferred collision-only render-node filtering. That means a node named `COL_*` or `Collision_*` may be extracted as collision, but its visual mesh behavior may not match the expected “collision-only” convention.

## Required reading

- `docs/ImplementationStatus.md`
- `Plans/Phase6A-LevelCollisionExtraction.md`
- `Plans/Phase6B-CollisionQueriesAndMovement.md`
- `Plans/Phase6C-BillboardSpriteRendering.md`
- `Plans/Phase6D-WorldMetadataFromGltf.md`
- `include/stellar/assets/SceneAsset.hpp`
- `include/stellar/assets/CollisionAsset.hpp`
- `include/stellar/assets/WorldMetadataAsset.hpp`
- `include/stellar/graphics/RenderScene.hpp`
- `src/import/gltf/SceneImport.cpp`
- `src/import/gltf/CollisionImport.cpp`
- `src/import/gltf/WorldMetadataImport.cpp`
- `src/graphics/RenderScene.cpp`
- `tests/import/gltf/ImporterRegression.cpp`
- `tests/graphics/RenderSceneInspection.cpp`
- `CMakeLists.txt`

## Scope

Implement the following:

1. Decide and implement collision-only render filtering:
   - Nodes named `COL_*`.
   - Nodes named `Collision_*`.
   - Mesh descendants of an exact node named `Collision`.

2. Add regression tests that prove:
   - Collision-marked meshes are extracted into `SceneAsset::level_collision`.
   - Collision-marked meshes are not submitted as normal render meshes after filtering.
   - Ordinary render meshes still render.
   - A `Collision` parent affects descendants for collision extraction and render filtering.
   - `SPAWN_*`, `TRIGGER_*`, `SPRITE_*`, and ordinary render nodes do not accidentally become collision.
   - Collision nodes do not accidentally become world metadata markers.

3. Keep the filtering policy documented in `docs/ImplementationStatus.md`.

## Recommended design

Prefer doing render filtering in the import/scene-construction layer, not in graphics backends.

Best path:

1. Centralize collision-node naming logic.
   - If a helper already exists in the importer, reuse it.
   - If duplicated logic exists, consolidate it into a small internal function shared by collision extraction and scene/render import where practical.

2. Preserve collision geometry extraction.
   - Do not remove the mesh data before collision extraction has read it.
   - Filtering should affect render scene membership, not the raw source traversal needed by collision extraction.

3. Exclude collision-only nodes from normal render primitives.
   - If the imported scene model stores nodes and meshes separately, make sure renderable node instances skip collision-only nodes.
   - Do not delete shared mesh assets that ordinary render nodes also reference.

4. Keep metadata extraction independent.
   - Marker nodes should remain marker-only unless they also intentionally match another convention.
   - Collision markers should not become world metadata.

## Non-goals

Do not implement:

- Character controller improvements.
- New collision algorithms.
- Trigger runtime behavior.
- ECS/server spawning.
- Third-party physics.
- New glTF extensions.
- Renderer backend rewrites.

## Suggested tests

Add or extend tests in `tests/import/gltf/ImporterRegression.cpp`:

1. `collision_only_node_is_not_rendered`
2. `collision_parent_descendant_is_not_rendered`
3. `ordinary_render_node_still_renders`
4. `collision_node_does_not_create_world_metadata_marker`
5. `metadata_marker_does_not_create_collision_mesh`

Add or extend tests in `tests/graphics/RenderSceneInspection.cpp` only if importer-level tests cannot prove render submission behavior.

## Validation commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_import_gltf_regression stellar_render_scene_inspection_test -j$(nproc)
ctest --test-dir build -R '^(gltf_importer_regression|render_scene_inspection)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- Collision node naming policy is documented.
- Collision-only meshes are extracted as collision but do not appear in normal render submissions.
- Ordinary render meshes are unaffected.
- Metadata markers remain metadata-only.
- Tests are display-free.
- Full default CTest passes.

## Completion notes template

Append after implementation:

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 7A status alignment, collision render filtering, and regression hardening.
- Documentation updates: <summarize ImplementationStatus changes>.
- Render filtering behavior: <describe exact node rules and where filtering occurs>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Robust character movement remains Phase 7B.
  - Runtime world assembly remains Phase 7C.
  - Runtime triggers remain Phase 7D.
```
