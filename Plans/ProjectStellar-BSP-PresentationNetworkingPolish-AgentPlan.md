# Project Stellar — BSP Presentation and Networking Polish Implementation Plan

Prepared for implementation agents working on `JTM-rootstorm/Project-Stellar`, branch
`bsp-gameplay-loop`.

This detailed active handoff is derived from the phase plan package under
`Plans/kilo_bsp_gameplay_followup_plans/`. The completed BSP gameplay-loop plans are archived at
`Plans/Archived/bsp_gameplay_loop/` and must not be restarted.

## Global invariants

- Server authority remains mandatory.
- Client-side gameplay scripting is forbidden.
- Lua remains server-authoritative and sandboxed.
- BSP import never executes scripts.
- Rendering, audio, and HUD state are presentation only.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable through shared graphics abstractions.
- BSP remains the canonical playable level format.
- Do not reintroduce retired pre-BSP importer functionality.

## Agent routing

- `@director`: phase sequencing, integration decisions, docs/archive/final validation.
- `@carmack`: server authority, client runtime seam, snapshot/delta contracts, networking,
  build/tests.
- `@miyamoto`: graphics presentation, billboard/entity rendering, render inspection tests.
- `@kojima`: gameplay semantics for pickups, interaction feedback, HUD/inventory meaning.
- `@suzuki`: audio event presentation and no-op fallback when audio polish is implemented.

Specialists must not create subagents or delegate. Cross-domain blockers escalate to `@director`.

## Phase order and dependencies

```text
Phase PN-0: Archive old plans + publish active follow-up plan docs
    ↓
Phase PN-1: Live scripted authoritative runtime integration
    ↓
Phase PN-2: Authoritative gameplay snapshot presentation
    ↘
     Phase PN-3: Snapshot/delta/event transport contracts
          ↓
     Phase PN-4: Local/remote transport bridge and client/server message path
          ↓
Phase PN-5: HUD/audio/toolchain polish over server-approved events
    ↓
Phase PN-6: Final documentation, archive hardening, validation, and NEXT.md handoff
```

PN-2 and PN-3 may overlap after PN-1 exposes stable `WorldSnapshot` and
`GameplayWorldSnapshot` data to the client frame loop. PN-5 visual-only work may begin after PN-2;
event-driven HUD/audio work should wait for PN-3 event types.

## Phase PN-0 — Archive and handoff

Owner: `@director`.

Scope:

- Move completed root-level gameplay-loop plans to `Plans/Archived/bsp_gameplay_loop/`.
- Publish this active plan and the concise handoff.
- Update `Plans/NEXT.md`, `docs/ImplementationStatus.md`, `docs/Design.md`, and
  `docs/BspAuthoring.md` so agents do not restart completed gameplay-loop work.

Validation:

```bash
git status --short
git diff -- Plans docs
git grep -n 'BspGameplayLoop-AgentPlan\|ProjectStellar-BSP-GameplayLoop-AgentPlan' -- Plans docs ':!Plans/Archived/**'
```

Acceptance:

- Old gameplay-loop plan filenames appear only as archive/historical references, not active entry
  points.
- No source behavior changes.
- `AGENTS.md` unchanged.

## Phase PN-1 — Live scripted authoritative runtime integration

Owner: `@carmack`; coordinator/reviewer: `@director`.

Primary files:

- `include/stellar/client/Application.hpp`
- `include/stellar/client/ApplicationConfig.hpp`
- `src/client/Application.cpp`
- `src/client/ApplicationConfig.cpp`
- `include/stellar/client/LocalLoopbackRuntime.hpp`
- `src/client/LocalLoopbackRuntime.cpp`
- `include/stellar/scripting/ScriptedWorldSession.hpp`
- `src/scripting/ScriptedWorldSession.cpp`
- `include/stellar/scripting/ScriptRegistry.hpp`
- `include/stellar/server/WorldSession.hpp`
- `include/stellar/server/GameplayWorld.hpp`

Implementation direction:

- Keep the public local runtime shape: `latest_snapshot()` and `update(input, delta_seconds)`.
- Internally support plain `server::WorldSession` for maps without scripts and
  `scripting::ScriptedWorldSession` for maps with trigger/object-collider script bindings.
- Add script-source loading for authoritative runtime preparation. Resolve script IDs against an
  explicit script root when configured, otherwise the map directory. Reject absolute paths and parent
  escapes after normalization.
- Missing script source is a deterministic startup/runtime preparation error for maps that reference
  scripts.
- Surface script events, script errors, command results, and whether the frame was scripted in
  `LocalLoopbackFrameResult`.

Validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar-client -j$(nproc)
cmake --build build --target \
  stellar_client_local_loopback_runtime_test \
  stellar_client_map_validation_smoke \
  stellar_scripted_world_session_test \
  stellar_script_command_processor_test \
  stellar_bsp_playable_world_smoke_test \
  -j$(nproc)
ctest --test-dir build -R '^(client_local_loopback_runtime|client_map_validation_smoke|client_cli_map_validation|scripted_world_session|script_command_processor|bsp_playable_world_smoke|bsp_scripted_playable_world_smoke|bsp_scripted_collision_smoke|bsp_scripted_object_collider_smoke)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Acceptance:

- No-script BSP client path still works.
- Script-bound maps construct a scripted authoritative runtime.
- Live client frame updates can produce script events/errors/command results.
- Pickup and gate/door interactions in scripted mode update authoritative snapshots.
- No client-side gameplay scripting is added.

## Phase PN-2 — Authoritative gameplay snapshot presentation

Owners: `@miyamoto` for graphics/presentation, `@carmack` for runtime data support,
`@kojima` for gameplay semantics, coordinated by `@director`.

Primary files:

- `include/stellar/server/GameplayWorld.hpp`
- `include/stellar/server/WorldSession.hpp`
- `include/stellar/graphics/BillboardSprite.hpp`
- `include/stellar/graphics/RenderLevel.hpp`
- `include/stellar/graphics/LevelRenderer.hpp`
- `src/graphics/LevelRenderer.cpp`
- `src/client/Application.cpp`

Implementation direction:

- Add a client presentation adapter such as `GameplayPresentation` that converts authoritative
  `WorldSnapshot`/`GameplayWorldSnapshot` data into backend-neutral `BillboardSprite` draw data.
- Present active sprite entities and active pickups. Hide inactive/collected pickups.
- Optionally present door/gate state as debug/status markers when configured.
- Extend `LevelRenderer` with presentation state without making graphics depend on client code.
- Feed presentation state in the live client frame loop after authoritative runtime update.

Validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
  stellar_player_presentation_test \
  stellar_render_level_inspection_test \
  stellar_graphics_backend_selection_test \
  stellar-client \
  -j$(nproc)
ctest --test-dir build -R '^(player_presentation|render_level_inspection|graphics_backend_selection|gameplay_presentation)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Acceptance:

- Server-owned gameplay snapshots drive live presentation.
- Active sprite/pickup entities produce billboards.
- Collected/inactive pickups stop presenting.
- Door/gate state can be observed without moving brush simulation.
- Renderer and client remain presentation-only.

## Phase PN-3 — Snapshot, delta, and event transport contracts

Owner: `@carmack`; gameplay semantics review: `@kojima`; coordinator: `@director`.

Primary files:

- `include/stellar/network/Messages.hpp`
- `include/stellar/network/SnapshotCodec.hpp`
- `include/stellar/network/SnapshotDelta.hpp`
- `src/network/SnapshotCodec.cpp`
- `src/network/SnapshotDelta.cpp`
- `tests/network/SnapshotCodec.cpp`
- `tests/network/SnapshotDelta.cpp`

If an existing network/serialization layer exists, integrate there rather than duplicating it.

Implementation direction:

- Define deterministic serializable contracts for client input commands, server snapshots, gameplay
  entity state, server-approved gameplay/presentation events, and structural snapshot deltas.
- Use stable entity/player IDs and deterministic ordering by ID.
- Implement a narrow deterministic codec with explicit endian handling, bounded strings/vectors,
  finite float validation, `std::expected` decode failures, and no external dependency if no codec
  exists.
- Add conversion helpers from runtime/script frame results to server-authored events such as pickup
  collected, door state changed, script command applied, and script error.

Validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
  stellar_snapshot_codec_test \
  stellar_snapshot_delta_test \
  stellar_server_world_session_test \
  stellar_scripted_world_session_test \
  -j$(nproc)
ctest --test-dir build -R '^(snapshot_codec|snapshot_delta|server_world_session|scripted_world_session)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Acceptance:

- `WorldSnapshot` converts to a transport-ready snapshot.
- Snapshot codec round-trips deterministically.
- Applying a delta to its baseline produces the target snapshot exactly in tests.
- Server-approved gameplay events are representable without renderer/audio dependencies.
- No remote transport or prediction is added prematurely.

## Phase PN-4 — Local/remote transport bridge

Owner: `@carmack`; coordinator/reviewer: `@director`; presentation integration review by
`@miyamoto` only where client presentation hookup is affected.

Primary files:

- `include/stellar/network/Transport.hpp`
- `src/network/LoopbackTransport.cpp`
- `include/stellar/network/LocalServerBridge.hpp` or `include/stellar/client/LocalServerBridge.hpp`
- `include/stellar/client/ClientWorldReceiver.hpp`
- tests under `tests/network/` and `tests/client/`

Implementation direction:

- Add a transport-neutral path where client input commands serialize to transport packets, a local
  authoritative server adapter consumes commands, and server snapshots/deltas/events flow back to a
  client receiver.
- In-memory loopback transport is required; real sockets are optional and may remain deferred.
- The client receiver applies full snapshots and deltas, exposes the latest authoritative snapshot,
  and queues events for HUD/audio presentation. No prediction in this phase.

Validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
  stellar_loopback_transport_test \
  stellar_client_world_receiver_test \
  stellar_snapshot_codec_test \
  stellar_snapshot_delta_test \
  -j$(nproc)
ctest --test-dir build -R '^(loopback_transport|client_world_receiver|snapshot_codec|snapshot_delta)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Acceptance:

- There is a transport-neutral path for input commands and server snapshots/events.
- Local loopback exercises the same message contracts planned for remote play.
- Client presentation can consume receiver snapshots/events.
- No client-side authority or prediction is introduced.

## Phase PN-5 — HUD, audio event, and BSP toolchain polish

Owners: `@kojima` for HUD/gameplay feedback, `@suzuki` for audio events, `@director` for BSP
toolchain/docs coordination.

Implementation direction:

- Add small presentation-only HUD/inventory data feedback from server-approved events where selected.
- Add audio event routing over server-approved events with explicit no-op fallback where selected.
- Add BSP authoring/toolchain definitions such as `tools/bsp/stellar_entities.fgd` where selected.
- Keep all slices optional/polish-sized and do not block core runtime/networking stabilization.

Validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(hud_presentation|audio_event_router|client_world_receiver|gameplay_presentation|bsp_authoring_smoke)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Acceptance:

- HUD/inventory feedback, if implemented, is client presentation cache only.
- Audio event routing, if implemented, is presentation-only and supports no-op fallback.
- BSP authoring docs/tool definitions reflect current script/runtime/presentation behavior.
- No gameplay authority moves into renderer, audio, HUD, or client scripts.

## Phase PN-6 — Final documentation, archive hardening, and validation

Owner: `@director`; specialist review from relevant owners.

Implementation direction:

- Update `docs/ImplementationStatus.md` with exact phase completion status, commands run, and results.
- Update `Plans/NEXT.md` to the next active scope if PN work is complete; otherwise keep it pointed at
  remaining PN phases.
- Update `docs/Design.md` and `docs/BspAuthoring.md` to match implemented behavior without
  overclaiming remote multiplayer, audio, HUD rendering, prediction, or interpolation.
- Archive the PN plan files under `Plans/Archived/bsp_presentation_networking_polish/` only after the
  PN scope is complete and NEXT points elsewhere.

Final validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(bsp_|render_level|runtime_world|client_|player_presentation|gameplay_presentation|server_world_session|scripted_world_session|trigger_script|object_collider_script|script_command_processor|snapshot_|loopback_transport|client_world_receiver|world_metadata_validation|collision_validation|character_controller|hud_presentation|audio_event_router)' --output-on-failure
git grep -n -i 'STELLAR_ENABLE_GLTF\|cgltf\|SceneAsset\|gltf' -- . ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'client-side gameplay scripting\|client gameplay script\|renderer-owned gameplay\|audio-owned gameplay' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
git grep -n 'BspGameplayLoop-AgentPlan\|ProjectStellar-BSP-GameplayLoop-AgentPlan' -- Plans docs ':!Plans/Archived/**'
```

Final acceptance:

- Documentation matches implemented reality.
- Old completed plans are archived.
- Active PN plans are either archived after completion or clearly remain active in root `Plans/`.
- Validation commands/results are recorded.
- Known deferrals are explicit.
