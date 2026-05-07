---
phase: FMP-0
title: Gap Audit and Acceptance Matrix
depends_on: []
can_parallelize: false
---

# FMP-0 — Gap Audit and Acceptance Matrix

## Goal

Freeze the exact full-parity target before writing renderer/runtime changes.

## Tasks

1. Create a tracked acceptance matrix with rows for configure/build, CTest, `stellar-client --validate-config`, `--validate-display`, `--validate-map`, single-player `--map`, listen-host, remote client, dedicated server, generated footstep audio, TrenchBroom/BSP tooling, and renderer material fixtures.
2. Columns: Linux OpenGL default, macOS default build, macOS Metal build, macOS Metal-only build.
3. Mark each row: `PASS`, `FAIL`, `SKIP_EXPECTED`, or an uncovered status for missing evidence.
4. Add a short note: full parity is not reached until no required row lacks coverage or fails.
5. Audit current code:
   ```bash
   git grep -n 'STELLAR_ENABLE_METAL\|kMetal\|SDL_WINDOW_METAL\|MetalGraphicsDevice' -- CMakeLists.txt include src tests docs Plans/NEXT.md
   git grep -n 'lightmap_texture\|normal_texture\|specular_texture\|metallic_roughness\|emissive_texture\|occlusion_texture' -- include src/graphics tests/graphics
   git grep -n 'MSG_NOSIGNAL\|SO_NOSIGPIPE\|POSITION_INDEPENDIENT_CODE' -- .
   ```

## Acceptance criteria

- A concrete acceptance matrix exists.
- Full compatibility/parity criteria are explicit.
- Current missing Metal parity items are listed as active blockers.

## Validation

```bash
git diff --check
```
