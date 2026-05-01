---
project: Project Stellar
branch: socket-transport
plan_family: socket_transport
agent_optimized_for: Kilo Code
status: draft_handoff
generated: 2026-05-01
---

# Phase ST-3 — Connection and Session Lifecycle

## Goal

Add deterministic connection/session lifecycle messages and state machines over existing transports before implementing real sockets.

This phase establishes how a client joins a server, how the server assigns authority, how protocol/map mismatches are rejected, and when input is accepted.

## Primary owner

`@carmack`, with `@director` architecture review.

## Design scope

Support a first-stage networked session with one active local player slot unless `WorldSession` has already been expanded for multiple players.

The protocol should be multi-client-ready, but implementation must not claim simultaneous multi-player gameplay until server simulation actually supports multiple authoritative players.

## Implementation tasks

### ST-3.1 Add session protocol types

Add a new public header or extend existing message headers. Preferred:

- `include/stellar/network/Session.hpp`
- `src/network/Session.cpp`
- `tests/network/Session.cpp`

Required concepts:

```cpp
using ProtocolVersion = std::uint32_t;
using ConnectionId = std::uint32_t;
using SessionId = std::uint64_t;

enum class SessionState : std::uint8_t {
    kDisconnected,
    kConnecting,
    kConnected,
    kRejected,
    kTimedOut,
};

struct ClientHello {
    ProtocolVersion protocol_version;
    std::string client_name;
    std::string requested_map_id;
    std::uint64_t client_nonce;
};

struct ServerWelcome {
    bool accepted;
    ProtocolVersion protocol_version;
    SessionId session_id;
    stellar::server::PlayerId assigned_player_id;
    std::string map_id;
    std::string rejection_code;
    std::string message;
};
```

Use final field names that fit project style. All public APIs require Doxygen `@brief`.

### ST-3.2 Define map identity

Add a deterministic map identity helper for session compatibility.

Minimum viable map identity:

- source path basename or normalized source URI
- optional content hash if available or easy to compute deterministically
- imported level source URI
- script identity is not required for this phase, but script load failures must still reject startup

Preferred helper:

```cpp
struct MapIdentity {
    std::string map_id;
    std::string source_uri;
    std::uint64_t content_hash = 0;
};
```

Do not block the phase on cryptographic hashing. A deterministic FNV-1a-style byte hash is acceptable if no project hash helper exists.

### ST-3.3 Extend binary codec

Add encode/decode support for:

- `ClientHello`
- `ServerWelcome`
- optional `ClientDisconnect`
- optional `ServerDisconnect`
- session rejection diagnostics

Requirements:

- Explicit little-endian fields like existing codec.
- Bounded strings.
- Clean `std::expected` failures for malformed/truncated/oversized messages.
- Protocol version mismatch produces deterministic rejection.
- Unknown message types are rejected without crashing.

### ST-3.4 Add session state machines

Add small state machines:

- client-side session state for hello/welcome/rejected/disconnected
- server-side pending/accepted session state

Requirements:

- Input commands before accepted welcome are rejected or ignored deterministically.
- Server overwrites/validates requested player id with assigned player id.
- The initial server snapshot is sent only after welcome acceptance.
- A rejected client receives an explanatory `ServerWelcome{accepted=false}` or equivalent.

### ST-3.5 Integrate session lifecycle with local networked runtime

Update ST-2 local networked runtime:

- On create, send `ClientHello`.
- Pump bridge/session until `ServerWelcome` accepted.
- Store assigned player id.
- Use assigned player id for outgoing commands and presentation extraction.
- Expose connection/session diagnostics in frame results.

For local mapped play, map identity should match automatically.

### ST-3.6 Tests

Add display-free tests:

- Client hello round-trip.
- Welcome accepted with assigned player id.
- Protocol version mismatch rejected.
- Map mismatch rejected.
- Input before welcome ignored/rejected.
- After welcome, input commands affect authoritative snapshot.
- Malformed session packets are rejected without crashing.
- Assigned player id is used for presentation extraction.

## Out of scope

- Remote sockets.
- Dedicated server CLI.
- Timeout based on real wall clock unless simple deterministic tick/frame counters are already available.
- Multi-player simulation expansion.

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_network_session_test stellar_snapshot_codec_test stellar_networked_client_runtime_test stellar_client_world_receiver_test -j$(nproc)
ctest --test-dir build -R '^(network_session|snapshot_codec|networked_client_runtime|client_world_receiver|loopback_transport)$' --output-on-failure
```

Full:

```bash
ctest --test-dir build --output-on-failure
```

## Documentation updates

Update:

- `docs/Design.md`: describe session lifecycle and first-stage active-player limit.
- `docs/ImplementationStatus.md`: record ST-3 completion notes and validation.
- `Plans/NEXT.md`: keep ST scope active; do not switch to post-ST yet.

## Completion checklist

- [ ] Session protocol types exist.
- [ ] Codec handles hello/welcome/reject deterministically.
- [ ] Local networked runtime uses handshake before input.
- [ ] Server authority assigns player id.
- [ ] Mismatched protocol/map is rejected.
- [ ] Focused tests pass.
- [ ] Full default CTest passes.
