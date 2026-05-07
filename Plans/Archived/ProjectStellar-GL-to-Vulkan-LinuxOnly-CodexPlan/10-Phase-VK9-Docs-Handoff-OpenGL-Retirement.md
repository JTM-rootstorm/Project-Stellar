---
phase: "VK-9"
title: "Docs, Handoff, And OpenGL Retirement"
repo: "JTM-rootstorm/Project-Stellar"
branch: "GL-to-vulkan"
platform_scope: "Linux Vulkan default; macOS Metal default"
status: "ready"
---

# Phase VK-9 — Docs, Handoff, And OpenGL Retirement

## Objective

Once Linux Vulkan reaches render/readback parity and macOS Metal regression passes, update active docs and retire OpenGL/GLEW from active support.

## Documentation Files To Update

- `docs/ImplementationStatus.md`
- `docs/Design.md`
- `Plans/NEXT.md`
- `README.md`
- `CMakePresets.json`
- `AGENTS.md` if renderer routing references OpenGL/Vulkan parity
- Doxygen included pages if they describe current renderer status

## Required New Status Language

Use this direction:

```markdown
Linux uses Vulkan as the supported/default renderer. macOS uses the native Metal backend.
OpenGL has been retired from active support after the Linux Vulkan migration.
Vulkan is intentionally Linux-only; macOS Vulkan/MoltenVK is not part of the project renderer matrix.
```

## Remove Or Legacy-Gate OpenGL

After VK-8 passes, remove:

- `STELLAR_ENABLE_OPENGL_BACKEND`
- `STELLAR_ENABLE_OPENGL_CONTEXT_TESTS`
- `find_package(OpenGL)`
- GLEW package discovery/linking
- `include/stellar/graphics/opengl/`
- `src/graphics/opengl/`
- OpenGL parser aliases
- `SDL_WINDOW_OPENGL` routing
- OpenGL context smoke tests

If user wants a short fallback period, keep OpenGL behind:

```cmake
option(STELLAR_ENABLE_OPENGL_LEGACY_BACKEND "Build retired OpenGL backend temporarily" OFF)
```

But the recommended final state is removal to avoid drift.

## Final Renderer Matrix In Docs

| Platform | Renderer |
|---|---|
| Linux | Vulkan |
| macOS | Metal |

Explicitly unsupported:

- macOS Vulkan
- MoltenVK
- OpenGL production backend

## README Setup

Linux setup should mention Vulkan SDK or distro Vulkan packages.

macOS setup should mention Metal and must not require Vulkan SDK.

## Audits

No active OpenGL dependency audit:

```bash
git grep -n -i \
  -e 'glew' \
  -e 'SDL_WINDOW_OPENGL' \
  -e 'STELLAR_ENABLE_OPENGL_BACKEND' \
  -e 'find_package(OpenGL' \
  -e 'OpenGLGraphicsDevice' \
  -- include src tests CMakeLists.txt CMakePresets.json tests/cmake docs README.md Plans/NEXT.md ':!Plans/Archived/**'
```

No macOS Vulkan/MoltenVK audit:

```bash
git grep -n -i \
  -e 'moltenvk' \
  -e 'VK_KHR_portability' \
  -e 'macos-vulkan' \
  -e 'macOS Vulkan' \
  -- include src tests CMakeLists.txt CMakePresets.json tests/cmake docs README.md Plans/NEXT.md ':!Plans/Archived/**'
```

Expected:

- No active MoltenVK/macOS Vulkan references.
- OpenGL references only in historical notes or removed entirely from active code.

## Final Validation

Linux:

```bash
cmake --preset linux-vulkan-only
cmake --build --preset linux-vulkan-only --parallel $(nproc)
ctest --preset linux-vulkan-only --output-on-failure
tools/dev/check_target_boundaries.sh
```

macOS:

```bash
cmake --preset macos-metal-only
cmake --build --preset macos-metal-only --parallel $(sysctl -n hw.ncpu)
ctest --preset macos-metal-only --output-on-failure
build-macos-metal-only/stellar-client --validate-display --renderer metal
```

## Final Handoff Template

```markdown
## GL-to-Vulkan Linux-Only Completion Notes

Branch: GL-to-vulkan
Date: YYYY-MM-DD
Commit: <sha or "not committed">

Summary:
- Added Linux-only Vulkan backend.
- Preserved macOS Metal backend.
- Retired OpenGL/GLEW from active support.
- Kept server/gameplay/audio/import/backend boundaries intact.
- Added Vulkan shader build, resource upload, draw, material, and readback support.
- Updated docs to Linux=Vulkan, macOS=Metal.

Validation:
- linux-vulkan-only configure/build/CTest: <result>
- macos-metal-only configure/build/CTest: <result>
- Linux opt-in Vulkan display/readback: <result or not run with reason>
- macOS Metal display smoke: <result or not run with reason>
- target boundaries: <result>
- OpenGL audit: <result>
- macOS Vulkan/MoltenVK audit: <result>

Remaining follow-up:
- <none or explicit items>
```

## Acceptance Criteria

- Active docs agree with implementation.
- Linux builds no longer require OpenGL/GLEW.
- macOS builds do not require Vulkan.
- `--renderer vulkan` works on Linux and fails clearly on macOS.
- `--renderer metal` works on macOS.
- OpenGL is retired or explicitly marked as temporary legacy if the user requests a fallback window.
