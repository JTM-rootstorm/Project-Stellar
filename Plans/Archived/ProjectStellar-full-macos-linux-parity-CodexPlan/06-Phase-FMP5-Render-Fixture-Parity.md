---
phase: FMP-5
title: Render Fixture Parity and Readback/Smoke Validation
depends_on: [FMP-4]
can_parallelize: false
---

# FMP-5 — Render Fixture Parity and Readback/Smoke Validation

## Goal

Prove Metal output is materially equivalent to Linux/OpenGL for supported BSP presentation features.

## Strategy

Use two layers:

1. Display-free fixture checks:
   - Import/render-level material records contain expected texture slots, factors, alpha modes, transforms, lightmaps, and tangents.
2. Opt-in Metal GPU/readback checks:
   - Render fixed test scenes.
   - Read back selected sample pixels or histograms.
   - Compare against stable expectations with tolerance.

## Tasks

1. Create or identify fixtures for base texture, lightmap, normal/specular sidecar, alpha mask, alpha blend, double-sided, emissive, and vertex color if available.
2. Add display-free tests asserting expected `RenderLevel` material bindings.
3. Add optional GPU readback:
   - Render deterministic scene.
   - Copy render target to `MTLBuffer`.
   - Validate sampled pixel regions.
4. Add equivalent OpenGL optional smoke if useful.
5. Store tolerance thresholds in code or manifest.

## Acceptance criteria

- Default tests catch missing material slot upload.
- Optional Metal readback catches broken shader paths.
- At least one lit BSP and one normal/specular sidecar fixture pass Metal display smoke.
- Visual parity limitations are documented with exact follow-up issues if any remain.

## Validation

```bash
ctest --test-dir build-macos-metal -R '^(render_level_upload|render_level_inspection|bsp_materials|bsp_lightmaps|render_parity)' --output-on-failure
STELLAR_RUN_METAL_CONTEXT_TESTS=1 ctest --test-dir build-macos-metal -R '^(metal_context_smoke|metal_render_readback)$' --output-on-failure
build-macos-metal/stellar-client --map <lit_fixture.bsp> --renderer metal
build-macos-metal/stellar-client --map <normal_specular_fixture.bsp> --renderer metal
```
