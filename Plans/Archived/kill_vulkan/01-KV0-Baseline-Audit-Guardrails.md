---
repo: "JTM-rootstorm/Project-Stellar"
branch: "kill-vulkan"
created_for: "Codex implementation handoff"
created_on: "2026-05-04"
status: "ready-for-implementation"
---

# KV-0 — Baseline, Audit, and Guardrails

## Goal

Establish the exact active Vulkan footprint on `kill-vulkan` before editing, and capture a baseline build/test result when the local environment allows it.

This phase prevents Codex from removing only the obvious backend files while leaving hidden CMake, CLI, docs, or tests nasties behind.

## Required Reading

Read these files before editing:

- `AGENTS.md`
- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `README.md`
- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `tests/cmake/GraphicsTests.cmake`
- `include/stellar/graphics/GraphicsBackend.hpp`
- `src/graphics/GraphicsBackend.cpp`
- `src/graphics/GraphicsDeviceFactory.cpp`
- `src/client/Application.cpp`
- `src/client/ApplicationConfig.cpp`
- `src/graphics/LevelRenderer.cpp`
- `tests/graphics/BackendSelection.cpp`
- `include/stellar/graphics/GraphicsDevice.hpp`

Skim, but do not deeply rewrite yet:

- `include/stellar/graphics/vulkan/VulkanGraphicsDevice.hpp`
- `src/graphics/vulkan/*`
- `tests/graphics/VulkanContextSmoke.cpp`

## Step 1 — Record Branch State

Run:

```bash
git status --short
git branch --show-current
git rev-parse HEAD
git log --oneline --decorate -5
git diff --stat
```

Expected:

- Current branch is `kill-vulkan`.
- Working tree state is known before edits.
- If uncommitted user work exists, stop and report it rather than overwriting it.

## Step 2 — Active Vulkan Audit

Run:

```bash
git grep -n -i \
  -e 'vulkan' \
  -e 'VK_' \
  -e 'Vk[A-Z]' \
  -e 'SDL_WINDOW_VULKAN' \
  -e 'STELLAR_ENABLE_VULKAN' \
  -e 'Vulkan::Vulkan' \
  -- include src tests CMakeLists.txt tests/cmake docs README.md Plans/NEXT.md ':!Plans/Archived/**'
```

Also run path-oriented audits:

```bash
find include src tests -path '*vulkan*' -o -path '*Vulkan*' | sort
git ls-files | grep -Ei '(^|/)vulkan|Vulkan'
```

Record active matches grouped by category:

- CMake/package/link/source list.
- Public graphics backend enum/parser/name.
- Factory/device construction.
- Client/window flag selection.
- Renderer math/backend-specific clip-space code.
- Tests/options.
- Active docs/status/readme.
- Backend implementation files to delete.

## Step 3 — Baseline Configure/Build/Test

Run if the environment has the current mandatory dependencies:

```bash
cmake -S . -B build-kill-vulkan-baseline -DCMAKE_BUILD_TYPE=Debug
cmake --build build-kill-vulkan-baseline -j$(nproc)
ctest --test-dir build-kill-vulkan-baseline --output-on-failure
tools/dev/check_target_boundaries.sh
```

If configure fails because Vulkan development files are missing, record that honestly as expected motivation for this removal branch. Do not install Vulkan just to make the old baseline pass unless the user specifically wants that.

## Step 4 — Decide Phase Ownership

Codex can safely split the implementation into these non-overlapping edit zones:

- Build/test config: `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/cmake/GraphicsTests.cmake`
- Runtime selection/factory/client: `GraphicsBackend.*`, `GraphicsDeviceFactory.cpp`, `Application*.cpp`, `LevelRenderer.cpp`, backend-selection tests
- File deletion: `include/stellar/graphics/vulkan/`, `src/graphics/vulkan/`, `tests/graphics/VulkanContextSmoke.cpp`
- Docs/status: `README.md`, `docs/Design.md`, `docs/ImplementationStatus.md`, `Plans/NEXT.md`, any active docs found by grep

Avoid parallel edits to the same file.

## Acceptance Criteria

- Baseline branch state recorded.
- Active Vulkan references are grouped.
- Baseline validation either passes or fails with clear environment/root cause.
- No source changes are made in this phase except optional notes if the user requested a committed plan, which they did not.
