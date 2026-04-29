# Phase 6D: World Metadata From glTF Node Conventions

## Objective

Extract simple gameplay/world metadata from glTF node names or extras so imported level files can define spawn points, triggers, portals, interaction points, and sprite/entity placements.

This phase connects authored glTF levels to the intended 3D world with 2D billboard entities without requiring a custom editor yet.

## Primary Agent

@carmack primary for gameplay/world metadata model.

Consult @miyamoto only if metadata extraction must interact with render-scene import or sprite rendering from Phase 6C.

Do not modify AGENTS.md.

## Prerequisites

Recommended prerequisites:

- Phase 6A complete if metadata should share imported level conventions.
- Phase 6C complete if sprite placements should immediately render.

This phase can still implement pure metadata extraction before Phase 6C if needed.

## Required Reading

Read before editing:

- `AGENTS.md`
- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `include/stellar/assets/SceneAsset.hpp`
- `include/stellar/scene/SceneGraph.hpp`
- `src/import/gltf/SceneImport.cpp`
- `src/import/gltf/NodeImport.cpp`
- `tests/import/gltf/ImporterRegression.cpp`
- Phase 6A completion notes if present
- Phase 6C completion notes if present
- `CMakeLists.txt`

## Scope

Implement metadata extraction only.

Required metadata types:

- Player spawn point.
- Generic entity spawn point.
- Trigger volume marker.
- Optional sprite placement marker if Phase 6C exists.

## Non-Goals

Do not implement:

- ECS runtime spawning unless the ECS/world layer already exists and integration is trivial.
- Trigger runtime behavior.
- Portal rendering or sector culling.
- Editor tooling.
- JSON schema validation beyond small importer tests.
- Physics/collision extraction. That belongs to Phase 6A.
- Billboard rendering. That belongs to Phase 6C.

## Metadata Conventions

Initial node-name conventions:

- `SPAWN_Player` = player spawn point.
- `SPAWN_<Archetype>` = generic spawn point with archetype string.
- `TRIGGER_<Name>` = trigger marker.
- `SPRITE_<Name>` = sprite placement marker.
- `PORTAL_<Name>` = optional parsed marker only if trivial; runtime behavior deferred.

Transform usage:

- Node world translation is metadata position.
- Node world rotation is metadata orientation if available.
- Node scale may be used for trigger volume extents if present.

Optional glTF `extras` support:

- If cgltf exposes node extras as raw JSON text and it is simple to preserve, store raw extras string in metadata.
- Do not add a JSON parser solely for this phase unless the project already has one.

## Data Model

Preferred new public header:

- `include/stellar/assets/WorldMetadataAsset.hpp`

Suggested structures:

```cpp
namespace stellar::assets {

enum class WorldMarkerType {
    kPlayerSpawn,
    kEntitySpawn,
    kTrigger,
    kSprite,
    kPortal,
};

struct WorldMarker {
    WorldMarkerType type = WorldMarkerType::kEntitySpawn;
    std::string name;
    std::string archetype;
    std::array<float, 3> position{};
    std::array<float, 4> rotation{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
    std::string extras_json;
};

struct WorldMetadataAsset {
    std::vector<WorldMarker> markers;
};

}
```

Add to `SceneAsset`:

```cpp
WorldMetadataAsset world_metadata;
```

Keep Doxygen `@brief` comments on public types.

## Extraction Rules

- Walk all imported glTF nodes.
- Match node names with the conventions above.
- Compute node world transform.
- Emit one marker per matching node.
- A marker node does not need a mesh.
- Marker extraction must not require graphics backends.
- Unknown node prefixes are ignored.
- Malformed known prefixes should fail only if ambiguous or unsafe. Prefer predictable degradation.

## Trigger Volume Rules

Initial trigger representation:

- Position from node world translation.
- Extents from absolute node scale.
- Shape defaults to box if a shape field is later added.

Do not implement runtime trigger overlap checks in this phase.

## Sprite Marker Rules

If Phase 6C exists:

- Store sprite marker metadata only.
- Do not automatically create render sprites unless a clean scene-renderer hook already exists.

If Phase 6C does not exist:

- Still store `SPRITE_*` markers for future use.

## Tests

Add display-free importer tests.

Required tests:

1. `SPAWN_Player` node creates player spawn marker.
2. `SPAWN_Imp` node creates entity spawn marker with archetype `Imp`.
3. `TRIGGER_DoorOpen` node creates trigger marker.
4. `SPRITE_Torch` node creates sprite marker.
5. Parent/child transforms are applied to marker position.
6. Ordinary render nodes create no markers.
7. Empty scene has empty metadata.

Suggested test location:

- `tests/import/gltf/ImporterRegression.cpp` if generated glTF helpers are reusable.
- Or `tests/import/gltf/WorldMetadata.cpp` with a new target if more maintainable.

## Build System

Update `CMakeLists.txt` if adding source files or a new test target.

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

- glTF node conventions produce backend-neutral world metadata.
- Player spawn, generic entity spawn, trigger, and sprite markers are covered by tests.
- Marker transforms are world-space and deterministic.
- Ordinary render/collision nodes do not create accidental metadata.
- Default tests remain display-free.
- Public APIs touched have Doxygen `@brief` comments.
- Completion notes are appended to this file.

## Completion Notes Template

Append after implementation:

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 6D world metadata extraction from glTF.
- Data model: <describe new headers/types and SceneAsset integration>.
- Node conventions: <describe implemented marker prefixes>.
- Transform behavior: <describe world transform handling>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Runtime spawning remains future work.
  - Trigger overlap/runtime behavior remains future work.
  - Sprite marker rendering remains future work unless Phase 6C integration was also added.
```
