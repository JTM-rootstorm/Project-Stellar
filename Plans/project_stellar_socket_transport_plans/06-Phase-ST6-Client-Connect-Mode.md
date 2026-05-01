---
project: Project Stellar
branch: socket-transport
plan_family: socket_transport
agent_optimized_for: Kilo Code
status: draft_handoff
generated: 2026-05-01
---

# Phase ST-6 — Client Connect Mode

## Goal

Add a `stellar-client --connect HOST:PORT` mode that connects to `stellar-server`, performs session handshake, sends input commands, receives authoritative snapshots/deltas/events, and renders from the received network state.

## Primary owner

`@carmack` for client/server/network integration. Consult `@miyamoto` only if renderer APIs must change.

## Runtime behavior

`stellar-client --connect HOST:PORT`:

- Does not require `--map` for authoritative gameplay state.
- Opens socket `ClientTransport`.
- Sends `ClientHello`.
- Receives `ServerWelcome`.
- Stores assigned player id.
- Sends input commands with monotonic command sequence.
- Drains `ClientWorldReceiver`.
- Renders camera and gameplay billboards from `NetworkWorldSnapshot`.
- Routes server-approved gameplay events to HUD/audio/presentation caches if live app already owns those caches.
- Rejects malformed or mismatched packets without crashing.

A combined local-host development command should work:

```bash
stellar-server --map path/to/map.bsp --listen 127.0.0.1:29070
stellar-client --connect 127.0.0.1:29070
```

## Implementation tasks

### ST-6.1 Extend application config and CLI

Update:

- `include/stellar/client/ApplicationConfig.hpp`
- `src/client/main.cpp`
- `src/client/ApplicationConfig.cpp`
- `src/client/Application.cpp`

Add config fields similar to:

```cpp
std::optional<std::string> connect_endpoint;
std::string client_name = "local";
```

CLI:

```bash
stellar-client --connect 127.0.0.1:29070
stellar-client --connect localhost:29070 --client-name Mike
stellar-client --validate-config --connect 127.0.0.1:29070
```

Conflict rules:

- `--map` without `--connect`: local networked runtime mode from ST-2.
- `--connect` without `--map`: remote client mode.
- `--map` plus `--connect`: either reject as ambiguous or treat map as optional compatibility expectation. Prefer reject unless Design.md says otherwise.
- `--script-root` only applies to local map/server-authority prep; remote client must not load gameplay scripts.

### ST-6.2 Add remote client runtime

Extend `NetworkedClientRuntime` or add `RemoteClientRuntime`.

Responsibilities:

- Own socket `ClientTransport`.
- Send hello and process welcome.
- Convert local input to `NetworkPlayerCommand`.
- Send commands only after accepted welcome.
- Drain `ClientWorldReceiver`.
- Expose latest snapshot, events, connection state, diagnostics, assigned player id.

Do not include prediction/reconciliation.

### ST-6.3 Update live render path

In remote connect mode:

- Use assigned player id from welcome.
- Extract camera from latest `NetworkWorldSnapshot`.
- Build gameplay billboard presentation from latest `NetworkWorldSnapshot`.
- If no snapshot exists yet, clear render view/presentation or show fallback camera safely.
- Do not create authoritative local runtime or load gameplay scripts on the client.

Renderer still needs a level to draw static BSP geometry. Pick one strategy and document it clearly:

Preferred minimal strategy:

1. Server welcome includes map identity only.
2. Remote client initially renders dynamic sprites/camera fallback until a future map distribution/caching phase.
3. For developer flow, allow optional `--presentation-map path/to/map.bsp` that loads the same BSP for static presentation only and validates map identity against welcome.

Alternative:

- Keep `--map` + `--connect` as presentation-only map path, explicitly documented as non-authoritative.

Do not implement map file transfer in this phase.

### ST-6.4 Route gameplay events

Connect received `GameplayEvent` records to existing presentation sinks:

- HUD cache.
- Audio event router/no-op sink.
- Tests should verify events arrive; live UI display can remain minimal if no HUD renderer exists.

### ST-6.5 Tests

Display-free tests:

- CLI parse accepts `--connect`.
- CLI parse rejects ambiguous or invalid combinations.
- Remote runtime sends hello.
- Remote runtime accepts welcome.
- Remote runtime sends input only after welcome.
- Remote runtime receives snapshot and exposes player presentation.
- Remote runtime receives gameplay events and drains them.
- Connect mode does not construct local authoritative runtime.
- Presentation-map behavior, if implemented, validates map identity or fails deterministically.

A bounded localhost integration test may start a server runtime in-process, not necessarily as a child process.

## Out of scope

- Map download/transfer.
- Prediction/reconciliation.
- Interpolation.
- Rich connection UI.
- Passwords/authentication.
- Public server browser.
- Hot reconnect after process restart.

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar-client stellar-server stellar_client_connect_test stellar_dedicated_server_test stellar_socket_transport_test -j$(nproc)
ctest --test-dir build -R '^(client_connect|dedicated_server|socket_transport|network_session|networked_client_runtime|client_world_receiver|gameplay_presentation|player_presentation)' --output-on-failure
```

Full:

```bash
ctest --test-dir build --output-on-failure
```

## Documentation updates

Update:

- `docs/Design.md`: remote client mode and presentation-map policy.
- `docs/ImplementationStatus.md`: ST-6 completion notes.
- `Plans/NEXT.md`: active phase moves to hardening/docs/archive.
- `docs/BspAuthoring.md`: only if map identity/presentation-map workflow affects authoring validation.

## Completion checklist

- [ ] `stellar-client --connect HOST:PORT` exists.
- [ ] Remote client performs hello/welcome.
- [ ] Remote client sends input through socket transport.
- [ ] Remote client renders from network snapshot.
- [ ] Gameplay events reach client presentation queues.
- [ ] Client does not load gameplay scripts in remote mode.
- [ ] Static map presentation policy is documented.
- [ ] Focused tests pass.
- [ ] Full default CTest passes.
