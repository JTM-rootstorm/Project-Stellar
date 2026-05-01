# Phase PN-4 — Local/Remote Transport Bridge and Client-Server Message Path

Optimized for Kilo Code intake.

## Owner

- Primary: `@carmack`
- Coordinator/reviewer: `@director`
- Presentation integration review: `@miyamoto` only where client presentation hookup is affected

## Goal

Introduce a transport-neutral client/server message path that can run the existing local loopback flow through the same snapshot/input contract intended for future remote play.

This phase should still keep default tests display-free and can ship with local/in-memory transport first. Real sockets may be stubbed or minimal if not already present.

## Required reading

- Phase PN-3 result
- `docs/Design.md` networking section
- `include/stellar/client/LocalLoopbackRuntime.hpp`
- `src/client/LocalLoopbackRuntime.cpp`
- `src/client/Application.cpp`
- Any existing `include/stellar/network/` or `src/network/` files

## Scope

Add the seam between client input and server snapshots:

- Client serializes/submits input commands.
- Server/local authority consumes commands.
- Server emits snapshot/delta/events.
- Client consumes snapshots/events and updates presentation state.

Remote socket implementation is optional in this phase. The required deliverable is the transport-neutral bridge and in-memory transport tests.

## Recommended architecture

### 1. Transport interface

Suggested files:

```text
include/stellar/network/Transport.hpp
src/network/LoopbackTransport.cpp
tests/network/LoopbackTransport.cpp
```

Suggested API:

```cpp
enum class TransportChannel {
    kReliable,
    kUnreliable,
};

struct TransportPacket {
    TransportChannel channel;
    std::vector<std::byte> payload;
};

class ClientTransport {
public:
    virtual ~ClientTransport() = default;
    virtual std::expected<void, platform::Error> send_to_server(TransportPacket packet) = 0;
    virtual std::vector<TransportPacket> receive_from_server() = 0;
};

class ServerTransport {
public:
    virtual ~ServerTransport() = default;
    virtual std::vector<TransportPacket> receive_from_client() = 0;
    virtual std::expected<void, platform::Error> send_to_client(TransportPacket packet) = 0;
};
```

In-memory loopback implementation can own two queues.

### 2. Local authoritative server adapter

Add a server adapter that wraps local authoritative runtime/session and processes network messages.

Suggested files:

```text
include/stellar/client/LocalServerBridge.hpp
src/client/LocalServerBridge.cpp
```

or under `network/` if cleaner:

```text
include/stellar/network/LocalServerBridge.hpp
src/network/LocalServerBridge.cpp
```

Responsibilities:

- Decode client input messages.
- Tick authoritative runtime with commands.
- Encode snapshot/delta/event messages.
- Preserve fixed-tick semantics.
- Preserve scripted vs plain session mode from Phase PN-1.

### 3. Client network state

Add a client-side receiver that decodes server snapshots/events into a presentation cache.

Suggested files:

```text
include/stellar/client/ClientWorldReceiver.hpp
src/client/ClientWorldReceiver.cpp
tests/client/ClientWorldReceiver.cpp
```

Responsibilities:

- Accept full snapshot.
- Apply delta to current baseline.
- Expose latest authoritative/interpolatable snapshot to presentation.
- Buffer events for HUD/audio presentation.
- No prediction in this phase.

### 4. Application integration

Add config mode only if useful:

- Default single-player can continue using direct local runtime until bridge is stable.
- Prefer adding a compile/test path first, then optionally route live local play through bridge.
- If routing live play through bridge, preserve exact behavior from Phase PN-1/2.

Avoid breaking current `LocalLoopbackRuntime` public tests until bridge is ready.

### 5. Remote transport placeholder

If adding real transport is too broad, add only interfaces and document remote socket implementation as next. If adding a minimal socket transport, keep it isolated and optional.

Do not implement prediction/reconciliation yet.

## Tests

Required display-free tests:

1. Loopback transport preserves reliable packet order.
2. Client input command serializes, crosses loopback, and decodes on server side.
3. Server emits snapshot that decodes into client receiver.
4. Delta message applies to client receiver baseline.
5. Gameplay event crosses bridge and is queued for presentation.
6. Scripted local server bridge propagates pickup/door events from Phase PN-1.
7. Malformed packet is rejected without crashing.

## Validation commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
  stellar_loopback_transport_test \
  stellar_client_world_receiver_test \
  stellar_snapshot_codec_test \
  stellar_snapshot_delta_test \
  -j$(nproc)

ctest --test-dir build -R '^(loopback_transport|client_world_receiver|snapshot_codec|snapshot_delta)$' --output-on-failure
```

Then:

```bash
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- There is a transport-neutral path for input commands and server snapshots/events.
- Local loopback can exercise the same message contracts planned for remote play.
- Client presentation can consume snapshots/events from receiver state.
- No client-side authority introduced.
- No prediction/reconciliation added unless explicitly scoped later.
- Default validation remains display-free.
