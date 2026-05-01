---
project: Project Stellar
branch: socket-transport
plan_family: socket_transport
agent_optimized_for: Kilo Code
status: draft_handoff
generated: 2026-05-01
---

# Phase ST-4 — Remote Socket Transport

## Goal

Implement Linux-first remote socket transport behind the existing transport abstraction so client/server packets can move over localhost or LAN without changing gameplay/message contracts.

## Primary owner

`@carmack`.

## Transport strategy

Use a minimal, deterministic, testable socket backend. The recommended first implementation is **TCP with length-prefixed packets**.

Rationale:

- The existing transport interface already models reliable FIFO delivery.
- TCP lets this stage prove real remote client/server gameplay without building reliability/ordering/loss recovery.
- The `TransportChannel` field should still be preserved in the packet envelope for future UDP/unreliable work.

True UDP/unreliable networking remains deferred unless the user explicitly scopes it.

## Implementation tasks

### ST-4.1 Add socket endpoint parsing

Add:

- `include/stellar/network/SocketAddress.hpp` or inside `SocketTransport.hpp`
- parser for `host:port`
- validation for empty host, invalid port, out-of-range port
- deterministic error codes/messages

Examples:

- `127.0.0.1:29070`
- `localhost:29070`
- `0.0.0.0:29070` for server bind

Tests cover valid and invalid endpoints.

### ST-4.2 Add TCP packet envelope

Define a compact packet envelope:

```text
magic/version
channel byte
payload length uint32 little-endian
payload bytes
```

Requirements:

- Maximum payload size constant.
- Reject oversized payloads before allocation.
- Handle partial reads/writes.
- Preserve packet boundaries.
- Decode errors do not crash or throw.
- Public APIs use `std::expected`.

### ST-4.3 Implement client socket transport

Preferred files:

- `include/stellar/network/SocketTransport.hpp`
- `src/network/SocketTransport.cpp`
- `tests/network/SocketTransport.cpp`

Preferred API:

```cpp
struct SocketTransportConfig {
    std::size_t max_packet_bytes = ...;
    bool nonblocking = true;
};

[[nodiscard]] std::expected<std::unique_ptr<ClientTransport>, TransportError>
connect_tcp_client(std::string_view endpoint, SocketTransportConfig config = {});
```

Client behavior:

- Connects to server endpoint.
- `send_to_server()` writes one encoded packet envelope.
- `receive_from_server()` drains all available complete packets.
- Clean disconnect is reported deterministically.
- Nonblocking mode must not spin or block tests indefinitely.

### ST-4.4 Implement server socket transport

Because the existing `ServerTransport` is single-client, pick one of these approaches:

Preferred for this phase:

1. Implement a single-client `TcpServerTransport` that satisfies `ServerTransport`.
2. Add a clearly named future-ready multi-client facade only if needed by ST-5.

Minimum viable API:

```cpp
[[nodiscard]] std::expected<std::unique_ptr<ServerTransport>, TransportError>
listen_tcp_server_once(std::string_view bind_endpoint, SocketTransportConfig config = {});
```

Behavior:

- Bind/listen on configured address.
- Accept one client connection.
- Drain client packets.
- Send server packets.
- Detect disconnect.
- No external network required for tests.

If a multi-client server transport is added, keep it additive and do not break existing `ServerTransport`.

### ST-4.5 CMake and platform guards

- Linux/POSIX sockets are acceptable for this branch.
- Add compile-time checks or platform guards for non-POSIX platforms.
- Keep default build working on Linux.
- Do not introduce new third-party networking dependencies unless the user approves.

### ST-4.6 Socket tests

Add local-only tests using loopback and ephemeral ports where possible.

Test cases:

- Client connects to local server.
- Reliable packet arrives FIFO.
- Multiple packets preserve boundaries.
- Partial write/read handling works.
- Oversized payload rejected.
- Invalid endpoint rejected.
- Disconnect is surfaced without crash.
- Session hello/welcome can cross real socket transport.
- Snapshot packet can cross real socket transport and decode through `ClientWorldReceiver`.

Avoid timing-fragile sleeps. Prefer nonblocking pump loops with small bounded iteration counts.

## Out of scope

- UDP.
- Packet loss simulation.
- Encryption/TLS.
- NAT traversal.
- Public Internet server discovery.
- Multi-player server simulation.

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_socket_transport_test stellar_network_session_test stellar_snapshot_codec_test -j$(nproc)
ctest --test-dir build -R '^(socket_transport|network_session|snapshot_codec|loopback_transport)$' --output-on-failure
```

Full:

```bash
ctest --test-dir build --output-on-failure
```

## Documentation updates

Update:

- `docs/Design.md`: socket transport is TCP-first, Linux-first, transport-neutral, and not prediction/reconciliation.
- `docs/ImplementationStatus.md`: ST-4 notes and validation.
- `Plans/NEXT.md`: active phase moves to dedicated server entry point.

## Completion checklist

- [ ] Socket endpoint parser exists and is tested.
- [ ] Client socket transport implements `ClientTransport`.
- [ ] Server socket transport implements `ServerTransport`.
- [ ] Packet envelope handles partial read/write and bounds.
- [ ] Hello/welcome crosses real socket transport.
- [ ] Snapshot packet crosses real socket transport.
- [ ] Focused tests pass.
- [ ] Full default CTest passes.
