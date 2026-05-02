# Phase 1 — Z-up World-Axis Contract

Branch target: `trenchbroom-compat`

## Goal

Create a central, explicit Z-up world-axis contract and begin replacing implicit Y-up literals with named constants. This phase should make the desired world convention obvious before deeper runtime changes begin.

## End-state convention

- Right axis: +X.
- Forward axis: +Y, unless a specific gameplay/camera module documents another horizontal convention.
- Up axis: +Z.
- Gravity/down axis: -Z.
- Horizontal gameplay plane: X/Y.
- Vertical coordinate: Z.
- 1 unit = 1 inch.
- Default player center on floor: `z = 36`.

## Tasks

### 1.1 Add central axis constants

Preferred option:

```text
include/stellar/core/WorldAxes.hpp
```

Suggested content shape:

```cpp
#pragma once

#include <array>

namespace stellar::core {

inline constexpr std::array<float, 3> kWorldRight{1.0F, 0.0F, 0.0F};
inline constexpr std::array<float, 3> kWorldForward{0.0F, 1.0F, 0.0F};
inline constexpr std::array<float, 3> kWorldUp{0.0F, 0.0F, 1.0F};
inline constexpr std::array<float, 3> kWorldDown{0.0F, 0.0F, -1.0F};

inline constexpr int kWorldRightIndex = 0;
inline constexpr int kWorldForwardIndex = 1;
inline constexpr int kWorldUpIndex = 2;

inline constexpr float kDefaultPlayerSpawnHeightInches = kPlayerHeightInches * 0.5F;

} // namespace stellar::core
```

If `WorldUnits.hpp` is the better home, add the axis constants there instead, but avoid scattering axis constants across unrelated headers.

### 1.2 Add GLM helpers if needed

If camera/presentation uses `glm::vec3`, add helpers in a small header or local conversion utilities:

```cpp
glm::vec3 world_up_glm();
glm::vec3 world_forward_glm();
glm::vec3 world_right_glm();
```

Keep this out of low-level headers if that would pull GLM into core areas that currently avoid it.

### 1.3 Replace obvious Y-up defaults

Update active code defaults such as:

- character movement input up;
- character move result ground normal;
- default gravity/down vectors;
- player spawn offset helpers;
- camera up vectors;
- billboard up basis;
- no-map fallback camera/world basis;
- any hardcoded ground plane tests.

Use `kWorldUp`/`kWorldDown` rather than repeating `{0, 0, 1}`.

### 1.4 Add axis-contract tests

Add small display-free tests that assert:

- world up is +Z;
- world down is -Z;
- default player spawn height is 36 inches;
- horizontal plane indices are X and Y;
- any conversion helpers preserve finite values.

Suggested target:

```text
tests/core/world_axes_test.cpp
```

### 1.5 Documentation stub updates

Update active docs enough that later phases are not written against Y-up:

- `docs/Design.md`: mark active branch direction as `trenchbroom-compat` and state planned end-state is Z-up.
- `docs/BspAuthoring.md`: add a WIP note that the branch is migrating from Y-up to Z-up and final examples will use `origin = "0 0 36"`.

Do not fully rewrite examples until Phase 7 unless the corresponding code has already changed.

## Files likely changed

- `include/stellar/core/WorldUnits.hpp`
- `include/stellar/core/WorldAxes.hpp` if new
- `include/stellar/physics/CharacterController.hpp`
- presentation/camera headers or sources using up vectors
- tests under `tests/core`, `tests/physics`, `tests/client`
- `docs/Design.md`
- `docs/BspAuthoring.md`
- `CMakeLists.txt` if adding a new test target

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_core_world_axes_test -j$(nproc) || true
ctest --test-dir build -R '^(world_axes|character_controller|collision_validation)' --output-on-failure
```

If the new target name differs, update the command.

Broad:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- [ ] A named Z-up axis contract exists in active code.
- [ ] New tests prove up is +Z.
- [ ] `CharacterMoveInput` and `CharacterMoveResult` defaults no longer use +Y up.
- [ ] Active docs indicate Z-up migration is in progress.
- [ ] Existing tests either pass or failures are isolated to expected downstream Y-up assumptions scheduled for Phase 2/3.
- [ ] No importer-side hidden scale conversion is added.

## Parallelization

Safe in parallel after the central header lands:

- Agent A: core header + tests.
- Agent B: physics defaults.
- Agent C: docs stubs.
- Agent D: grep audit and issue list.

Avoid concurrent edits to the same movement/camera source files.
