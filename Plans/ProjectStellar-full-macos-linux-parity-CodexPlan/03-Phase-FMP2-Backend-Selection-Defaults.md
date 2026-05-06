---
phase: FMP-2
title: Backend Selection, Defaults, and macOS OpenGL Policy
depends_on: [FMP-1]
can_parallelize: false
---

# FMP-2 — Backend Selection, Defaults, and macOS OpenGL Policy

## Goal

Make renderer selection deterministic and user-friendly on every platform.

## Tasks

1. Decide default backend:
   - Linux/default: OpenGL.
   - macOS with Metal built: Metal should become the recommended final renderer once parity is complete.
2. Add helpers:
   ```cpp
   GraphicsBackend default_graphics_backend() noexcept;
   bool graphics_backend_available(GraphicsBackend backend) noexcept;
   ```
3. Make unsupported backend errors mention backend name, compiled backends, and suggested backend for current platform.
4. Add CLI tests for `--renderer metal`, `--renderer mtl`, `--renderer vulkan`, and `--renderer opengl` when OpenGL is disabled.
5. Keep macOS OpenGL documented as unsupported/experimental unless a real supported fallback is added.

## Acceptance criteria

- Backend selection behavior is deterministic and tested.
- User-facing messages are clear.
- Linux behavior is unchanged unless explicitly intended.
- macOS Metal builds can run `stellar-client --validate-config --renderer metal`.

## Validation

```bash
ctest --test-dir build -R '^(graphics_backend_selection|client_cli)' --output-on-failure
ctest --test-dir build-macos-metal -R '^(graphics_backend_selection|client_cli)' --output-on-failure
build-macos-metal/stellar-client --validate-config --renderer metal
build-macos-metal/stellar-client --validate-config --renderer vulkan && exit 1 || true
```
