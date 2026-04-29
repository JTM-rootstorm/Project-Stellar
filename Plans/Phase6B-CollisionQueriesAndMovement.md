# Phase 6B: Collision Queries And Minimal Movement Resolution

## Objective

Add simple runtime collision queries against Phase 6A static level collision data and prove that game-world movement can resolve against imported glTF level geometry.

This phase should make collision usable by gameplay/server code without adding a full physics engine.

## Primary Agent

@carmack primary.

Consult @miyamoto only if render-scene or graphics asset integration unexpectedly blocks tests.

Do not modify AGENTS.md.

## Prerequisite

Phase 6A must be complete:

- Backend-neutral static level collision data exists.
- Collision triangles are extracted from glTF nodes.
- Collision data is available without OpenGL/Vulkan.

If Phase 6A is not complete, stop and implement Phase 6A first.

## Required Reading

Read before editing:

- `AGENTS.md`
- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `include/stellar/assets/CollisionAsset.hpp`
- `include/stellar/assets/SceneAsset.hpp`
- Any Phase 6A completion notes
- Existing math/scene/runtime utility headers
- `CMakeLists.txt`

## Scope

Implement a small collision query module for static level geometry.

Required capabilities:

- Raycast or segment cast against collision triangles.
- Ground probe or downward ray query.
- Simple swept sphere or swept point query for horizontal movement.
- Simple slide response against walls.
- Display-free tests.

## Non-Goals

Do not implement:

- Full rigid body physics.
- Dynamic body simulation.
- Networking/state replication.
- ECS integration unless the repo already has an ECS surface ready.
- Stair stepping unless trivial after basic slide works.
- Navigation/pathfinding.
- Broadphase acceleration beyond simple AABB pruning.
- Continuous capsule collision if it becomes too large. Use swept sphere or point first.

## Suggested Files

Preferred public headers:

- `include/stellar/physics/CollisionWorld.hpp`
- `include/stellar/physics/CollisionQuery.hpp` or similar

Preferred source files:

- `src/physics/CollisionWorld.cpp`
- `src/physics/CollisionQuery.cpp`

If the repo has no `physics` directory, create one.

## Data Model

Suggested query result:

```cpp
struct RaycastHit {
    bool hit = false;
    float t = 0.0f;
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
};
```

Suggested movement result:

```cpp
struct MoveResult {
    std::array<float, 3> position{};
    std::array<float, 3> velocity{};
    bool hit = false;
    bool grounded = false;
};
```

Suggested collision world:

```cpp
class CollisionWorld {
public:
    explicit CollisionWorld(const stellar::assets::LevelCollisionAsset& asset);
    RaycastHit raycast(std::array<float, 3> origin, std::array<float, 3> delta) const noexcept;
    MoveResult move_sphere(std::array<float, 3> position,
                           std::array<float, 3> velocity,
                           float radius) const noexcept;
};
```

Exact API may differ. Keep it small and testable.

## Algorithm Guidance

Use simple, deterministic algorithms:

- Ray/segment triangle intersection using Moller-Trumbore or equivalent.
- Treat `delta` as finite segment length, not infinite ray, unless API says otherwise.
- For movement, start with iterative sweep-and-slide:
  - attempt move along velocity
  - find earliest hit
  - move to safe point before hit
  - project remaining velocity along hit plane
  - repeat up to a small fixed count, such as 3
- If true swept sphere vs triangle is too large, implement point sweep plus radius-expanded plane/AABB approximation and document limitations.

Prefer correctness and clear tests over broad feature coverage.

## Coordinate System

Use the same coordinate system as imported glTF world data.

Do not silently swap axes.

If coordinate assumptions are unclear, write tests using simple axis-aligned floor/wall triangles and document the convention.

## Tests

Add display-free tests.

Required tests:

1. Ray misses empty collision world.
2. Ray hits a floor triangle.
3. Ray returns nearest hit when two triangles are along the segment.
4. Ray misses outside triangle bounds.
5. Horizontal movement stops before a wall.
6. Horizontal movement slides along an angled wall or axis-aligned wall.
7. Downward ground probe detects a floor.
8. Empty collision world movement passes through unchanged.

Suggested test file:

- `tests/physics/CollisionWorld.cpp`

Suggested target:

- `stellar_collision_world_test`

## Build System

Update `CMakeLists.txt` if adding physics library files or a new test target.

Default CTest must remain display-free.

No OpenGL/Vulkan context may be required.

## Validation Commands

Run at minimum:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_collision_world_test -j$(nproc)
ctest --test-dir build -R '^collision_world$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Adjust target/test names if implementation chooses different names. Record actual commands in completion notes.

## Acceptance Criteria

- Static glTF-derived collision data can be queried without graphics backends.
- Segment/raycast returns deterministic nearest hits.
- Minimal movement collision can stop/slide against static walls.
- Ground probe works for simple floor geometry.
- Tests are display-free.
- No third-party physics engine is added.
- Public APIs touched have Doxygen `@brief` comments.
- Completion notes are appended to this file.

## Completion Notes Template

Append after implementation:

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 6B collision queries and minimal movement resolution.
- Query API: <describe public API>.
- Movement behavior: <describe sweep/slide approach and limitations>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - ECS/server integration remains future work.
  - More robust capsule/stair/step handling remains future work.
  - Broadphase acceleration remains future work unless already added.
```
