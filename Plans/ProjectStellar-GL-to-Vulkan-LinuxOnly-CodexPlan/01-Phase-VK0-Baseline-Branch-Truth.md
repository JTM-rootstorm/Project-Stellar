---
phase: "VK-0"
title: "Baseline And Branch Truth"
repo: "JTM-rootstorm/Project-Stellar"
branch: "GL-to-vulkan"
platform_scope: "Linux Vulkan only; macOS Metal preserved"
status: "ready"
---

# Phase VK-0 — Baseline And Branch Truth

## Objective

Record the current renderer/build/docs state on `GL-to-vulkan` before any code edits. This phase prevents Codex from accidentally treating old Vulkan-removal guardrails as still authoritative.

## Key Decision

This branch intentionally reintroduces Vulkan for Linux only.

Do not reintroduce Vulkan on macOS. macOS stays on Metal.

## Files To Inspect

- `docs/ImplementationStatus.md`
- `docs/Design.md`
- `Plans/NEXT.md`
- `CMakeLists.txt`
- `CMakePresets.json`
- `include/stellar/graphics/GraphicsBackend.hpp`
- `src/graphics/GraphicsBackend.cpp`
- `src/graphics/GraphicsDeviceFactory.cpp`
- `src/client/Application.cpp`
- `tests/CMakeLists.txt`
- `tests/cmake/GraphicsTests.cmake`
- `AGENTS.md`

## Commands

```bash
git status
git branch --show-current
git rev-parse HEAD
```

Expected branch: `GL-to-vulkan`.

Renderer audit:

```bash
git grep -n -i \
  -e 'opengl' \
  -e 'glew' \
  -e 'metal' \
  -e 'vulkan' \
  -e 'SDL_WINDOW_OPENGL' \
  -e 'SDL_WINDOW_METAL' \
  -e 'SDL_WINDOW_VULKAN' \
  -e 'STELLAR_ENABLE_OPENGL_BACKEND' \
  -e 'STELLAR_ENABLE_METAL' \
  -e 'STELLAR_ENABLE_VULKAN' \
  -- include src tests CMakeLists.txt CMakePresets.json tests/cmake docs README.md Plans/NEXT.md ':!Plans/Archived/**'
```

Baseline validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh
```

On macOS, use:

```bash
cmake --preset macos-metal
cmake --build --preset macos-metal --parallel $(sysctl -n hw.ncpu)
ctest --preset macos-metal --output-on-failure
```

## Required Documentation Note

Create or update:

```text
Plans/GLToVulkanLinuxOnly-CodexPlan/00-MASTER-GLToVulkanLinuxOnly-CodexPlan.md
```

If adding this plan into the repo, make sure `docs/ImplementationStatus.md` points at it as active scope.

## Acceptance Criteria

- Baseline commands have recorded pass/fail results.
- Active docs that conflict with Linux-only Vulkan direction are listed.
- No implementation changes are made in this phase unless needed to add the plan file.
- Branch status is clean or all local changes are accounted for.
- Codex reports any inability to run validation and gives exact commands for the user to run.

## Handoff Snippet

```markdown
## VK-0 Baseline Notes

Branch: GL-to-vulkan
Commit: <sha>
Date: <date>

Summary:
- Current renderer defaults:
- Current CMake backend options:
- Current active docs that need renderer updates:
- Validation results:

Risks:
- <items>

Next phase:
- VK-1 Linux Vulkan build/dependency/presets.
```
