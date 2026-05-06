---
phase: MC-2
title: macOS POSIX Socket Portability
depends_on: [MC-1]
can_parallelize: true
---

# MC-2 — macOS POSIX Socket Portability

## Goal

Make `stellar_transport` compile and behave correctly on macOS by replacing Linux-only socket assumptions.

## Files likely changed

```text
src/network/SocketTransport.cpp
tests/network/SocketTransport.cpp       # if existing coverage needs expansion
tests/cmake/NetworkTests.cmake          # only if adding a focused test
docs/ImplementationStatus.md
```

## Problem

`SocketTransport.cpp` includes `__APPLE__` in the POSIX path but calls:

```cpp
::send(socket_.get(), data, remaining, MSG_NOSIGNAL)
```

`MSG_NOSIGNAL` is Linux-oriented. macOS/BSD should use `SO_NOSIGPIPE` on the socket and pass `0` as the send flags, or use another explicit SIGPIPE suppression policy.

## Tasks

1. Introduce a small platform helper:
   ```cpp
   [[nodiscard]] int send_flags() noexcept {
   #if defined(MSG_NOSIGNAL)
       return MSG_NOSIGNAL;
   #else
       return 0;
   #endif
   }
   ```
2. On Apple/BSD, after socket creation and before sending, set:
   ```cpp
   #if defined(__APPLE__) && defined(SO_NOSIGPIPE)
       const int one = 1;
       (void)::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
   #endif
   ```
3. Apply this to accepted client sockets and outbound client sockets.
4. Preserve existing `would_block()` and bounded I/O behavior.
5. Add/adjust tests so encode/decode and loopback/default transport tests remain display-free.
6. Do not add platform UI dependencies to `stellar_transport`.

## Acceptance criteria

- `stellar_transport` compiles on macOS.
- Linux behavior remains unchanged.
- No direct dependencies from protocol/transport to client, graphics, audio, authority, or server runtime are introduced.
- Socket tests remain default CTest safe.

## Validation

```bash
cmake --build build --target stellar_transport_test stellar_socket_transport_test -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
ctest --test-dir build -R '^(transport|socket_transport|network_session)$' --output-on-failure
tools/dev/check_target_boundaries.sh .
```
