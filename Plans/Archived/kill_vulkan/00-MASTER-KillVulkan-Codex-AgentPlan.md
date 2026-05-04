---
repo: "JTM-rootstorm/Project-Stellar"
branch: "kill-vulkan"
created_for: "Codex implementation handoff"
created_on: "2026-05-04"
status: "ready-for-implementation"
---

# MASTER — Remove Vulkan Support, Keep Future Native Backend Door Open

## 0. Mission

Remove Vulkan as a supported, build-linked, runtime-selectable backend from Project Stellar on branch `kill-vulkan`.

The desired current-state renderer is:

- **OpenGL only** for production/runtime rendering.
- **No mandatory Vulkan SDK, loader, headers, libraries, CMake package, or test target.**
- The existing backend-neutral graphics abstraction remains valuable and should not be collapsed into ad-hoc OpenGL calls.
- Future Windows/macOS renderer work remains possible through the abstraction, likely as DirectX/Direct3D and Metal backends when explicitly scoped later.

This plan intentionally removes maintenance burden without over-building future DirectX/Metal scaffolding now.

## 1. Current Evidence Snapshot

Branch `kill-vulkan` currently still contains Vulkan support in active code/build paths.

Observed active Vulkan integration points:

- `CMakeLists.txt`
  - `find_package(Vulkan REQUIRED)`
  - `src/graphics/vulkan/VulkanGraphicsDevice*.cpp` in `stellar_graphics`
  - `${Vulkan_INCLUDE_DIRS}` in include directories
  - `Vulkan::Vulkan` in `stellar_graphics` link libraries
- `include/stellar/graphics/GraphicsBackend.hpp`
  - `GraphicsBackend::kVulkan`
  - parser documentation for `vulkan` / `vk`
- `src/graphics/GraphicsBackend.cpp`
  - accepts `vulkan`, `vk`, and `Vulkan`
  - returns `vulkan` from `graphics_backend_name`
- `src/graphics/GraphicsDeviceFactory.cpp`
  - includes `stellar/graphics/vulkan/VulkanGraphicsDevice.hpp`
  - constructs `vulkan::VulkanGraphicsDevice`
- `src/client/Application.cpp`
  - selects `SDL_WINDOW_VULKAN` when the backend is Vulkan
- `src/graphics/LevelRenderer.cpp`
  - applies Vulkan-specific clip-space correction
- `tests/CMakeLists.txt`
  - declares `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS`
- `tests/cmake/GraphicsTests.cmake`
  - conditionally builds `tests/graphics/VulkanContextSmoke.cpp`
- `include/stellar/graphics/vulkan/` and `src/graphics/vulkan/`
  - contain the backend implementation and raw Vulkan handles/types
- Active docs/status still describe OpenGL/Vulkan parity in several places.

Branch comparison note: `kill-vulkan` was one commit ahead of `main` during inspection, but the compared commit only moved spec-normal-texture plan files into `Plans/Archived/`. It did **not** remove Vulkan yet.

## 2. Non-Goals

Do not implement DirectX, Direct3D 11/12, Metal, MoltenVK, SDL GPU, bgfx, or wgpu in this branch.

Do not rewrite the renderer architecture. Keep:

- `GraphicsDevice`
- `GraphicsBackend`
- `GraphicsDeviceFactory`
- `LevelRenderer`
- backend-neutral material/mesh/texture upload contracts
- display-free importer/runtime/render submission tests

Do not edit archived historical plans except for a tiny note only if a stale active index explicitly points at them as current. Prefer updating active docs/status.

Do not make default tests require a display, GPU, OpenGL context, or Vulkan context.

## 3. Future Backend Door To Leave Open

Keep the abstraction shape, but make current enum/config honest.

Recommended current model:

```cpp
enum class GraphicsBackend {
    kOpenGL,
};
```

Future DirectX/Metal can later add:

```cpp
kDirectX,
kMetal,
```

when implementation exists.

Future backend integration seams to preserve:

- `parse_graphics_backend()`
- `graphics_backend_name()`
- `create_graphics_device(GraphicsBackend)`
- backend-aware platform window/context creation in `Application.cpp`
- backend-specific projection/clip-space handling in renderer math when new APIs need it
- backend-neutral `GraphicsDevice` resource contracts

Avoid placeholder enum values for DirectX/Metal now because they would create selectable-but-unimplemented backends, which is the exact class of maintenance nasty this branch is removing.

## 4. Phase Map

| Phase | Name | Purpose | Can overlap? |
|---|---|---|---|
| KV-0 | Baseline and audit | Record branch state, active Vulkan references, and current validation baseline. | No; do first. |
| KV-1 | Build/dependency removal | Remove hard Vulkan package/link/source/test target requirements. | Can overlap with KV-2 only after agreeing on file ownership. |
| KV-2 | Runtime selection cleanup | Make OpenGL the only accepted runtime backend and preserve future seams. | Can run in parallel with KV-3 only if tests are coordinated. |
| KV-3 | Delete Vulkan backend files/tests | Remove implementation files and active test files. | After KV-1 or alongside it. |
| KV-4 | Docs/status cleanup | Make active docs say OpenGL-only now, future DirectX/Metal possible later. | Can start after KV-2 decisions are clear. |
| KV-5 | Final validation and handoff | Prove no active Vulkan requirements remain and all default tests pass. | No; do last. |

## 5. Global Guardrails For Codex

- Work on branch `kill-vulkan`.
- Keep changes minimal, reviewable, and phased.
- Do not collapse public graphics abstractions into OpenGL-specific APIs.
- Keep Doxygen `@brief` docs on public API changes.
- Keep fallible setup/upload paths using `std::expected<T, stellar::platform::Error>`.
- Keep frame/destroy methods `noexcept` where they are currently `noexcept`.
- Preserve C++23 and CMake 3.20+ compatibility.
- Keep `stellar-server` and server-side targets free of graphics/window dependencies.
- Keep default CTest display-free.
- Do not add new mandatory dependencies.
- Do not commit unless the user explicitly asks for implementation commits.

## 6. Required Final State

Final active-source expectations:

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

Expected result after KV-4/KV-5:

- No active code/build/test matches.
- No active docs present Vulkan as current support.
- Archived historical plans may still contain Vulkan references and should be excluded from the gate.

Final validation commands:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh
ctest --test-dir build -R '^(graphics_backend_selection|render_level_upload|render_level_inspection|target_boundary|client_cli|client_map_validation|server_cli|bsp_|trenchbroom|world_axes|collision_world|runtime_world|server_world_session|scripted_world_session)' --output-on-failure
```

Optional but strongly preferred:

```bash
ldd build/stellar-client | grep -i vulkan && echo "unexpected Vulkan runtime dependency"
cmake -S . -B build-no-vulkan-sdk -DCMAKE_BUILD_TYPE=Debug
```

Run `build-no-vulkan-sdk` in an environment without Vulkan development files when available. If the environment still has Vulkan installed, record that the configure no longer calls `find_package(Vulkan REQUIRED)` and no target links `Vulkan::Vulkan`.

## 7. Final Handoff Template

```markdown
## Kill Vulkan Completion Notes

Branch: kill-vulkan
Date: YYYY-MM-DD
Commit: <sha or "not committed">

Summary:
- Removed Vulkan from active CMake dependencies, graphics source lists, include dirs, link libs, and opt-in test targets.
- Removed `GraphicsBackend::kVulkan`, Vulkan parser aliases, Vulkan factory construction, and Vulkan SDL window flag selection.
- Deleted active Vulkan backend implementation files and Vulkan context smoke test.
- Preserved the backend-neutral graphics abstraction for future DirectX/Metal support.
- Updated active docs/status to describe OpenGL as the current renderer and DirectX/Metal as future possibilities only.

Validation:
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`: <result>
- `cmake --build build -j$(nproc)`: <result>
- `ctest --test-dir build --output-on-failure`: <result>
- `tools/dev/check_target_boundaries.sh`: <result>
- Vulkan grep audit: <result>
- Optional no-Vulkan-SDK configure: <result or not run with reason>
- Optional `ldd build/stellar-client | grep -i vulkan`: <result or not run with reason>

Remaining follow-up:
- None for Vulkan removal.
- Future native renderer support should be explicitly planned as DirectX/Metal backend additions through the existing abstraction.
```
