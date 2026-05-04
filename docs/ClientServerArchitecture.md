# Client/Server Architecture

Status: client/server split complete through CS-9 as of 2026-05-03.

## Mode Matrix

| Mode | CLI | Authority | Transport | Presentation | Script Loading |
| --- | --- | --- | --- | --- | --- |
| Single-player no-server | `stellar-client --map path/to/map.bsp` | in-process `stellar_single_player` over `stellar_authority` | none; no listener or handshake | local client | authority-side only |
| Remote client | `stellar-client --connect HOST:PORT` | remote server/listen host | socket client via `stellar_client_net`/`stellar_transport` | local client | none |
| Listen server | `stellar-client --host --map path/to/map.bsp [--listen HOST:PORT]` | in-process server host via `stellar_listen_server`/`stellar_server_runtime` | loopback/in-process host path plus optional socket listener | host client | authority-side only |
| Dedicated server | `stellar-server --map path/to/map.bsp --listen HOST:PORT` | server process via `stellar_dedicated_server` | socket listener | none/headless | authority-side only |

## Ownership Table

| Area | Owner Module | Notes |
| --- | --- | --- |
| Protocol DTOs/codecs | `stellar_protocol` | No server, scripting, client runtime, graphics, audio, or presentation dependencies. |
| Transport endpoints | `stellar_transport` | Loopback and TCP/POSIX transport; depends on protocol and low-level platform/system support. |
| BSP/script authority prep | `stellar_authority` | Loads/validates maps, resolves script roots, creates authority sessions, converts snapshots/events. |
| Networked server lifecycle | `stellar_server_runtime` | Shared by dedicated and listen-server modes. |
| Dedicated server CLI/runtime | `stellar_dedicated_server` | Thin headless wrapper; no client runtime/presentation links. |
| Single-player runtime | `stellar_single_player` | Local authoritative stepping without server startup or session handshake. |
| Remote/listen client networking | `stellar_client_net` | Socket client, session state, input sequencing, `ClientWorldReceiver`; no authority/server/scripting links. |
| Presentation helpers | `stellar_client_presentation` | Player/gameplay/HUD/audio/render-view presentation only. |
| Client application | `stellar_client_app` | Mode selection, platform/window/input, and frame loop composition. |
| Listen host | `stellar_listen_server` | Host-client lifetime binding over server runtime and transport. |

## Target Graph / CMake Targets

```text
stellar_protocol
    ▲
    ├── stellar_transport
    │       ├── stellar_client_net
    │       └── stellar_server_runtime
    │
    ├── stellar_authority
    │       ├── stellar_single_player
    │       └── stellar_server_runtime
    │               ├── stellar_dedicated_server → stellar-server
    │               └── stellar_listen_server
    │
    └── stellar_client_presentation
            └── stellar_client_app → stellar-client
```

`stellar_client_app` may compose `stellar_single_player`, `stellar_client_net`,
`stellar_listen_server`, and `stellar_client_presentation`, but remote-client isolation is enforced at
`stellar_client_net` rather than by forbidding top-level application composition.

Compatibility shims may remain for transition, but new code should target the split libraries.

## CLI Examples

```bash
# Validate and run no-server single-player authority in-process.
stellar-client --map assets/maps/example.bsp

# Presentation-only remote client.
stellar-client --connect 127.0.0.1:29070 --client-name Mike

# Host a listen server whose lifetime is tied to the client process.
stellar-client --host --map assets/maps/example.bsp --listen 127.0.0.1:29070

# Headless dedicated server.
stellar-server --map assets/maps/example.bsp --listen 0.0.0.0:29070

# Display-free authority/map validation.
stellar-server --validate-config --map assets/maps/example.bsp
```

## Deferred Features

- Presentation-map workflow for remote clients.
- Client interpolation and presentation smoothing.
- Client prediction/reconciliation.
- True multiplayer beyond the current one accepted TCP client / one active player limit.
- UDP/unreliable transport and transport selection.
- Map transfer/caching.
- Authentication, encryption, matchmaking, reconnect/resume, and public Internet deployment.

## Testing Strategy and Boundary Checks

Default validation remains display-free:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Focused client/server validation:

```bash
ctest --test-dir build -R '^(protocol|transport|network_session|socket_transport|server_runtime|dedicated_server|listen_server_host|client_single_player_runtime|client_connect|client_map_validation_smoke|client_cli_map_validation|client_world_receiver|gameplay_presentation|player_presentation|hud_presentation|audio_event_router|bsp_|runtime_world|server_world_session|scripted_world_session)' --output-on-failure
```

Boundary audit:

```bash
git grep -n 'LocalServerBridge\|LocalLoopbackRuntime' -- include src tests ':!Plans/Archived/**'
git grep -n 'stellar/server/WorldSession.hpp\|stellar/scripting/ScriptedWorldSession.hpp' -- include/stellar/network src/network include/stellar/client src/client ':!Plans/Archived/**'
tools/dev/check_target_boundaries.sh
```

Expected authority references may remain in authority, server-runtime, dedicated-server,
single-player, listen-server, or top-level client application composition paths. They must not appear as
remote-client runtime ownership or links; `stellar_client_net` is the enforced remote-client isolation
boundary.
