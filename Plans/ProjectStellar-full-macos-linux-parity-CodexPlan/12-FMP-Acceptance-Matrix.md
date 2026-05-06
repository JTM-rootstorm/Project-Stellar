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
| `stellar-client --validate-display` | `SKIP_EXPECTED` | `SKIP_EXPECTED` | `NOT_COVERED` | `NOT_COVERED` | Default display validation is opt-in. macOS OpenGL is unsupported/experimental. Metal display validation still needs a display-attached run. |
| `stellar-client --validate-map <fixture>` | `PASS` | `PASS` | `PASS` | `PASS` | Import/map validation is backend-neutral and covered by CLI/runtime smoke. |
| Single-player `stellar-client --map <fixture>` | `PASS` | `PASS` | `PASS` | `PASS` | Display-free runtime tests cover single-player simulation startup. Full display smoke is skipped without an attached display. |
| Listen-host mode | `PASS` | `PASS` | `PASS` | `PASS` | Display-free listen-host and loopback runtime tests cover the macOS socket path. |
| Remote client mode | `PASS` | `PASS` | `PASS` | `PASS` | Display-free client/server loopback runtime tests cover remote-client connection flow. |
| Dedicated server | `PASS` | `PASS` | `PASS` | `PASS` | Server targets remain backend-neutral and do not link graphics or miniaudio in the current CMake graph. |
| Generated footstep audio no-device path | `PASS` | `PASS` | `PASS` | `PASS` | Default audio tests use no-device/no-op behavior, decode generated WAV assets without opening a device, and cover presentation diagnostics. |
| Generated footstep audible smoke | `SKIP_EXPECTED` | `SKIP_EXPECTED` | `NOT_COVERED` | `NOT_COVERED` | Audible miniaudio playback is opt-in; the manual macOS smoke command is documented, but the current no-display session did not execute it. |
| TrenchBroom/BSP tooling | `PASS` | `PASS` | `PASS` | `PASS` | Shell syntax/docs checks pass. Optional external BSP compiler gaps skip with CTest return code `77`, and macOS TrenchBroom/VHLT notes are documented. |
| Renderer material fixtures | `PASS` | `SKIP_EXPECTED` | `PASS` | `PASS` | Metal now consumes the active OpenGL material contract in shader bindings; GPU readback parity remains tracked separately. |
| Optional Metal GPU/readback parity | `SKIP_EXPECTED` | `SKIP_EXPECTED` | `NOT_COVERED` | `NOT_COVERED` | Display-free fixture coverage is tracked in `13-FMP-Render-Fixture-Matrix.md`; no tracked readback/histogram test exists yet. |
| CI or preset build matrix | `PASS` | `PASS` | `PASS` | `PASS` | `CMakePresets.json` provides configure/build/test presets for each required matrix entry. |

## Active Blockers

- FMP-3 opt-in display smoke still needs a display-attached local run. The
  projection, viewport, drawable/depth diagnostics, and display-free tests are
  in place, but the current session has no SDL display.
- FMP-5 still needs opt-in Metal readback/smoke coverage. Display-free material
  fixture assertions and the fixture matrix are in place.
- FMP-6 added tracked display-free runtime smoke coverage for single-player,
  listen-host, remote-client, and dedicated-server workflows. Full Metal display
  smoke remains gated on an attached display.
- FMP-7 covered no-device diagnostics, decode-only generated WAV loading, and
  `STELLAR_MINIAUDIO_NO_RUNTIME_LINKING=ON` framework-link validation. Optional
  audible macOS miniaudio smoke still needs a display/audio-device session.
- FMP-8 covered shell/Python tooling on macOS and made optional external BSP
  compiler gaps skip clearly with exit code `77`.

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
- The FMP-0 audit found that `MaterialUpload` preserved full material data while
  Metal consumed only base color, vertex color, and base texture; FMP-4 updated
  the Metal shader path to consume the active material contract.
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

## FMP-3 Validation Notes

FMP-3 added a Metal projection correction from OpenGL-style clip depth to Metal
zero-to-one clip depth, explicit Metal viewport setup from SDL drawable pixels,
and `STELLAR_DEBUG_RENDER=1` diagnostics for backend projection convention,
drawable size, depth texture size, and viewport.

Local focused validation on 2026-05-06:

```bash
ctest --test-dir build-macos -R '^(render_level_inspection|render_level_upload|graphics_backend_selection)$' --output-on-failure
ctest --test-dir build-macos-metal -R '^(render_level_inspection|render_level_upload|graphics_backend_selection|metal_context_smoke)$' --output-on-failure
git diff --check
```

The display-free tests passed. `metal_context_smoke` skipped by default. The
opt-in display validation command:

```bash
STELLAR_DEBUG_RENDER=1 STELLAR_DEBUG_RENDER_FRAMES=1 build-macos-metal/stellar-client --validate-display --renderer metal
```

could not run in this session because SDL reported that the video driver did not
add any displays.

## FMP-4 Validation Notes

FMP-4 expanded the embedded Metal shader and binding path to consume the active
OpenGL material contract: base color and vertex color, base textures, lightmaps
on secondary UVs, normal maps with tangent basis, metallic/roughness, occlusion,
emissive, specular texture/factors, texture transforms, texcoord set selection,
alpha mask/blend, double-sided culling, unlit materials, camera-dependent
specular, and global/key light constants. Metal material debug logs list bound
slots for the first few materials when `STELLAR_DEBUG_RENDER=1`.

Local focused validation on 2026-05-06:

```bash
ctest --test-dir build-macos-metal -R '^(metal_shader_compile|render_level_upload|render_level_inspection|bsp_materials|bsp_lightmaps|graphics_backend_selection)$' --output-on-failure
ctest --test-dir build-macos-metal-only -R '^(metal_shader_compile|render_level_upload|render_level_inspection|bsp_materials|bsp_lightmaps|graphics_backend_selection)$' --output-on-failure
git diff --check
```

Both Metal builds passed the focused suite, including the display-free
`metal_shader_compile` test. Pixel/readback comparison remains FMP-5 work.

## FMP-5 Validation Notes

FMP-5 expanded the display-free sidecar material fixture to assert every active
material slot and factor that the Metal shader now consumes: normal, specular,
metallic/roughness, occlusion, emissive, lightmap, texture transforms, texcoord
set selection, alpha mask, double-sided, emissive factor, and scalar material
factors. `13-FMP-Render-Fixture-Matrix.md` records the fixture coverage and keeps
the optional GPU/readback gap explicit.

Local focused validation on 2026-05-06:

```bash
ctest --test-dir build-macos-metal -R '^(render_level_upload|render_level_inspection|metal_shader_compile|bsp_materials|bsp_lightmaps)$' --output-on-failure
git diff --check
```

The display-free suite passed. Metal pixel/readback validation remains
`NOT_COVERED` until a display/GPU-backed deterministic readback test is added.

## FMP-6 Validation Notes

FMP-6 added `tools/ci/run_macos_runtime_smoke.sh` as a local/CI-oriented runtime
smoke wrapper for macOS Metal builds. The script validates client renderer
configuration, map lookup from repository and build working directories, server
map configuration on `127.0.0.1:0`, the focused display-free runtime CTest
slice, and optional single-player display smoke when an attached display is
available.

Local focused validation on 2026-05-06:

```bash
ctest --test-dir build-macos-metal -R '^(loopback_transport|transport_loopback|transport_socket|socket_transport|network_session|server_runtime|dedicated_server|listen_server_host|client_single_player_runtime|client_connect|client_world_receiver|client_map_validation_smoke|client_cli_validate_map)$' --output-on-failure
STELLAR_SKIP_DISPLAY_SMOKE=1 tools/ci/run_macos_runtime_smoke.sh
```

The display-free runtime paths passed. The full Metal single-player display
smoke remains skipped in the current no-display session.

## FMP-7 Validation Notes

FMP-7 added device-free miniaudio decode coverage for generated footstep WAV
assets and tightened presentation diagnostics around missing assets, unknown
sound ids, and uninitialized sinks. The optional audible smoke path is
documented in `08-Phase-FMP7-Audio-Parity.md` and remains manual because it
requires a display-attached macOS session and an available audio output device.

Local focused validation on 2026-05-06:

```bash
cmake --build build --target stellar_miniaudio_sink_test stellar_generated_footstep_assets_test stellar_audio_event_router_test stellar_footstep_audio_pipeline_test
ctest --test-dir build -R '^(audio_event_router|miniaudio_sink|generated_footstep_assets|footstep_audio_pipeline)$' --output-on-failure
cmake --build build-macos-metal --target stellar_miniaudio_sink_test stellar_generated_footstep_assets_test stellar_audio_event_router_test stellar_footstep_audio_pipeline_test
ctest --test-dir build-macos-metal -R '^(audio_event_router|miniaudio_sink|generated_footstep_assets|footstep_audio_pipeline)$' --output-on-failure
cmake --build build-macos-metal-only --target stellar_miniaudio_sink_test stellar_generated_footstep_assets_test stellar_audio_event_router_test stellar_footstep_audio_pipeline_test
ctest --test-dir build-macos-metal-only -R '^(audio_event_router|miniaudio_sink|generated_footstep_assets|footstep_audio_pipeline)$' --output-on-failure
cmake -S . -B build-macos-audio-frameworks -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_MINIAUDIO_NO_RUNTIME_LINKING=ON
cmake --build build-macos-audio-frameworks --target stellar_miniaudio_sink_test stellar-client stellar-server
ctest --test-dir build-macos-audio-frameworks -R '^miniaudio_sink$' --output-on-failure
otool -L build-macos-audio-frameworks/stellar-client
otool -L build-macos-audio-frameworks/stellar-server
```

The default, macOS Metal, and macOS Metal-only audio slices passed. The
framework-link build configured and built; `stellar-client` links CoreFoundation,
CoreAudio, and AudioToolbox when `STELLAR_MINIAUDIO_NO_RUNTIME_LINKING=ON`,
while `stellar-server` links only libc++ and libSystem in that build tree.

## FMP-8 Validation Notes

FMP-8 made BSP/TrenchBroom tooling skip optional external compilers with return
code `77` when host-native tools are unavailable or incompatible, avoids
selecting repository-local Linux ELF VHLT binaries on macOS, and documents macOS
TrenchBroom package setup plus Homebrew dependency notes.

Local focused validation on 2026-05-06:

```bash
bash -n tools/bsp/compile_trenchbroom_bsp30.sh
bash -n tools/bsp/compile_vhlt_bsp30.sh
bash -n tools/bsp/validate_trenchbroom_bsp30.sh
bash -n tools/bsp/run_vhlt_fixture_matrix.sh
bash -n tools/trenchbroom/Stellar/bin/stellar_tb_compile.sh
bash -n tools/trenchbroom/Stellar/bin/stellar_tb_validate.sh
python3 tools/docs/check_docs_consistency.py
ctest --test-dir build-macos-metal -R '^(trenchbroom|bsp_)' --output-on-failure
```

The focused tooling/import slice passed with optional external compiler coverage
reported as skipped where host-native tools were unavailable.
