---
phase: FMP-5.1
title: Metal Display Readback & Pixel Parity
depends_on: [FMP-4]  # assume previous material/shader parity is complete
can_parallelize: false
---

# FMP-5.1 — Metal Display Readback & Pixel Parity

## Goal

Prove that Metal renders match OpenGL outputs per material slot using display-attached readback.

## Files likely changed / used

```text
src/graphics/metal/MetalGraphicsDevice.mm
src/graphics/RenderLevel.cpp
tests/graphics/RenderSceneInspection.cpp
tests/graphics/MetalRenderReadback.cpp  # new
tests/fixtures/lit_fixture.bsp
tests/fixtures/normal_specular_fixture.bsp
docs/ImplementationStatus.md
```
## Tasks

1. **Set up display-attached Metal test context**
   - Create `SDL_Window*` with `SDL_WINDOW_METAL | SDL_WINDOW_ALLOW_HIGHDPI`
   - Attach `MTLDevice` and `CAMetalLayer` via `SDL_Metal_CreateView`
   - Ensure drawable size matches Retina/HiDPI if present
2. **Render test scenes**
   - `lit_fixture.bsp` for base/lightmap check
   - `normal_specular_fixture.bsp` for normal/specular/emissive check
   - Render with all relevant material slots bound
3. **Read back frame buffer**
   - Copy drawable content to `MTLBuffer`
   - Convert RGBA to CPU-accessible pixel array
   - Compute histograms per channel and per material slot
4. **Compare against reference / tolerance**
   - Compare to Linux/OpenGL expected results
   - Define acceptable delta (e.g., ±2% per channel) for minor rounding differences
5. **Log and report**
   - Per-frame and per-slot pass/fail
   - Debug logs of viewport size, projection matrix, depth texture size
   - Output summary CSV or JSON for automated agent consumption
6. **Skip conditions**
   - No attached display -> skip with return code 77
   - Unsupported Metal device -> skip with return code 77

## Acceptance criteria

- Every material slot renders in Metal and matches OpenGL within tolerance
- No crash, hang, or invalid drawable errors
- Logs include all viewport/projection/debug diagnostics
- Test can be re-run for multiple fixtures consistently

## Validation

```bash
STELLAR_RUN_METAL_CONTEXT_TESTS=1 build-macos-metal/stellar-client --map tests/fixtures/lit_fixture.bsp --renderer metal --validate-display --readback-output /tmp/metal_readback_lit.json
STELLAR_RUN_METAL_CONTEXT_TESTS=1 build-macos-metal/stellar-client --map tests/fixtures/normal_specular_fixture.bsp --renderer metal --validate-display --readback-output /tmp/metal_readback_specular.json

# Optional comparison script
python3 tools/graphics/compare_readback.py /tmp/metal_readback_lit.json /tmp/reference_linux_lit.json --tolerance 0.02
python3 tools/graphics/compare_readback.py /tmp/metal_readback_specular.json /tmp/reference_linux_specular.json --tolerance 0.02
```
