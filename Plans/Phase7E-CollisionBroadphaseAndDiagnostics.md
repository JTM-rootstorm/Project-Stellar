# Phase 7E: Static Collision Broadphase and Diagnostics

## Objective

Improve collision query scalability for larger imported glTF levels by adding a simple static broadphase and diagnostics.

This should happen after correctness-focused movement work unless profiling shows immediate need earlier.

## Primary agent recommendation

Use an agent strong in performance-oriented data structures and regression testing.

Suggested primary: `@carmack`.

## Prerequisites

Recommended:

- Phase 7B complete enough to define real query patterns.
- Phase 7C complete enough to expose runtime diagnostics.

## Required reading

- `include/stellar/assets/CollisionAsset.hpp`
- `include/stellar/physics/CollisionWorld.hpp`
- `src/physics/CollisionWorld.cpp`
- `tests/physics/CollisionWorld.cpp`
- Phase 7B completion notes if present
- Phase 7C completion notes if present
- `CMakeLists.txt`

## Scope

Add an optional internal acceleration structure to `CollisionWorld` or a new `StaticCollisionBroadphase`.

Recommended broadphase options:

1. **Static BVH** over triangle AABBs.
   - Best general-purpose option.
   - Good for raycasts and sweeps.
   - Deterministic if build order is stable.

2. **Uniform grid/spatial hash**.
   - Good for game-like worlds if cell size is chosen well.
   - More tuning required.

Recommendation: implement a simple static BVH first.

## Recommended public API

Keep public API mostly unchanged.

Optionally add diagnostics:

```cpp
namespace stellar::physics {

struct CollisionWorldStats {
    std::size_t mesh_count = 0;
    std::size_t triangle_count = 0;
    std::size_t broadphase_node_count = 0;
    std::size_t last_query_triangle_tests = 0;
};

[[nodiscard]] CollisionWorldStats stats() const noexcept;

}
```

Avoid exposing the BVH data structure unless there is a clear use case.

## Recommended implementation steps

### Step 1: Add triangle AABB helpers

Implement internal helpers for:

- Triangle bounds.
- Segment-vs-AABB.
- Swept-sphere-vs-AABB or expanded AABB segment test.

Tests:

- Segment intersects AABB.
- Segment misses AABB.
- Expanded AABB catches radius.

### Step 2: Build a deterministic static BVH

Build once in `CollisionWorld` constructor.

Potential design:

```cpp
struct TriangleRef {
    std::size_t mesh_index;
    std::size_t triangle_index;
    Aabb bounds;
    Vec3 centroid;
};

struct BvhNode {
    Aabb bounds;
    std::uint32_t first;
    std::uint32_t count;
    std::uint32_t left;
    std::uint32_t right;
};
```

Keep leaf size simple, such as 4-8 triangles.

Use longest-axis split by centroid median. Use stable sort or deterministic tie-breaking.

### Step 3: Route raycast through BVH

Raycast should produce exactly the same hit result as brute force.

Tests:

- Existing raycast tests still pass.
- Brute-force comparison helper confirms nearest hit equivalence on a small synthetic scene.
- Tie behavior is deterministic.

### Step 4: Route movement/sweeps through BVH

For movement queries, broadphase against expanded bounds.

Tests:

- Existing movement tests still pass.
- Character controller tests still pass if Phase 7B exists.
- Candidate pruning does not miss edge/corner cases.

### Step 5: Add diagnostics

Expose useful stats:

- triangle count,
- mesh count,
- node count,
- optional query candidate count.

Do not make tests brittle by asserting exact internal node counts unless the BVH layout is intentionally stable.

## Non-goals

Do not implement:

- Dynamic broadphase updates.
- Moving rigid bodies.
- Runtime mesh mutation.
- Multithreaded BVH build.
- SIMD optimization.
- GPU collision.
- Navigation mesh.

## Suggested tests

Add or extend `tests/physics/CollisionWorld.cpp`.

Optional separate test file:

- `tests/physics/CollisionBroadphase.cpp`

Required tests:

1. BVH raycast matches brute-force nearest hit.
2. BVH raycast misses when brute force misses.
3. BVH handles empty collision asset.
4. BVH handles single triangle.
5. BVH handles many triangles deterministically.
6. BVH movement query still stops before wall.
7. BVH movement query still slides along wall.
8. Degenerate triangles do not crash build or query.

## Validation commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_collision_world_test -j$(nproc)
ctest --test-dir build -R '^collision_world$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

If a separate target is added:

```bash
cmake --build build --target stellar_collision_broadphase_test -j$(nproc)
ctest --test-dir build -R '^collision_broadphase$' --output-on-failure
```

## Acceptance criteria

- Collision queries return the same observable results as the previous brute-force implementation.
- Large synthetic scenes perform fewer triangle tests for representative queries.
- Empty/small scenes still work.
- Public API remains small.
- Diagnostics are available.
- Default tests remain display-free.

## Completion notes template

Append after implementation:

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 7E static collision broadphase and diagnostics.
- Broadphase design: <describe BVH/grid and build rules>.
- Public API: <describe stats/diagnostics if added>.
- Query behavior: <state equivalence with previous brute-force behavior>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Dynamic broadphase remains future work.
  - Profiling/benchmark targets remain future work unless added.
```
