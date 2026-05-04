# Stellar Engine - Next Scope Handoff

Status scope: active client/server decoupling handoff and completed historical scope guardrails.

## Current Entry Point

`docs/ImplementationStatus.md` is the source of truth for branch status. The active scope is client/server decoupling from the Phase CS plan:

- `Plans/ClientServerSplit-AgentPlan.md`

The proposal source is preserved at:

- `Plans/ProjectStellar-ClientServerDecoupling-AgentPlan.md`

## Active Focus - Client/Server Decoupling

The next implementation sequence is Phase CS-0 through CS-9 from `Plans/ClientServerSplit-AgentPlan.md`. The current branch should decouple presentation, protocol/transport, authority, server hosting, single-player, listen-server, and dedicated-server ownership without discarding the completed socket/session work.

Problem statement:

- `stellar-client --connect HOST:PORT` already behaves like a presentation-only remote client.
- `stellar-server` already behaves like a headless authoritative dedicated server.
- Local mapped play still makes the client own a transport-backed server bridge and authoritative runtime setup.
- Protocol DTOs still depend on server and scripting implementation types.
- Client startup/config code and dedicated server startup duplicate BSP/script/authority preparation logic.
- `LocalServerBridge` lives in the client namespace even though it owns authoritative server session state.

Target runtime modes:

| Mode | Command | Authority Owner | Socket Listener | Script Loading | Lifetime |
|---|---|---|---:|---:|---|
| Single-player | `stellar-client --map path/to/map.bsp` | In-process single-player authority facade | no | authority-side only | client |
| Remote client | `stellar-client --connect HOST:PORT` | remote server/listen host | no | no | client |
| Listen server | `stellar-client --host --map path/to/map.bsp [--listen HOST:PORT]` | in-process server host | yes when listening | authority-side only | host client |
| Dedicated server | `stellar-server --map path/to/map.bsp --listen HOST:PORT` | server process | yes | authority-side only | server process |

Immediate Phase CS-0 tasks:

- [x] Create `Plans/ClientServerSplit-AgentPlan.md` from the proposal and preserve the proposal content.
- [x] Update `Plans/NEXT.md` so client/server decoupling is the active focus.
- [x] Mark completed socket transport and TrenchBroom work as historical and not to be restarted.
- [x] Record target runtime modes: single-player, remote client, listen server, and dedicated server.
- [x] Update `docs/ImplementationStatus.md` with active scope, problem statement, intended module boundaries, phase checklist, dependency guardrails, and TODO policy.
- [x] Run Phase CS-0 validation and record results.

## Dependency Audit Guardrails

Build graph and source dependency work must preserve these boundaries as the split is implemented:

- Protocol must not link server, scripting, client runtime, graphics, audio, or platform presentation targets.
- Transport must depend only on protocol plus low-level platform/system support and must not depend on server runtime, scripting, or client presentation.
- Remote client runtime must not link authority, server runtime/core, or scripting.
- Dedicated server must not link client runtime or presentation.
- Authority/server-side script loading must remain sandboxed and authoritative-side only.
- Broad TODOs must be tracked in `Plans/ClientServerSplit-AgentPlan.md`; do not scatter TODO comments through source.

Useful audit commands for later code phases:

```bash
git grep -n 'stellar/server/\|stellar/scripting/' -- include/stellar/network src/network include/stellar/client src/client ':!Plans/Archived/**'
git grep -n 'LocalServerBridge\|LocalLoopbackRuntime\|networked_runtime' -- include src tests ':!Plans/Archived/**'
cmake --build build --target help
```

## Active Phase Checklist

- [x] CS-0 - Architecture baseline and guardrails.
- [ ] CS-1 - Extract protocol DTOs away from server/scripting implementation types.
- [ ] CS-2 - Extract shared authority bootstrap.
- [ ] CS-3 - Move `LocalServerBridge` out of the client and generalize it as server runtime.
- [ ] CS-4 - Introduce a single client-facing runtime interface.
- [ ] CS-5 - Add true single-player runtime without server startup.
- [ ] CS-6 - Add listen-server host mode.
- [ ] CS-7 - Remove legacy local loopback runtime and client-owned authority leftovers.
- [ ] CS-8 - Build graph enforcement.
- [ ] CS-9 - Documentation update and final handoff.

Do not begin CS-7 deletion until CS-5 and CS-6 replacement tests are passing.

## Historical Completed Scope

Completed plan packages remain archived and must not be restarted:

- BSP gameplay loop: `Plans/Archived/bsp_gameplay_loop/`.
- BSP presentation/networking polish: `Plans/Archived/bsp_presentation_networking_polish/`.
- Socket transport: `Plans/Archived/socket_transport/`.
- TrenchBroom BSP30 compatibility and Z-up migration: `Plans/Archived/trenchbroom_compat/`.

The completed socket transport and networked session lifecycle work is retained as implementation context only. The branch already has `stellar-server`, `stellar-client --connect HOST:PORT`, Linux/POSIX TCP `SocketTransport`, `ClientHello`, `ServerWelcome`, `ClientWorldReceiver`, and `NetworkWorldSnapshot` presentation. Do not restart socket transport/session lifecycle work.

The completed TrenchBroom BSP30 compatibility work is retained as implementation context only. The branch already has the project-owned Stellar TrenchBroom package, BSP30 compile/validation wrappers, Z-up authoring/runtime conventions, fixture coverage, lightmap support, and server-authoritative `func_door`/`func_button` support. Do not restart TrenchBroom compatibility or Z-up migration work.

Do not restart completed collision, movement, Lua scripting, object-collider, BSP migration, BSP hardening, BSP gameplay-loop foundation work, BSP presentation/networking polish work, socket transport/session lifecycle work, or TrenchBroom compatibility work.

## Invariants

- BSP remains the canonical playable level format.
- The active world-axis convention is Z-up.
- TrenchBroom authoring targets BSP30, with 1 editor unit = 1 gameplay inch.
- Default player spawn centers are authored 36 inches above the floor for the default capsule.
- Server authority remains mandatory for networked, listen-server, and dedicated-server play.
- Single-player may run authoritative simulation in-process, but it must not start a network server or listen socket.
- Lua scripting remains mandatory, sandboxed, and authoritative-side only.
- Remote client mode must not load gameplay scripts, construct `WorldSession`, construct `ScriptedWorldSession`, or own authoritative runtime state.
- Import never executes scripts.
- Runtime collision, movement, triggers, object colliders, scripting, and networking contracts remain backend-neutral.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable through the shared graphics abstraction.
- Rendering, audio, HUD, and UI are presentation only and never sources of gameplay truth.
- No client prediction, interpolation, map transfer, or reconciliation is active in the completed socket transport scope.
- Do not add Source/VBSP, dynamic rigid bodies, moving brush classes beyond the implemented `func_door`/`func_button` path, full PBR, client-side gameplay scripting, model/animation systems, third-party physics, arbitrary non-Stellar entity parity, UDP/unreliable transport, authentication, encryption, matchmaking, public Internet deployment, or retired importer functionality unless explicitly requested.

## Validation Runbook

Phase CS-0 docs/guardrails validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

If full validation is infeasible, run the narrowest available configure/build graph check and record the limitation in `docs/ImplementationStatus.md`.

Phase CS-0 result on 2026-05-03: CMake configure passed, full debug build passed, and full default
CTest passed 91/91.

Future focused validation should follow the phase-specific commands in `Plans/ClientServerSplit-AgentPlan.md`.
