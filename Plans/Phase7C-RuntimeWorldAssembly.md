# Phase 7C: Runtime World Assembly From Imported glTF Scenes

## Objective

Connect the imported asset data from Phase 6 into a coherent runtime world object that gameplay/client code can use.

After this phase, a loaded glTF scene should have a single runtime representation that exposes:

- Render scene data.
- Static collision world.
- Player spawn and entity spawn markers.
- Trigger metadata.
- Sprite markers or sprite render submissions.
- Optional validation diagnostics.

This phase is about integration, not new physics algorithms.

## Primary agent recommendation

Use an agent strong in architecture, data flow, and tests.

Suggested primary: `@miyamoto` if render scene wiring dominates.

Consult `@carmack` for collision/world semantics.

## Required reading

- `include/stellar/assets/SceneAsset.hpp`
- `include/stellar/assets/CollisionAsset.hpp`
- `include/stellar/assets/WorldMetadataAsset.hpp`
- `include/stellar/physics/CollisionWorld.hpp`
- `include/stellar/graphics/RenderScene.hpp`
- `include/stellar/graphics/BillboardSprite.hpp`
- `src/client/Application.cpp`
- `src/client/ApplicationConfig.cpp`
- `tests/client/AssetValidationSmoke.cpp`
- `tests/import/gltf/ImporterRegression.cpp`
- `tests/graphics/RenderSceneInspection.cpp`
- `CMakeLists.txt`
- Phase 7A and Phase 7B completion notes if present.

## Scope

Implement a backend-neutral runtime world assembly layer.

Preferred new files:

- `include/stellar/world/RuntimeWorld.hpp`
- `src/world/RuntimeWorld.cpp`
- `tests/world/RuntimeWorld.cpp`

Possible names:

- `GameWorld`
- `LoadedWorld`
- `RuntimeScene`

Pick one and keep it small.

## Recommended public API

```cpp
namespace stellar::world {

struct RuntimeWorldDiagnostics {
    bool has_collision = false;
    std::size_t collision_mesh_count = 0;
    std::size_t collision_triangle_count = 0;
    std::size_t marker_count = 0;
    std::size_t sprite_marker_count = 0;
    bool has_player_spawn = false;
    std::vector<std::string> warnings;
};

struct RuntimeWorld {
    const stellar::assets::SceneAsset* scene_asset = nullptr;
    std::optional<stellar::physics::CollisionWorld> collision_world;
    stellar::assets::WorldMetadataAsset world_metadata;
    RuntimeWorldDiagnostics diagnostics;
};

[[nodiscard]] RuntimeWorld build_runtime_world(const stellar::assets::SceneAsset& scene);

}
```

If `CollisionWorld` cannot be stored by value due to references, use a stable ownership pattern that keeps lifetime obvious.

## Recommended implementation steps

### Step 1: Add a `world` library

Add:

```cmake
add_library(stellar_world
    src/world/RuntimeWorld.cpp
)

target_include_directories(stellar_world PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_world PUBLIC
    stellar_assets
    stellar_physics
    stellar_graphics
)
```

If graphics dependency is unnecessary, do not add it. Prefer narrower dependencies.

### Step 2: Build from `SceneAsset`

`build_runtime_world(scene)` should:

- Keep a pointer/reference to the source `SceneAsset`.
- Create a `CollisionWorld` only when `scene.level_collision` exists and has meshes.
- Copy or reference `world_metadata` consistently.
- Produce diagnostics.

### Step 3: Add marker lookup helpers

Add small helpers:

- `find_player_spawn`
- `find_markers_by_type`
- `find_spawn_by_archetype`
- `find_sprite_markers`
- `find_trigger_markers`

Keep these pure and testable.

### Step 4: Create billboard sprites from sprite markers only if clean

Optional in this phase:

- Convert `SPRITE_*` metadata into `BillboardSprite` records.
- Only do this if texture selection is already available or can use a deterministic placeholder.

Better default:

- Expose sprite markers through the runtime world.
- Defer actual marker-to-texture mapping to a later content-binding phase.

### Step 5: Wire client validation

Update the client asset validation smoke path to optionally report:

- collision present/absent,
- marker count,
- player spawn present/absent,
- trigger count,
- sprite marker count.

Do not require a graphics context.

## Non-goals

Do not implement:

- ECS runtime spawning.
- Real gameplay behavior.
- Trigger overlap events.
- Character movement loop.
- Networking.
- Editor tooling.
- Sprite atlas/material binding.
- Third-party physics.

## Suggested tests

Create `tests/world/RuntimeWorld.cpp`.

Add target:

- `stellar_runtime_world_test`
- CTest name: `runtime_world`

Required tests:

1. Empty scene builds an empty runtime world.
2. Scene with collision creates a collision world.
3. Scene without collision leaves collision world empty.
4. Player spawn marker is discoverable.
5. Generic entity spawn marker is discoverable by archetype.
6. Trigger markers are discoverable.
7. Sprite markers are discoverable.
8. Diagnostics counts are correct.
9. Lifetime assumptions are documented and tested enough to avoid dangling references in obvious construction paths.

Also extend `tests/client/AssetValidationSmoke.cpp` if client validation output changes.

## Validation commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_runtime_world_test stellar_client_asset_validation_smoke -j$(nproc)
ctest --test-dir build -R '^(runtime_world|client_asset_validation_smoke|client_cli_asset_validation)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- A `SceneAsset` can be assembled into a small runtime world object.
- Static collision, metadata, and diagnostics are available from one place.
- No graphics context is required.
- Tests cover empty and populated scenes.
- Client asset validation remains display-free.
- Public APIs have Doxygen `@brief` comments.
- Documentation states runtime integration status accurately.

## Completion notes template

Append after implementation:

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 7C runtime world assembly.
- Public API: <describe RuntimeWorld/build helpers>.
- Collision integration: <describe CollisionWorld lifetime/creation>.
- Metadata integration: <describe marker lookup behavior>.
- Client validation: <describe any output changes>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Runtime trigger overlap remains Phase 7D.
  - Broadphase remains Phase 7E.
  - ECS/gameplay spawning remains future work.
```
