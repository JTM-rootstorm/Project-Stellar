---
repo: "JTM-rootstorm/Project-Stellar"
branch: "kill-vulkan"
created_for: "Codex implementation handoff"
created_on: "2026-05-04"
status: "ready-for-implementation"
---

# KV-3 — Delete Vulkan Backend Implementation and Active Vulkan Tests

## Goal

Remove the physical Vulkan backend and smoke test files after build/runtime references are unhooked.

This phase turns the removal from “unused code” into real maintenance reduction.

## Files/Paths To Delete

Delete these active implementation paths:

```text
include/stellar/graphics/vulkan/
src/graphics/vulkan/
tests/graphics/VulkanContextSmoke.cpp
```

Expected files include:

```text
include/stellar/graphics/vulkan/VulkanGraphicsDevice.hpp
src/graphics/vulkan/VulkanGraphicsDevice.cpp
src/graphics/vulkan/VulkanGraphicsDeviceCommon.cpp
src/graphics/vulkan/VulkanGraphicsDeviceDevice.cpp
src/graphics/vulkan/VulkanGraphicsDeviceSwapchain.cpp
src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp
src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp
src/graphics/vulkan/VulkanGraphicsDevicePrivate.hpp
tests/graphics/VulkanContextSmoke.cpp
```

If additional files are found under those directories, delete them too unless they are clearly unrelated historical notes.

## Tasks

### 1. Delete files

Use git-aware deletes:

```bash
git rm -r include/stellar/graphics/vulkan src/graphics/vulkan
git rm tests/graphics/VulkanContextSmoke.cpp
```

If `git rm` reports files already deleted by a previous phase, confirm the path is absent and proceed.

### 2. Remove raw Vulkan includes/types from active code

Run:

```bash
git grep -n \
  -e '<vulkan/vulkan.h>' \
  -e 'Vk[A-Z]' \
  -e 'VK_' \
  -- include src tests ':!Plans/Archived/**'
```

Expected:

- No matches in active code/tests after KV-3.
- Any remaining matches are stale docs/status and should be handled in KV-4.

### 3. Re-run build after deletion

```bash
cmake -S . -B build-kv3 -DCMAKE_BUILD_TYPE=Debug
cmake --build build-kv3 -j$(nproc)
ctest --test-dir build-kv3 -R '^(graphics_backend_selection|render_level_upload|render_level_inspection)' --output-on-failure
```

## Important Guardrails

Do not delete:

- `include/stellar/graphics/GraphicsDevice.hpp`
- `include/stellar/graphics/GraphicsBackend.hpp`
- `src/graphics/GraphicsDeviceFactory.cpp`
- `src/graphics/opengl/`
- `tests/graphics/OpenGLContextSmoke.cpp`
- display-free `RenderLevel` / graphics inspection tests

Do not replace Vulkan deletion with stubs. No `NullVulkanGraphicsDevice`, no compile-time `#if 0`, no “temporarily disabled” backend. The active project should not carry Vulkan backend code.

## Acceptance Criteria

- Active Vulkan implementation directories are gone.
- Active Vulkan context smoke test file is gone.
- Build does not reference deleted paths.
- No raw Vulkan SDK include/types/macros remain in active include/src/tests.
