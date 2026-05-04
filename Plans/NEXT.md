# Stellar Engine - Next Scope Handoff

Status scope: completed client/server split handoff and completed historical scope guardrails.

## Current Entry Point

`docs/ImplementationStatus.md` is the source of truth for branch status. Client/server decoupling is
complete through Phase CS-9 as of 2026-05-03.

Completed plan/proposal files:

- `Plans/ClientServerSplit-AgentPlan.md`
- `Plans/ProjectStellar-ClientServerDecoupling-AgentPlan.md`

Focused architecture doc:

- `docs/ClientServerArchitecture.md`

## Completed Client/Server Split

The branch now separates protocol, transport, authority, server runtime, dedicated-server,
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

## Follow-Up Options After Decoupling

1. Presentation-map workflow for remote clients, explicitly separate from authority map loading and
   gameplay script execution.
2. Client interpolation/presentation smoothing over authoritative snapshots.
3. Client prediction/reconciliation, reconciled against server authority.
4. True multiplayer simulation beyond the current one accepted TCP client / one active player limit.
5. UDP/unreliable transport and transport selection.
6. Map transfer/caching after presentation-map ownership is defined.
7. Build/docs follow-ups: keep `docs/ClientServerArchitecture.md`, `docs/Design.md`, and
   `docs/ImplementationStatus.md` aligned when target names or runtime contracts change.
8. Richer presentation systems: sprite animation, HUD/inventory/VFX, miniaudio-backed playback, and
   local presentation asset workflows.

## Historical Completed Scope Guardrails

Completed plan packages remain archived and must not be restarted:

- BSP gameplay loop: `Plans/Archived/bsp_gameplay_loop/`.
- BSP presentation/networking polish: `Plans/Archived/bsp_presentation_networking_polish/`.
- Socket transport: `Plans/Archived/socket_transport/`.
- TrenchBroom BSP30 compatibility and Z-up migration: `Plans/Archived/trenchbroom_compat/`.

The completed socket transport and networked session lifecycle work is retained as implementation
context only. The branch already has `stellar-server`, `stellar-client --connect HOST:PORT`,
Linux/POSIX TCP `SocketTransport`, `ClientHello`, `ServerWelcome`, `ClientWorldReceiver`, and
`NetworkWorldSnapshot` presentation. Do not restart socket transport/session lifecycle work.

The completed TrenchBroom BSP30 compatibility work is retained as implementation context only. The
branch already has the project-owned Stellar TrenchBroom package, BSP30 compile/validation wrappers,
Z-up authoring/runtime conventions, fixture coverage, lightmap support, and server-authoritative
`func_door`/`func_button` support. Do not restart TrenchBroom compatibility or Z-up migration work.

Do not restart completed collision, movement, Lua scripting, object-collider, BSP migration, BSP
hardening, BSP gameplay-loop foundation work, BSP presentation/networking polish work, socket
transport/session lifecycle work, TrenchBroom compatibility work, or the completed client/server split.

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
- OpenGL/Vulkan remain runtime-selectable through the shared graphics abstraction.
- Rendering, audio, HUD, and UI are presentation only and never sources of gameplay truth.
- No client prediction, interpolation, map transfer, reconciliation, UDP/unreliable transport,
  authentication, encryption, matchmaking, or public Internet deployment is active unless a future plan
  explicitly scopes it.
- Do not add Source/VBSP, dynamic rigid bodies, moving brush classes beyond the implemented
  `func_door`/`func_button` path, full PBR, client-side gameplay scripting, model/animation systems,
  third-party physics, arbitrary non-Stellar entity parity, or retired importer functionality unless
  explicitly requested.

## Validation Runbook

Final validation for post-split changes should prefer:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(protocol|transport|network_session|socket_transport|server_runtime|dedicated_server|listen_server_host|client_single_player_runtime|client_connect|client_map_validation_smoke|client_cli_map_validation|client_world_receiver|gameplay_presentation|player_presentation|hud_presentation|audio_event_router|bsp_|runtime_world|server_world_session|scripted_world_session)' --output-on-failure
tools/dev/check_target_boundaries.sh
```

CS-8 committed validation from `8dce477c757e293fdb6f39cbca81b809b202b7e8` passed configure,
selected target builds, CTest regex 10/10 after protocol/transport aliases, and the target-boundary
script.

CS-9 final handoff validation on 2026-05-03 passed configure, full debug build, full default CTest
97/97, focused client/server CTest 44/44, `tools/dev/check_target_boundaries.sh`, and the final
source audits for legacy local loopback/client-owned authority references.
