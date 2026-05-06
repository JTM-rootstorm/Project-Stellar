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
| Configure/build | `PASS` | `PASS` | `PASS` | `NOT_COVERED` | Existing docs report passing default and Metal builds. Metal-only appears CMake-gated away from OpenGL/GLEW, but needs tracked validation. |
| Default CTest | `PASS` | `PASS` | `PASS` | `NOT_COVERED` | Existing docs report 102/102 for default and Metal builds. Metal-only CTest is not yet captured. |
| `stellar-client --validate-config` | `PASS` | `PASS` | `PASS` | `PASS` | Config validation does not require display creation. Metal-only still requires explicit renderer selection for render use. |
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
| CI or preset build matrix | `FAIL` | `FAIL` | `FAIL` | `FAIL` | No tracked `.github` workflow, `CMakePresets.json`, or equivalent matrix script exists yet. |

## Active Blockers

- FMP-1 must add tracked build matrix commands or presets and validate the
  `macos-metal-only` build/test path.
- FMP-1 should harden target-boundary checks so server/authority targets also
  reject `stellar_audio_miniaudio` and raw `miniaudio`, not only `stellar_audio`.
- FMP-2 must add deterministic backend availability/default helpers. OpenGL is
  still the default even in OpenGL-disabled/Metal-enabled builds.
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
