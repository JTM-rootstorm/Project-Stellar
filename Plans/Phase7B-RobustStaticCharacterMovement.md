# Phase 7B: Robust Static Character Movement Over Imported Collision

## Objective

Upgrade the current minimal static collision movement into a small, reliable character-movement layer suitable for a 3D world with 2D billboard entities.

This phase should still avoid a full physics engine. The goal is a deterministic, testable character controller over static glTF-derived collision.

## Primary agent recommendation

Use an agent strong in geometry/math and defensive C++.

Suggested primary: `@carmack`.

Consult `@miyamoto` only if render/debug visualization hooks become blockers. Do not add renderer work in this phase unless it is optional diagnostics.

## Current baseline

The branch has:

- `include/stellar/physics/CollisionWorld.hpp`
- `src/physics/CollisionWorld.cpp`
- `tests/physics/CollisionWorld.cpp`

Current implemented capabilities include:

- Finite segment `raycast`.
- Downward `probe_ground`.
- Minimal `move_sphere` sweep/slide.
- Tests for empty worlds, ray hits/misses, nearest hit, wall stop, wall slide, and ground probe.

Current known limitation:

- Sphere movement handles radius by offsetting triangle planes and only accepts contacts whose projected point lies inside the source triangle.
- Triangle edges and vertices are not expanded into capsules.
- This is not a complete capsule controller.
- No stable start-overlap recovery, stair stepping, slope limits, or robust ground snap exists yet.

## Required reading

- `include/stellar/physics/CollisionWorld.hpp`
- `src/physics/CollisionWorld.cpp`
- `tests/physics/CollisionWorld.cpp`
- `include/stellar/assets/CollisionAsset.hpp`
- `Plans/Phase6B-CollisionQueriesAndMovement.md`
- `docs/ImplementationStatus.md`
- `CMakeLists.txt`

## Scope

Implement a new character-movement API while preserving the existing low-level query API.

Preferred new files:

- `include/stellar/physics/CharacterController.hpp`
- `src/physics/CharacterController.cpp`
- `tests/physics/CharacterController.cpp`

Alternative acceptable:

- Extend `CollisionWorld` only if the API remains clean and tests remain readable.

## Recommended public API

Use clear, small structs with Doxygen `@brief` comments.

Suggested API:

```cpp
namespace stellar::physics {

struct CharacterControllerConfig {
    float radius = 0.35f;
    float height = 1.8f;
    float skin_width = 0.03f;
    float max_slope_degrees = 50.0f;
    float step_height = 0.35f;
    float ground_snap_distance = 0.12f;
    int max_slide_iterations = 4;
};

struct CharacterMoveInput {
    std::array<float, 3> position{};
    std::array<float, 3> displacement{};
    std::array<float, 3> up{0.0f, 1.0f, 0.0f};
};

struct CharacterMoveResult {
    std::array<float, 3> position{};
    std::array<float, 3> remaining_displacement{};
    std::array<float, 3> ground_normal{0.0f, 1.0f, 0.0f};
    bool hit = false;
    bool grounded = false;
    bool stepped = false;
    bool started_overlapping = false;
    int iterations = 0;
};

class CharacterController {
public:
    explicit CharacterController(const CollisionWorld& world) noexcept;

    [[nodiscard]] CharacterMoveResult move(const CharacterMoveInput& input,
                                           const CharacterControllerConfig& config) const noexcept;
};

}
```

If capsule height support is too large, implement a vertical capsule approximation in tests and document limits. Do not pretend it is full rigid-body simulation.

## Recommended implementation path

### Step 1: Keep `CollisionWorld` stable

Do not break current `CollisionWorld` tests.

Add helper queries only if needed:

- `sphere_cast`
- `capsule_cast`
- `overlap_sphere`
- `recover_sphere_overlap`
- `classify_surface`

Keep helpers deterministic and display-free.

### Step 2: Add start-overlap recovery

Movement should handle the character starting slightly inside static geometry.

Recommended behavior:

1. Detect penetration against nearby triangles.
2. Push out along the most stable separating direction.
3. Limit recovery iterations.
4. Record `started_overlapping = true`.

Tests:

- Starts just inside wall and recovers outside.
- Starts slightly below floor and recovers above.
- Deep penetration does not create NaN or runaway movement.

### Step 3: Improve sphere/capsule sweep contacts

Implement contact handling for:

- Triangle face.
- Triangle edges.
- Triangle vertices.

Best practical path:

1. Keep face-plane sweep.
2. Add closest-point-on-triangle support.
3. For sphere movement, treat edges/vertices through closest-point tests at candidate times or conservative sub-steps.
4. Prefer a robust conservative method over a clever fragile exact method.

Tests:

- Sphere hits near triangle edge.
- Sphere hits triangle vertex/corner.
- Sphere slides around an outside corner without tunneling through.

### Step 4: Add slope limits

Surfaces with normal dot up below the configured threshold should be walls, not ground.

Tests:

- Walkable slope grounds the character.
- Too-steep slope blocks or slides character.
- Ground probe respects slope limit.

### Step 5: Add ground snap

After horizontal movement, snap down a small distance when a floor is within `ground_snap_distance`.

Tests:

- Moving across slightly uneven floor stays grounded.
- Snap does not pull character through thin floors.
- Snap does not stick to steep walls.

### Step 6: Add simple stair/step handling

Implement conservative step-up behavior:

1. Attempt normal horizontal move.
2. If blocked by a low obstacle, try raising by `step_height`.
3. Move horizontally.
4. Probe/snap down onto the step.
5. Mark `stepped = true`.

Tests:

- Character steps onto low block.
- Character does not step onto obstacle taller than `step_height`.
- Character does not step through wall with no landing.

### Step 7: Preserve deterministic slide behavior

Slide should remain fixed-iteration and deterministic.

Tests:

- Slide along axis-aligned wall.
- Slide along angled wall.
- Slide into two-wall corner stops cleanly.
- Remaining displacement is stable and finite.

## Non-goals

Do not implement:

- Dynamic rigid bodies.
- Forces, impulses, mass, friction simulation.
- Networking or replication.
- ECS integration.
- Animation integration.
- Navmesh/pathfinding.
- Arbitrary gravity directions beyond accepting an `up` vector if simple.
- GPU/debug rendering as a requirement.

## Suggested tests

Create `tests/physics/CharacterController.cpp`.

Add test target:

- `stellar_character_controller_test`
- CTest name: `character_controller`

Required tests:

1. Empty world movement passes through unchanged.
2. Grounded state detected on floor.
3. Start-overlap with floor recovers.
4. Start-overlap with wall recovers.
5. Horizontal wall stop.
6. Slide along axis-aligned wall.
7. Slide along angled wall.
8. Corner collision stops cleanly.
9. Sphere/capsule catches triangle edge.
10. Sphere/capsule catches triangle vertex.
11. Walkable slope remains ground.
12. Too-steep slope is not ground.
13. Ground snap keeps character grounded over small drop.
14. Step-up succeeds for low obstacle.
15. Step-up fails for high obstacle.
16. No NaN/Inf outputs for degenerate triangles or zero displacement.

## CMake guidance

Add `src/physics/CharacterController.cpp` to `stellar_physics`.

Add test target near the existing collision world test:

```cmake
add_executable(stellar_character_controller_test
    tests/physics/CharacterController.cpp
)

target_include_directories(stellar_character_controller_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_character_controller_test PRIVATE
    stellar_physics
)

add_test(NAME character_controller COMMAND stellar_character_controller_test)
```

## Validation commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_collision_world_test stellar_character_controller_test -j$(nproc)
ctest --test-dir build -R '^(collision_world|character_controller)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- Existing `collision_world` tests still pass.
- New character controller handles static triangle collision more robustly than `move_sphere`.
- Start-overlap recovery works for shallow floor/wall penetrations.
- Edge and vertex contacts are covered by tests.
- Grounding, slope limits, ground snap, and step-up behavior are deterministic.
- No full physics engine is added.
- Public APIs have Doxygen `@brief` comments.
- Default tests remain display-free.

## Completion notes template

Append after implementation:

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 7B robust static character movement.
- Public API: <describe new controller/config/result types>.
- Movement behavior: <describe overlap recovery, sweep/slide, grounding, slope, snap, and step handling>.
- Limitations: <document what is still not a full physics solver>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Runtime world integration remains Phase 7C.
  - Runtime triggers remain Phase 7D.
  - Broadphase remains Phase 7E unless already needed for correctness.
```
