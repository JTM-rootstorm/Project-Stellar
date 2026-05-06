---
phase: MC-4
title: OpenGL macOS Fallback Decision and Diagnostics
depends_on: [MC-1]
can_parallelize: true
---

# MC-4 — OpenGL macOS Fallback Decision and Diagnostics

## Goal

Avoid treating the existing OpenGL path as a supported macOS target unless it is explicitly downgraded and validated.

## Files likely changed

```text
src/graphics/opengl/OpenGLGraphicsDevice.cpp
src/graphics/GraphicsBackend.cpp
tests/graphics/BackendSelection.cpp
tests/graphics/OpenGLContextSmoke.cpp
docs/ImplementationStatus.md
Plans/NEXT.md
```

## Problem

The current OpenGL device requests a 4.5 core context and uses shader source beginning with `#version 430 core`. Apple’s OpenGL stack is deprecated and macOS-facing APIs expose OpenGL 4.1 Core as the relevant modern profile. This means the current OpenGL path is unlikely to create a context or compile shaders on macOS.

## Recommendation

Do not spend too much time making OpenGL perfect on macOS. Use this phase only to produce clear diagnostics and, optionally, a temporary fallback.

## Tasks

Choose one path:

### Path A — Recommended: Metal-only support target on macOS

1. On Apple, leave OpenGL available only as `experimental/unsupported`.
2. When `--renderer opengl --validate-display` fails, print a diagnostic explaining:
   - requested OpenGL version,
   - current SDL video driver,
   - macOS OpenGL deprecation,
   - recommended `--renderer metal` once Metal backend lands.
3. Keep tests display-free by default.

### Path B — Temporary OpenGL fallback

1. On Apple only, request OpenGL 4.1 Core:
   ```cpp
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
   ```
2. Downgrade shader versions and features to GLSL 410-compatible syntax.
3. Add an opt-in `opengl_context_smoke` macOS test.
4. Clearly document that this is transitional and not the long-term macOS renderer.

## Acceptance criteria

- The project no longer silently assumes OpenGL 4.5 is usable on macOS.
- Error messages point developers toward Metal.
- Linux/default OpenGL behavior remains intact.
- No Metal enum/CLI alias is added unless MC-5 is also implemented.

## Validation

```bash
ctest --test-dir build -R '^(graphics_backend_selection|render_level_upload|render_level_inspection|opengl_context_smoke)$' --output-on-failure
```
