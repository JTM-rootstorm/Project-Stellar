---
project: Project Stellar
branch: socket-transport
plan_family: socket_transport
agent_optimized_for: Kilo Code
status: draft_handoff
generated: 2026-05-01
---

# Phase ST-7 — Hardening, Documentation, Validation, and Archival

## Goal

Close the `socket-transport` implementation run with deterministic validation, accurate documentation, and archived active plans.

## Primary owner

`@director` for docs/integration review. `@carmack` fixes network/server/test blockers.

## Implementation tasks

### ST-7.1 Protocol and authority audit

Run code review and grep audits for these risks:

- client creates or mutates authoritative gameplay state in remote mode
- client loads gameplay scripts in remote mode
- renderer/audio/HUD/UI marked or treated as authoritative
- input accepted before server welcome
- player id trusted from client rather than assigned by server
- socket tests requiring external network
- blocking loops with no bounded exit in tests
- map mismatch ignored silently
- protocol mismatch ignored silently

Suggested greps:

```bash
git grep -n -i 'client-side gameplay scripting\|client gameplay script\|renderer-owned gameplay\|audio-owned gameplay' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'TODO.*prediction\|TODO.*reconciliation\|predict' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'LocalLoopbackRuntime' -- src/client include/stellar/client tests/client ':!build*/**'
```

Interpretation matters: documented prohibitions and tests may be valid hits. Source authority violations are not.

### ST-7.2 Network robustness audit

Verify:

- malformed packets are rejected with deterministic errors
- oversized packets are rejected before large allocation
- partial reads/writes are handled
- disconnects do not crash
- connection state recovers or fails cleanly
- server validates assigned player id
- full snapshots reset receiver baseline
- deltas require baseline
- gameplay events are queued/drained once
- socket resources close in destructors
- tests do not hang if sockets fail

### ST-7.3 Documentation finalization

Update:

- `docs/ImplementationStatus.md`
  - Add completed socket transport scope section at the top.
  - Record ST-2 through ST-7 completion notes.
  - Include validation commands and results.
  - List known deferred post-ST items.

- `Plans/NEXT.md`
  - Mark socket transport/session lifecycle complete.
  - Point to next recommended options:
    - client interpolation
    - client prediction/reconciliation
    - true multi-player simulation if still deferred
    - UDP/unreliable transport
    - map distribution/caching
    - richer HUD/UI/VFX
    - miniaudio playback
    - sprite atlas/sheet animation

- `docs/Design.md`
  - Describe actual socket transport implementation.
  - State TCP-first or actual chosen protocol honestly.
  - Describe `stellar-server`.
  - Describe `stellar-client --connect`.
  - Clarify what is not implemented.

- Optional `docs/Networking.md`
  - Add only if networking behavior is now detailed enough to deserve a focused doc.
  - Keep it factual and aligned with code.

- `docs/BspAuthoring.md`
  - Update only if server/client workflows changed map validation or presentation-map expectations.

Do not alter `AGENTS.md` without explicit user approval.

### ST-7.4 Plan archival

Move active socket transport plans:

From:

- `Plans/SocketTransport-AgentPlan.md`
- `Plans/ProjectStellar-SocketTransport-AgentPlan.md`

To:

- `Plans/Archived/socket_transport/SocketTransport-AgentPlan.md`
- `Plans/Archived/socket_transport/ProjectStellar-SocketTransport-AgentPlan.md`

If Kilo imported this package into additional `Plans/` files, archive those too under:

- `Plans/Archived/socket_transport/kilo_socket_transport_plans/`

Ensure root `Plans/` contains only current/next active handoff files, not completed ST phase plans.

### ST-7.5 Final validation

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(networked_client_runtime|network_session|socket_transport|dedicated_server|client_connect|client_world_receiver|loopback_transport|snapshot_|bsp_|runtime_world|server_world_session|scripted_world_session|script_command_processor|gameplay_presentation|player_presentation|hud_presentation|audio_event_router)' --output-on-failure
git grep -n -i 'STELLAR_ENABLE_GLTF\|cgltf\|SceneAsset\|gltf' -- . ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'client-side gameplay scripting\|client gameplay script\|renderer-owned gameplay\|audio-owned gameplay' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
git grep -n 'SocketTransport-AgentPlan\|ProjectStellar-SocketTransport-AgentPlan' -- Plans docs ':!Plans/Archived/**'
```

Expected final audit interpretation:

- Retired importer hits should be documentation-only or absent.
- Authority-violation hits should be prohibitions/docs only, not active implementation.
- Socket plan filename search should not find root active plan files after archival, except documented historical references.

### ST-7.6 Final status summary

At the end of implementation, produce a concise status in `docs/ImplementationStatus.md`:

- What was implemented.
- What validation passed.
- What remains deferred.
- Any known manual validation not run.
- Any socket platform limitations.

## Out of scope

- Starting the next implementation scope.
- Adding features to make tests pass if those features belong to deferred work.
- Updating AGENTS coordination rules without explicit user approval.

## Completion checklist

- [ ] ST-2 through ST-6 completion notes exist.
- [ ] Socket transport plans are archived.
- [ ] `Plans/NEXT.md` points to post-ST options.
- [ ] `docs/Design.md` accurately describes implemented networking.
- [ ] `docs/ImplementationStatus.md` is the branch source of truth.
- [ ] Full default CTest passes.
- [ ] Focused ST/BSP/runtime/client/server tests pass.
- [ ] Audits interpreted and recorded.
- [ ] Known deferred work listed honestly.
