# Stellar Engine - Next Scope Handoff

Status scope: active Linux-only GL-to-Vulkan migration on `GL-to-vulkan`, completed full
macOS/Linux parity validation, completed macOS compatibility and Metal backend implementation,
completed audio footsteps implementation, completed Vulkan removal, completed client/server split
handoff, and completed historical scope guardrails.

## Current Entry Point

`docs/ImplementationStatus.md` is the source of truth for branch status. The active implementation
slice is the Linux-only GL-to-Vulkan migration tracked by
`Plans/ProjectStellar-GL-to-Vulkan-LinuxOnly-CodexPlan/00-MASTER-GLToVulkanLinuxOnly-CodexPlan.md`.
VK-3 Linux Vulkan device scaffold is complete on `GL-to-vulkan` as of 2026-05-06, and the next
phase is VK-4 SPIR-V shader pipeline.

Historical status remains relevant but is no longer the active branch objective: full macOS/Linux
parity validation is complete on `macos-compat` as of 2026-05-06, the earlier macOS compatibility
and Metal backend slice is complete through MC-8 on `macos-compat` as of 2026-05-05, Vulkan removal
is complete through KV-5 as of 2026-05-04, and client/server decoupling is complete through Phase
CS-9 as of 2026-05-03. This branch intentionally reintroduces Vulkan for Linux only while preserving
macOS Metal and excluding macOS Vulkan/MoltenVK.

## Completed macOS Compatibility And Metal Backend Scope

Completed phase checklist:

- [x] MC-0 - Baseline, guardrails, and branch truth.
- [x] MC-1 - macOS build and toolchain hygiene.
- [x] MC-2 - macOS POSIX socket portability.
- [x] MC-3 - macOS audio runtime sink.
- [x] MC-4 - OpenGL macOS fallback diagnostics.
- [x] MC-5 - Metal backend scaffold.
- [x] MC-6 - Metal resource upload and frame lifecycle.
- [x] MC-7 - Initial Metal shader/material path.
- [x] MC-8 - Documentation, validation, and handoff.

Current renderer contract:

- OpenGL remains the default renderer where it builds.
- macOS OpenGL is deprecated and treated as unsupported/experimental; diagnostics point developers
  toward Metal.
- Metal is compiled only on Apple platforms with `STELLAR_ENABLE_METAL=ON`.
- Metal parser aliases `metal` and `mtl` exist only in Metal-enabled builds; default builds reject
  them with a clear unsupported-backend diagnostic.
- The Metal backend creates a real SDL Metal view, CAMetalLayer, MTLDevice, command queue, depth
  target, mesh buffers, RGB/RGBA textures, material records, and a shader path that consumes the
  active OpenGL material contract, including lightmaps, normal/specular maps, metallic/roughness,
  occlusion, emissive, texture transforms, alpha behavior, culling, unlit behavior, and
  camera-dependent specular.
- Full parity validation is closed: display-attached Metal smoke/readback coverage passed, optional
  audible miniaudio smoke passed on Metal and Metal-only macOS builds, and the Linux `linux-default`
  preset passed configure, build, and CTest 103/103 on a Linux host on 2026-05-06.

macOS runbook:

```bash
brew install cmake sdl2 glew glm
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure

cmake -S . -B build-macos-metal -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_METAL=ON
cmake --build build-macos-metal -j$(sysctl -n hw.ncpu)
ctest --test-dir build-macos-metal --output-on-failure
build-macos-metal/stellar-client --validate-display --renderer metal
```

Opt-in audible audio smoke:

```bash
STELLAR_ENABLE_AUDIO=1 STELLAR_AUDIO_SMOKE_CONFIRM=heard \
  build-macos-metal/stellar-audio-smoke \
  --sound footstep_concrete_0 \
  --sound footstep_metal_1 \
  --duration-ms 2500
```

## Completed Audio Footsteps Scope

Current objective: derive a tiny server-safe footstep surface id from BSP source texture/material
names, emit deterministic server-approved footstep gameplay events from authoritative movement, and
route those events to generated retro one-shot presentation sounds.

Active phase checklist:

- [x] AF-0 - Branch/docs guardrails.
- [x] AF-1 - Collision surface identity and footstep surface resolver.
- [x] AF-2 - Authoritative footstep cadence and `GameplayEventKind::kFootstep`.
- [x] AF-3 - Presentation audio routing and generated retro footstep sounds.
- [x] AF-4 - End-to-end display-free hardening and documentation.

Default validation remains display-free. Generated retro WAV one-shots are acceptable placeholder
assets for this slice. Footsteps remain server-approved presentation events; no renderer, HUD, audio
sink, or client-side prediction path becomes gameplay truth.

Non-goals:

- No full material gameplay system.
- No client-side footstep authority.
- No prediction/reconciliation.
- No broad spatial audio engine.
- No dynamic terrain/material decals.

Completed plan/proposal files:

- `Plans/AudioFootsteps-AgentPlan.md`
- `Plans/audio_footsteps_plan/00-MASTER-AudioFootsteps-AgentPlan.md`

Historical completed plan/proposal files:

- `Plans/Archived/kill_vulkan/00-MASTER-KillVulkan-Codex-AgentPlan.md`
- `Plans/ClientServerSplit-AgentPlan.md`
- `Plans/ProjectStellar-ClientServerDecoupling-AgentPlan.md`

Focused architecture doc:

- `docs/ClientServerArchitecture.md`

## Completed Vulkan Removal

The branch removed Vulkan from active build, runtime, tests, and support documentation while retaining
the reusable graphics interfaces needed for future renderer backends.

Completed Vulkan-removal outcomes:

- Root CMake no longer calls `find_package(Vulkan)`, references Vulkan include directories, links
  `Vulkan::Vulkan`, or lists Vulkan backend source files.
- Test CMake no longer exposes `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS` or builds the Vulkan context
  smoke test.
- `GraphicsBackend` no longer has `kVulkan`; parser aliases such as `vulkan` and `vk` must fail as
  unsupported.
- The graphics device factory creates OpenGL devices only.
- Client window creation uses OpenGL window flags only.
- Vulkan-specific projection conversion was removed from the level renderer.
- Active Vulkan backend implementation files and active Vulkan smoke tests were deleted.
- Active docs now describe OpenGL as the current renderer and DirectX/Metal as future possibilities
  only.

Useful Vulkan-removal audits:

```bash
git grep -n -i \
  -e 'vulkan' \
  -e 'VK_' \
  -e 'Vk[A-Z]' \
  -e 'SDL_WINDOW_VULKAN' \
  -e 'STELLAR_ENABLE_VULKAN' \
  -e 'Vulkan::Vulkan' \
  -e 'Vulkan_INCLUDE_DIRS' \
  -- include src tests CMakeLists.txt tests/cmake docs README.md Plans/NEXT.md ':!Plans/Archived/**'

git ls-files | grep -Ei '(^|/)vulkan|Vulkan' | grep -v '^Plans/Archived/' || true
```

Expected result: no active code/build/test references remain. Active docs may mention Vulkan only as
historical removal status or as a deliberately rejected backend.

## Completed Client/Server Split

The branch separates protocol, transport, authority, server runtime, dedicated-server,
single-player, remote-client, listen-server, presentation, and client application composition modules.
This completed the CS-0 through CS-9 sequence without restarting the completed socket/session lifecycle
or TrenchBroom BSP30 compatibility work.

Runtime modes:

| Mode | Command | Authority Owner | Socket Listener | Script Loading | Lifetime |
|---|---|---|---:|---:|---|
| Single-player | `stellar-client --map path/to/map.bsp` | In-process single-player authority facade | no | authority-side only | client |
| Remote client | `stellar-client --connect HOST:PORT` | remote server/listen host | no | no | client |
| Listen server | `stellar-client --host --map path/to/map.bsp [--listen HOST:PORT]` | in-process server host | yes when listening | authority-side only | host client |
| Dedicated server | `stellar-server --map path/to/map.bsp --listen HOST:PORT` | server process | yes | authority-side only | server process |

Completed phase checklist:

- [x] CS-0 - Architecture baseline and guardrails.
- [x] CS-1 - Extract protocol DTOs away from server/scripting implementation types.
- [x] CS-2 - Extract shared authority bootstrap.
- [x] CS-3 - Move `LocalServerBridge` out of the client and generalize it as server runtime.
- [x] CS-4 - Introduce a single client-facing runtime interface.
- [x] CS-5 - Add true single-player runtime without server startup.
- [x] CS-6 - Add listen-server host mode.
- [x] CS-7 - Remove legacy local loopback runtime and client-owned authority leftovers.
- [x] CS-8 - Build graph enforcement.
- [x] CS-9 - Documentation update and final handoff.

## Preserved Boundary Guardrails

- Protocol must not link server, scripting, client runtime, graphics, audio, or platform presentation targets.
- Transport must depend only on protocol plus low-level platform/system support and must not depend on server runtime, scripting, or client presentation.
- Remote client runtime must not link authority, server runtime/core, or scripting.
- Dedicated server must not link client runtime or presentation.
- Authority/server-side script loading remains sandboxed and authoritative-side only.
- Broad follow-ups belong in plans/status docs, not scattered source TODO comments.

Useful final audit commands:

```bash
git grep -n 'LocalServerBridge\|LocalLoopbackRuntime' -- include src tests ':!Plans/Archived/**'
git grep -n 'stellar/server/WorldSession.hpp\|stellar/scripting/ScriptedWorldSession.hpp' -- include/stellar/network src/network include/stellar/client src/client ':!Plans/Archived/**'
tools/dev/check_target_boundaries.sh
```

Expected references to authority/server/scripting types can remain in single-player, listen-host,
dedicated-server, server-runtime, authority, or top-level application composition paths. They must not
reintroduce authority links into `stellar_client_net`; CMake direct-link assertions and
`tools/dev/check_target_boundaries.sh` enforce that isolation.

## Follow-Up Options After Current Completed Scope

1. Presentation-map workflow for remote clients, explicitly separate from authority map loading and
   gameplay script execution.
2. Client interpolation/presentation smoothing over authoritative snapshots.
3. Client prediction/reconciliation, reconciled against server authority.
4. True multiplayer simulation beyond the current one accepted TCP client / one active player limit.
5. UDP/unreliable transport and transport selection.
6. Map transfer/caching after presentation-map ownership is defined.
7. Future renderer backend work for Windows/macOS, explicitly planned as DirectX/Direct3D or Metal
   implementation work through the existing graphics abstraction.
8. Build/docs follow-ups: keep `docs/ClientServerArchitecture.md`, `docs/Design.md`, and
   `docs/ImplementationStatus.md` aligned when target names or runtime contracts change.
9. Richer presentation systems: sprite animation, HUD/inventory/VFX, miniaudio-backed playback, and
   local presentation asset workflows.

## Historical Completed Scope Guardrails

Completed plan packages remain archived and must not be restarted:

- Vulkan removal: `Plans/Archived/kill_vulkan/`.
- BSP gameplay loop: `Plans/Archived/bsp_gameplay_loop/`.
- BSP presentation/networking polish: `Plans/Archived/bsp_presentation_networking_polish/`.
- Socket transport: `Plans/Archived/socket_transport/`.
- TrenchBroom BSP30 compatibility and Z-up migration: `Plans/Archived/trenchbroom_compat/`.

The completed Vulkan-removal work is retained as implementation context only. Do not reintroduce
Vulkan aliases, enum values, source files, CMake package requirements, or tests unless a future plan
explicitly reverses the renderer direction.

The completed socket transport and networked session lifecycle work is retained as implementation
context only. The branch already has `stellar-server`, `stellar-client --connect HOST:PORT`,
Linux/POSIX TCP `SocketTransport`, `ClientHello`, `ServerWelcome`, `ClientWorldReceiver`, and
`NetworkWorldSnapshot` presentation. Do not restart socket transport/session lifecycle work.

The completed TrenchBroom BSP30 compatibility work is retained as implementation context only. The
branch already has the project-owned Stellar TrenchBroom package, BSP30 compile/validation wrappers,
Z-up authoring/runtime conventions, fixture coverage, lightmap support, and server-authoritative
`func_door`/`func_button` support. Do not restart TrenchBroom compatibility or Z-up migration work.

Do not restart completed Vulkan removal, collision, movement, Lua scripting, object-collider, BSP
migration, BSP hardening, BSP gameplay-loop foundation work, BSP presentation/networking polish work,
socket transport/session lifecycle work, TrenchBroom compatibility work, or the completed
client/server split.

## Invariants

- BSP remains the canonical playable level format.
- The active world-axis convention is Z-up.
- TrenchBroom authoring targets BSP30, with 1 editor unit = 1 gameplay inch.
- Default player spawn centers are authored 36 inches above the floor for the default capsule.
- Server authority remains mandatory for networked, listen-server, and dedicated-server play.
- Single-player runs authoritative simulation in-process without starting a network server or listen socket.
- Lua scripting remains mandatory, sandboxed, and authoritative-side only.
- Remote client mode must not load gameplay scripts, construct `WorldSession`, construct
  `ScriptedWorldSession`, or own authoritative runtime state.
- Import never executes scripts.
- Runtime collision, movement, triggers, object colliders, scripting, and networking contracts remain backend-neutral.
- Default tests remain display-free.
- OpenGL is the default renderer through the shared graphics abstraction. Metal is an Apple-gated
  experimental backend behind `STELLAR_ENABLE_METAL=ON`; full Metal normal/specular/lightmap parity
  remains follow-up work.
- Rendering, audio, HUD, and UI are presentation only and never sources of gameplay truth.
- No client prediction, interpolation, map transfer, reconciliation, UDP/unreliable transport,
  authentication, encryption, matchmaking, or public Internet deployment is active unless a future plan
  explicitly scopes it.
- Do not add Vulkan, Source/VBSP, dynamic rigid bodies, moving brush classes beyond the implemented
  `func_door`/`func_button` path, full PBR, client-side gameplay scripting, model/animation systems,
  third-party physics, arbitrary non-Stellar entity parity, or retired importer functionality unless
  explicitly requested.

## Validation Runbook

Final validation for post-removal/post-split changes should prefer:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(graphics_backend_selection|render_level_upload|render_level_inspection|target_boundary|protocol|transport|network_session|socket_transport|server_runtime|dedicated_server|listen_server_host|client_single_player_runtime|client_connect|client_map_validation_smoke|client_cli_map_validation|client_world_receiver|gameplay_presentation|player_presentation|hud_presentation|audio_event_router|bsp_|runtime_world|server_world_session|scripted_world_session)' --output-on-failure
tools/dev/check_target_boundaries.sh
```

For renderer-support changes, also run the active Vulkan-removal audits shown above and confirm that
`stellar-client --renderer vulkan --validate-config` and `stellar-client --graphics-backend vk --validate-config`
fail early with unsupported-backend errors.

For Metal changes on macOS, also run:

```bash
cmake -S . -B build-macos-metal -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_METAL=ON
cmake --build build-macos-metal -j$(sysctl -n hw.ncpu)
ctest --test-dir build-macos-metal --output-on-failure
STELLAR_RUN_METAL_CONTEXT_TESTS=1 \
  ctest --test-dir build-macos-metal -R '^metal_context_smoke$' --output-on-failure
build-macos-metal/stellar-client --validate-display --renderer metal
```

CS-8 committed validation from `8dce477c757e293fdb6f39cbca81b809b202b7e8` passed configure,
selected target builds, CTest regex 10/10 after protocol/transport aliases, and the target-boundary
script.

CS-9 final handoff validation on 2026-05-03 passed configure, full debug build, full default CTest
97/97, focused client/server CTest 44/44, `tools/dev/check_target_boundaries.sh`, and the final
source audits for legacy local loopback/client-owned authority references.
