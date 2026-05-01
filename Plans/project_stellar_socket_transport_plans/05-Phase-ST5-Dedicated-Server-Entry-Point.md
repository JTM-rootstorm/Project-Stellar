---
project: Project Stellar
branch: socket-transport
plan_family: socket_transport
agent_optimized_for: Kilo Code
status: draft_handoff
generated: 2026-05-01
---

# Phase ST-5 — Dedicated Server Entry Point

## Goal

Add a minimal `stellar-server` executable that loads a BSP map, owns authoritative runtime/session state, accepts a socket client, and emits authoritative snapshots/deltas/events.

## Primary owner

`@carmack`, with `@director` integration review.

## First-stage server scope

The server should support one active player connection unless `WorldSession` has been explicitly expanded for multiple authoritative players.

Required behavior:

- Server owns map loading.
- Server owns script loading.
- Server owns player id assignment.
- Server owns authoritative ticks.
- Server emits snapshots/deltas/gameplay events over transport.
- Client never owns gameplay truth.

Do not claim full simultaneous multi-player gameplay unless implemented and tested.

## Implementation tasks

### ST-5.1 Add server runtime abstraction

Preferred files:

- `include/stellar/server/DedicatedServer.hpp`
- `src/server/DedicatedServer.cpp`
- `tests/server/DedicatedServer.cpp`

Suggested types:

```cpp
struct DedicatedServerConfig {
    std::string map_path;
    std::optional<std::string> script_root;
    std::string listen_endpoint = "127.0.0.1:29070";
    int tick_rate = 60;
    int snapshot_rate = 20;
    int max_clients = 1;
    bool validate_only = false;
};

struct DedicatedServerStatus {
    bool running = false;
    std::string map_id;
    std::uint64_t tick = 0;
    std::uint32_t connected_clients = 0;
};

class DedicatedServer {
public:
    static std::expected<DedicatedServer, stellar::platform::Error> create(
        DedicatedServerConfig config);

    [[nodiscard]] std::expected<void, stellar::platform::Error> run();
    [[nodiscard]] DedicatedServerStatus status() const noexcept;
};
```

Adapt names to project style.

### ST-5.2 Reuse existing map/script prep

Do not duplicate BSP/script loading logic if it can be factored safely from client prep.

Acceptable refactor:

- Move shared map/script/runtime preparation into a backend-neutral helper under `stellar::server` or `stellar::world`.
- Client and server call the same helper.
- Keep platform/window/graphics dependencies out of server code.

The server must fail deterministically for:

- missing map
- non-BSP extension
- invalid BSP
- script path escape
- missing script source
- script load failure

### ST-5.3 Integrate session lifecycle and socket transport

Server run loop responsibilities:

1. Create socket server transport.
2. Accept/connect one client.
3. Wait for `ClientHello`.
4. Validate protocol and map identity.
5. Send `ServerWelcome`.
6. Receive client input commands.
7. Advance authoritative tick at fixed rate.
8. Send full snapshot first, then deltas when available.
9. Send server-approved gameplay events.
10. Detect disconnect and shut down or return to waiting state.

For tests, expose a bounded `pump_once(delta_seconds)` path rather than requiring an infinite loop.

### ST-5.4 Add `stellar-server` executable

Add `src/server/main.cpp` or equivalent and CMake target:

```bash
stellar-server --map path/to/map.bsp --listen 127.0.0.1:29070
stellar-server --validate-config --map path/to/map.bsp
stellar-server --map path/to/map.bsp --script-root path/to/scripts --listen 0.0.0.0:29070
```

CLI requirements:

- Unknown args fail deterministically.
- `--map` is required except maybe help mode.
- `--validate-config` loads/validates map/scripts and exits before opening socket.
- `--listen` parses endpoint.
- `--tick-rate` and `--snapshot-rate` validate finite positive integer ranges.
- `--max-clients` defaults to 1 for this phase.

### ST-5.5 Tests

Display-free tests:

- Server config parse success/failure.
- Validate-only map path succeeds for generated BSP fixture.
- Invalid map path fails.
- Missing script fails when map references script.
- Server accepts local socket client hello and sends welcome.
- Server sends first full snapshot.
- Server receives input command and updates snapshot.
- Scripted pickup/door event crosses server runtime into gameplay event packets.
- Disconnect does not crash.

Use generated BSP fixtures already in tests. Do not require a display/GPU.

## Out of scope

- Running a public Internet server.
- Multi-process flaky integration tests unless deterministic and bounded.
- Multi-player simulation beyond the current session implementation.
- Server UI/admin console.
- Hot map reload.
- Persistence/save games.

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar-server stellar_dedicated_server_test stellar_socket_transport_test stellar_network_session_test -j$(nproc)
ctest --test-dir build -R '^(dedicated_server|socket_transport|network_session|snapshot_|scripted_world_session|bsp_)' --output-on-failure
```

Full:

```bash
ctest --test-dir build --output-on-failure
```

## Documentation updates

Update:

- `docs/Design.md`: document `stellar-server` as authoritative entry point.
- `docs/ImplementationStatus.md`: ST-5 completion notes.
- `Plans/NEXT.md`: active phase moves to client connect mode.
- Optional new `docs/Networking.md` if the implementation details need a focused reference.

## Completion checklist

- [ ] `stellar-server` target exists.
- [ ] `--validate-config --map` works display-free.
- [ ] Server can accept one socket client.
- [ ] Server owns map/script/runtime/session.
- [ ] Welcome and first snapshot sent.
- [ ] Input updates authoritative snapshot.
- [ ] Focused tests pass.
- [ ] Full default CTest passes.
