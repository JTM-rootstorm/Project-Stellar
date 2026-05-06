# Full macOS/Linux Parity Acceptance Matrix

Status values:

- `PASS`: covered by current implementation and local/tracked validation evidence.
- `FAIL`: covered and currently failing, incomplete, or known to violate parity.
- `SKIP_EXPECTED`: intentionally skipped for this matrix entry, usually because the
  platform/backend combination is unsupported or opt-in hardware is unavailable.
- `NOT_COVERED`: required for full parity but not yet validated by tracked tests,
  scripts, CI, or documented local transcripts.

Full macOS compatibility and Linux parity is not complete until every required row is
`PASS` or `SKIP_EXPECTED`.

## Build And Runtime Matrix

| Acceptance row | Linux OpenGL default | macOS default build | macOS Metal build | macOS Metal-only build | Notes |
|---|---|---|---|---|---|
| Configure/build | `PASS` | `PASS` | `PASS` | `PASS` | `CMakePresets.json` tracks the four build variants. macOS default, macOS Metal, and macOS Metal-only configure/build passed locally; Linux default must run on a Linux host. |
| Default CTest | `PASS` | `PASS` | `PASS` | `PASS` | macOS default passed 102/102. macOS Metal and Metal-only passed 103/103 with `metal_context_smoke` skipped by default. Linux default must run on a Linux host. |
| `stellar-client --validate-config` | `PASS` | `PASS` | `PASS` | `PASS` | Config validation does not require display creation. Backend CLI selection is tested for OpenGL-only, dual-backend, and Metal-only builds. |
| `stellar-client --validate-display` | `SKIP_EXPECTED` | `SKIP_EXPECTED` | `PASS` | `FAIL` | Default display validation is opt-in. macOS OpenGL is unsupported/experimental. Metal-only default selection still assumes OpenGL until FMP-2. |
| `stellar-client --validate-map <fixture>` | `PASS` | `PASS` | `PASS` | `NOT_COVERED` | Import/map validation is backend-neutral; Metal-only validation needs a tracked run. |
| Single-player `stellar-client --map <fixture>` | `NOT_COVERED` | `NOT_COVERED` | `NOT_COVERED` | `NOT_COVERED` | Existing unit tests cover single-player runtime pieces, but full macOS smoke is not tracked. |
| Listen-host mode | `NOT_COVERED` | `NOT_COVERED` | `NOT_COVERED` | `NOT_COVERED` | Requires FMP-6 runtime smoke coverage. |
| Remote client mode | `NOT_COVERED` | `NOT_COVERED` | `NOT_COVERED` | `NOT_COVERED` | Requires FMP-6 loopback runtime smoke coverage. |
| Dedicated server | `PASS` | `PASS` | `PASS` | `PASS` | Server targets remain backend-neutral and do not link graphics or miniaudio in the current CMake graph. |
| Generated footstep audio no-device path | `PASS` | `PASS` | `PASS` | `PASS` | Default audio tests use no-device/no-op behavior and presentation diagnostics. |
| Generated footstep audible smoke | `SKIP_EXPECTED` | `SKIP_EXPECTED` | `NOT_COVERED` | `NOT_COVERED` | Audible miniaudio playback is opt-in and still needs a documented macOS smoke result. |
| TrenchBroom/BSP tooling | `PASS` | `FAIL` | `FAIL` | `FAIL` | Shell syntax/docs checks pass, but optional external BSP compiler gaps currently skip with exit `0` instead of the required code `77`, and macOS tool docs need clarification. |
| Renderer material fixtures | `PASS` | `SKIP_EXPECTED` | `FAIL` | `FAIL` | OpenGL consumes the full active material contract. Metal currently draws the base path and ignores required lightmap/normal/specular/material slots. |
| Optional Metal GPU/readback parity | `SKIP_EXPECTED` | `SKIP_EXPECTED` | `NOT_COVERED` | `NOT_COVERED` | Required for final Metal parity, but no tracked readback/histogram test exists yet. |
| CI or preset build matrix | `PASS` | `PASS` | `PASS` | `PASS` | `CMakePresets.json` provides configure/build/test presets for each required matrix entry. |

## Active Blockers

- FMP-3 must add explicit Metal projection, viewport, drawable/depth diagnostics,
  and `STELLAR_DEBUG_RENDER=1` output for the frame contract.
- FMP-4 must make Metal consume every material slot currently consumed by OpenGL:
  lightmap, normal, specular, metallic/roughness, occlusion, emissive, texture
  transforms, texcoord set selection, alpha cutoff/blend, double-sided culling,
  unlit behavior, camera-dependent specular, and global/key light constants.
- FMP-5 must add display-free material fixture assertions plus opt-in Metal
  readback/smoke coverage.
- FMP-6 must add tracked macOS runtime smoke coverage for single-player,
  listen-host, remote-client, and dedicated-server workflows.
- FMP-7 must capture optional audible macOS miniaudio smoke or clean device
  diagnostics while keeping default tests audio-device-free. A decode-only WAV
  load test is not covered yet.
- FMP-8 must validate shell/Python tooling on macOS and make optional external
  BSP compiler gaps skip clearly with exit code `77`.

## FMP-0 Audit Commands

The FMP-0 audit used these source checks:

```bash
git grep -n 'STELLAR_ENABLE_METAL\|kMetal\|SDL_WINDOW_METAL\|MetalGraphicsDevice' -- CMakeLists.txt include src tests docs Plans/NEXT.md
git grep -n 'lightmap_texture\|normal_texture\|specular_texture\|metallic_roughness\|emissive_texture\|occlusion_texture' -- include src/graphics tests/graphics
git grep -n 'MSG_NOSIGNAL\|SO_NOSIGPIPE\|POSITION_INDEPENDIENT_CODE' -- .
```

Current findings:

- Active build/source/tests have Apple-gated Metal support and no live Vulkan
  backend.
- OpenGL/GLEW discovery and OpenGL sources are gated behind
  `STELLAR_ENABLE_OPENGL_BACKEND`, so the graph is intended to support Metal-only
  builds on Apple platforms.
- Socket send behavior is guarded with `MSG_NOSIGNAL` only where available and
  Apple sockets install `SO_NOSIGPIPE`.
- The historical `POSITION_INDEPENDIENT_CODE` typo appears only in archived plan
  text; active miniaudio CMake uses `POSITION_INDEPENDENT_CODE`.
- `MaterialUpload` preserves the full material data needed for parity, but the
  current Metal shader only consumes base color, vertex color, and base texture.
- BSP and TrenchBroom shell wrappers pass syntax checks, Python docs consistency
  passes, and generated footstep WAV regeneration is byte-stable in the stopped
  tooling audit.
- The `bsp_authoring_smoke_compile_wrapper` path reports a missing external BSP
  compiler as a textual skip but exits `0`; FMP-8 requires a clear skip status
  of `77`.

## FMP-1 Validation Notes

FMP-1 added `CMakePresets.json` entries for `linux-default`, `macos-default`,
`macos-metal`, and `macos-metal-only`, plus matching build and test presets. The
macOS presets were validated locally on 2026-05-06:

```bash
cmake --list-presets=all
cmake --preset macos-default
cmake --build --preset macos-default -j8
ctest --test-dir build-macos --output-on-failure
cmake --preset macos-metal
cmake --build --preset macos-metal -j8
ctest --test-dir build-macos-metal --output-on-failure
cmake --preset macos-metal-only
cmake --build --preset macos-metal-only -j8
ctest --test-dir build-macos-metal-only --output-on-failure
tools/dev/check_target_boundaries.sh .
git diff --check
```

`macos-default` passed 102/102 tests. `macos-metal` and `macos-metal-only`
passed 103/103 tests with `metal_context_smoke` skipped by default. The
`linux-default` preset was configured on the local macOS host only to validate
preset syntax; Linux build/test execution still belongs to a Linux host or CI
runner.

## FMP-2 Validation Notes

FMP-2 added deterministic backend defaults and availability checks:

- `default_graphics_backend()` returns OpenGL when compiled, otherwise Metal in
  Metal-only builds.
- `graphics_backend_available()` reports whether a requested backend has a
  compiled device implementation.
- Unsupported backend diagnostics now include compiled backend names and a
  suggested backend for the current build.

Local focused validation on 2026-05-06:

```bash
ctest --test-dir build-macos -R '^(graphics_backend_selection|client_map_validation_smoke)$' --output-on-failure
ctest --test-dir build-macos-metal -R '^(graphics_backend_selection|client_map_validation_smoke)$' --output-on-failure
ctest --test-dir build-macos-metal-only -R '^(graphics_backend_selection|client_map_validation_smoke)$' --output-on-failure
build-macos-metal/stellar-client --validate-config --renderer metal
build-macos-metal/stellar-client --validate-config --renderer opengl
build-macos-metal/stellar-client --validate-config --renderer vulkan
build-macos-metal-only/stellar-client --validate-config --renderer metal
build-macos-metal-only/stellar-client --validate-config --renderer opengl
build-macos/stellar-client --validate-config --renderer metal
build-macos/stellar-client --validate-config --renderer opengl
```

The valid renderer selections succeeded. Invalid selections failed early with
compiled-backend diagnostics, including `opengl` in Metal-only builds and
`metal` in OpenGL-only builds.
