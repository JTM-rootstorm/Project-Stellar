---
repo: "JTM-rootstorm/Project-Stellar"
branch: "kill-vulkan"
created_for: "Codex implementation handoff"
created_on: "2026-05-04"
status: "ready-for-implementation"
---

# KV-5 — Final Validation, Audits, and Handoff

## Goal

Prove Vulkan support has been removed from active code, build configuration, tests, and active docs while preserving OpenGL functionality and future backend seams.

## Step 1 — Full Active Vulkan Grep Audit

Run:

```bash
git grep -n -i \
  -e 'vulkan' \
  -e 'VK_' \
  -e 'Vk[A-Z]' \
  -e 'SDL_WINDOW_VULKAN' \
  -e 'STELLAR_ENABLE_VULKAN' \
  -e 'Vulkan::Vulkan' \
  -e 'Vulkan_INCLUDE_DIRS' \
  -- include src tests CMakeLists.txt tests/cmake docs README.md Plans/NEXT.md ':!Plans/Archived/**'
```

Expected:

- No active code/build/test matches.
- Active docs may contain only a short historical/current-status note such as “Vulkan support was intentionally removed.”
- If active docs still contain commands like `--renderer vulkan`, remove them.

Also run:

```bash
find include src tests -path '*vulkan*' -o -path '*Vulkan*' | sort
git ls-files | grep -Ei '(^|/)vulkan|Vulkan' | grep -v '^Plans/Archived/' || true
```

Expected:

- No active Vulkan files remain.

## Step 2 — Configure/Build/Test

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh
```

Run focused tests relevant to the removal:

```bash
ctest --test-dir build -R '^(graphics_backend_selection|render_level_upload|render_level_inspection|target_boundary|client_cli|client_map_validation|server_cli|bsp_|trenchbroom|world_axes|collision_world|runtime_world|server_world_session|scripted_world_session)' --output-on-failure
```

## Step 3 — CLI Behavior Checks

Run display-free checks:

```bash
build/stellar-client --validate-config
build/stellar-client --renderer opengl --validate-config
build/stellar-client --graphics-backend opengl --validate-config
```

Run negative parser checks:

```bash
build/stellar-client --renderer vulkan --validate-config
build/stellar-client --graphics-backend vk --validate-config
```

Expected:

- OpenGL commands succeed.
- Vulkan/vk commands fail early with unsupported-backend message.
- Negative checks should be recorded as expected failures, not test failures.

If the test suite has CLI tests, encode these expectations there rather than relying only on manual commands.

## Step 4 — Optional Link/Package Checks

If running on Linux:

```bash
ldd build/stellar-client | grep -i vulkan && echo "unexpected Vulkan runtime dependency"
```

Expected:

- No Vulkan runtime dependency reported due to project links.
- Be careful: system OpenGL/SDL may internally dlopen or depend on graphics stack libraries; only flag direct project link surprises.

Optional no-Vulkan-SDK configure:

```bash
cmake -S . -B build-no-vulkan-sdk -DCMAKE_BUILD_TYPE=Debug
```

Run this in an environment/container without Vulkan development headers/packages if available. If not available, record “not run” and rely on the CMake grep showing no `find_package(Vulkan)`.

## Step 5 — Review Diff Shape

Run:

```bash
git status --short
git diff --stat
git diff -- CMakeLists.txt tests/CMakeLists.txt tests/cmake/GraphicsTests.cmake
git diff -- include/stellar/graphics/GraphicsBackend.hpp src/graphics/GraphicsBackend.cpp src/graphics/GraphicsDeviceFactory.cpp src/client/Application.cpp src/graphics/LevelRenderer.cpp
```

Expected diff shape:

- CMake removes Vulkan package/link/source/test target hooks.
- Runtime selection rejects Vulkan and constructs OpenGL only.
- Vulkan files are deleted.
- Docs status updated.
- No unrelated gameplay/server/collision rewrites.

## Final Handoff Note

Add a final note for the user or implementation PR:

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
- CLI negative `--renderer vulkan`: <result>
- Optional no-Vulkan-SDK configure: <result or not run with reason>
- Optional direct-link audit: <result or not run with reason>

Remaining follow-up:
- None for active Vulkan removal.
- Future DirectX/Metal support should be planned separately as concrete backend additions through the existing abstraction.
```

## Acceptance Criteria

- Full default build/test suite passes.
- No active CMake/test/source dependency on Vulkan remains.
- User-facing CLI rejects Vulkan.
- Active docs no longer advertise Vulkan as supported.
- Backend-neutral architecture remains available for future DirectX/Metal.
- Final handoff is honest about any tests not run.
