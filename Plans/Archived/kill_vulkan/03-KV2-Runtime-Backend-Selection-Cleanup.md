---
repo: "JTM-rootstorm/Project-Stellar"
branch: "kill-vulkan"
created_for: "Codex implementation handoff"
created_on: "2026-05-04"
status: "ready-for-implementation"
---

# KV-2 — Runtime Backend Selection Cleanup

## Goal

Make OpenGL the only accepted runtime backend while preserving the abstraction seam for future DirectX/Metal support.

This phase removes selectable-but-unwanted Vulkan behavior from user-facing CLI/config and internal factories.

## Files To Edit

Primary:

- `include/stellar/graphics/GraphicsBackend.hpp`
- `src/graphics/GraphicsBackend.cpp`
- `src/graphics/GraphicsDeviceFactory.cpp`
- `src/client/Application.cpp`
- `src/client/ApplicationConfig.cpp`
- `src/graphics/LevelRenderer.cpp`
- `tests/graphics/BackendSelection.cpp`
- client CLI/config tests that assert renderer parsing behavior

Possibly:

- `include/stellar/client/ApplicationConfig.hpp` if comments mention OpenGL/Vulkan.
- Any active docs/comments in public headers.

## Tasks

### 1. Make `GraphicsBackend` OpenGL-only

In `include/stellar/graphics/GraphicsBackend.hpp`:

- Remove `GraphicsBackend::kVulkan`.
- Update Doxygen/comments to say OpenGL is the current implemented backend.
- Keep the enum and helper functions; do not remove the abstraction.

Target shape:

```cpp
/**
 * @brief Runtime-selectable graphics backend.
 *
 * Currently only OpenGL is implemented. Future native backends such as
 * DirectX or Metal should be added here only when their implementations exist.
 */
enum class GraphicsBackend {
    /** @brief OpenGL backend. */
    kOpenGL,
};
```

### 2. Remove Vulkan parser aliases

In `src/graphics/GraphicsBackend.cpp`:

- Accept only:
  - `opengl`
  - `gl`
  - `OpenGL`
- Reject:
  - `vulkan`
  - `vk`
  - `Vulkan`

Use a clear error message:

```text
Unsupported graphics backend: vulkan (expected opengl)
```

`graphics_backend_name()` should return `"opengl"` for all current cases.

Do not add placeholder parsing for `directx`, `d3d`, `metal`, or `mtl` yet.

### 3. Remove Vulkan factory construction

In `src/graphics/GraphicsDeviceFactory.cpp`:

- Delete `#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"`.
- Delete the `case GraphicsBackend::kVulkan`.
- Return `std::make_unique<opengl::OpenGLGraphicsDevice>()` for `kOpenGL` and default.

Target shape:

```cpp
std::unique_ptr<GraphicsDevice> create_graphics_device(GraphicsBackend backend) {
    switch (backend) {
        case GraphicsBackend::kOpenGL:
        default:
            return std::make_unique<opengl::OpenGLGraphicsDevice>();
    }
}
```

### 4. Remove Vulkan SDL window flag path

In `src/client/Application.cpp`:

- Replace `backend_window_flags()` with an OpenGL-only function or inline `SDL_WINDOW_OPENGL`.
- Remove all references to `SDL_WINDOW_VULKAN`.
- Keep `--validate-display` behavior, but it should validate OpenGL only unless another backend exists later.

Target behavior:

```cpp
Uint32 backend_window_flags(stellar::graphics::GraphicsBackend /*backend*/) noexcept {
    return SDL_WINDOW_OPENGL;
}
```

or eliminate the helper entirely.

### 5. Remove Vulkan projection/clip-space branch

In `src/graphics/LevelRenderer.cpp`, remove the Vulkan-specific clip-space correction:

```cpp
if (backend == GraphicsBackend::kVulkan) { ... }
```

Simplest current target:

```cpp
glm::mat4 make_projection_for_backend(GraphicsBackend /*backend*/,
                                      float vertical_fov_degrees,
                                      float aspect,
                                      float near_plane,
                                      float far_plane) {
  return glm::perspective(glm::radians(vertical_fov_degrees),
                          aspect, near_plane, far_plane);
}
```

Keep the helper if it is useful as a future backend seam, but make it OpenGL-only today.

### 6. Update tests

In `tests/graphics/BackendSelection.cpp` and any client CLI tests:

Required expectations:

- `parse_graphics_backend("opengl")` succeeds with `kOpenGL`.
- `parse_graphics_backend("gl")` succeeds with `kOpenGL`.
- `graphics_backend_name(GraphicsBackend::kOpenGL) == "opengl"`.
- `parse_graphics_backend("vulkan")` fails.
- `parse_graphics_backend("vk")` fails.
- CLI `--renderer vulkan` and `--graphics-backend vulkan` should fail config parsing with the unsupported-backend error.

If existing tests treated Vulkan as valid, rewrite them to treat it as rejected.

## Validation

Run focused tests first:

```bash
cmake --build build-kv1 -j$(nproc) || cmake -S . -B build-kv2 -DCMAKE_BUILD_TYPE=Debug
cmake --build build-kv2 -j$(nproc)
ctest --test-dir build-kv2 -R '^(graphics_backend_selection|client_cli|client_map_validation)' --output-on-failure
```

Then run source grep:

```bash
git grep -n -i \
  -e 'kVulkan' \
  -e 'SDL_WINDOW_VULKAN' \
  -e 'renderer vulkan' \
  -e 'graphics-backend vulkan' \
  -- include src tests ':!Plans/Archived/**'
```

Expected:

- No active code references `kVulkan`.
- `vulkan` may remain in negative tests only if those tests assert it fails.

## Acceptance Criteria

- Vulkan is no longer runtime-selectable.
- CLI/config reject `vulkan` and `vk`.
- Factory only constructs OpenGL.
- Client windows are created with OpenGL flags only.
- Projection math no longer has Vulkan-only correction.
- Backend abstraction remains intact for future DirectX/Metal additions.
