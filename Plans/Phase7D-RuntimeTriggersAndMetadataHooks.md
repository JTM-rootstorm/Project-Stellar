# Phase 7D: Runtime Trigger Volumes and Metadata-Driven Gameplay Hooks

## Objective

Make `TRIGGER_*` world metadata useful at runtime through simple, deterministic overlap queries and state transitions.

This phase should not introduce a full ECS. It should provide a small backend-neutral trigger module that gameplay code can call.

## Primary agent recommendation

Use an agent strong in gameplay systems and deterministic tests.

Suggested primary: `@carmack`.

Consult `@miyamoto` only if sprite/render marker binding becomes relevant.

## Prerequisites

Recommended:

- Phase 7C runtime world assembly complete.

Acceptable alternative:

- Build directly on `WorldMetadataAsset` if Phase 7C is not complete, but do not duplicate integration logic that Phase 7C should own.

## Required reading

- `include/stellar/assets/WorldMetadataAsset.hpp`
- `src/import/gltf/WorldMetadataImport.cpp`
- `Plans/Phase6D-WorldMetadataFromGltf.md`
- `include/stellar/physics/CollisionWorld.hpp`
- Phase 7C completion notes if present
- `CMakeLists.txt`

## Scope

Implement simple runtime trigger overlap support.

Preferred new files:

- `include/stellar/world/TriggerSystem.hpp`
- `src/world/TriggerSystem.cpp`
- `tests/world/TriggerSystem.cpp`

Alternative:

- `include/stellar/physics/TriggerVolume.hpp` if keeping triggers under physics is cleaner.

## Recommended data model

```cpp
namespace stellar::world {

struct TriggerVolume {
    std::string name;
    std::array<float, 3> center{};
    std::array<float, 3> half_extents{0.5f, 0.5f, 0.5f};
};

struct TriggerOverlap {
    std::string name;
    bool entered = false;
    bool stayed = false;
    bool exited = false;
};

class TriggerSystem {
public:
    void set_triggers(std::span<const TriggerVolume> triggers);

    [[nodiscard]] std::vector<TriggerOverlap> update_sphere(
        std::array<float, 3> center,
        float radius) noexcept;
};

[[nodiscard]] std::vector<TriggerVolume> build_trigger_volumes(
    const stellar::assets::WorldMetadataAsset& metadata);

}
```

If `std::span` creates compatibility friction, use a vector reference.

## Recommended behavior

- `TRIGGER_*` markers become axis-aligned box trigger volumes.
- Marker position is the box center.
- Marker scale becomes extents or full size according to the policy already documented in metadata import. Pick one and document it clearly.
- Sphere-vs-AABB overlap is enough for this phase.
- System remembers previous overlap state so it can emit enter/stay/exit.
- Deterministic ordering by trigger name or source marker order.
- No gameplay scripts or callbacks yet; return data to caller.

## Implementation steps

### Step 1: Convert metadata to volumes

Implement `build_trigger_volumes(metadata)`.

Rules:

- Only `WorldMarkerType::kTrigger` becomes a trigger.
- Use marker name as trigger name.
- Use absolute marker scale for size.
- Clamp zero/negative extents to a small minimum or document zero-volume behavior.

### Step 2: Add sphere overlap query

Implement sphere-vs-AABB overlap.

Tests:

- Sphere outside does not overlap.
- Sphere touching edge overlaps according to documented inclusive policy.
- Sphere inside overlaps.
- Multiple triggers return deterministic ordering.

### Step 3: Add enter/stay/exit state

`TriggerSystem::update_sphere` should compare current overlaps to previous overlaps.

Tests:

- First overlap emits enter.
- Second frame emits stay.
- Leaving emits exit.
- Re-entering emits enter again.

### Step 4: Integrate with runtime world

If Phase 7C exists:

- Add optional helper to build a trigger system from `RuntimeWorld`.
- Keep state ownership outside `RuntimeWorld` unless there is a clear reason.

## Non-goals

Do not implement:

- Script execution.
- ECS entity events.
- Physics collision response.
- Complex trigger shapes.
- Rotated trigger boxes unless already trivial.
- Portal runtime behavior.
- Networking.
- Editor UI.

## Suggested tests

Create `tests/world/TriggerSystem.cpp`.

Required tests:

1. Metadata trigger marker becomes trigger volume.
2. Non-trigger markers are ignored.
3. Sphere outside AABB does not overlap.
4. Sphere touching AABB overlaps according to documented rule.
5. Sphere inside AABB overlaps.
6. Enter event emitted once.
7. Stay event emitted on continued overlap.
8. Exit event emitted on leaving.
9. Multiple trigger ordering is deterministic.
10. Empty trigger system returns no events.

## Validation commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_trigger_system_test -j$(nproc)
ctest --test-dir build -R '^trigger_system$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- Trigger metadata can be converted into runtime trigger volumes.
- Sphere-vs-trigger overlap works without graphics backends.
- Enter/stay/exit state is deterministic.
- No ECS or scripting dependency is added.
- Public APIs have Doxygen `@brief` comments.
- Default tests remain display-free.

## Completion notes template

Append after implementation:

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 7D runtime trigger volumes and metadata hooks.
- Public API: <describe trigger types/system>.
- Metadata conversion: <describe marker-to-volume rules>.
- Runtime behavior: <describe enter/stay/exit semantics>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - ECS/gameplay callback integration remains future work.
  - Rotated/non-box trigger shapes remain future work.
  - Portal runtime behavior remains future work.
```
