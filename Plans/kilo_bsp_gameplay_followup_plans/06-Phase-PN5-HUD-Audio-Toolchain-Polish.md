# Phase PN-5 — HUD, Audio Event, and BSP Toolchain Polish

Optimized for Kilo Code intake.

## Owner

- HUD/gameplay feedback: `@kojima` with `@miyamoto` if rendering/UI hooks are needed
- Audio events: `@suzuki`
- BSP toolchain/docs: `@director` coordinating `@kojima` and `@carmack`
- Overall sequencing: `@director`

## Goal

Add lightweight player-facing feedback over the authoritative snapshot/event work without expanding gameplay authority or taking on full UI/audio/editor systems.

This phase is intentionally polish-sized. It should not block core runtime/networking if those phases are still being stabilized.

## Dependencies

- Phase PN-1 for scripted live runtime.
- Phase PN-2 for gameplay presentation state.
- Phase PN-3 for server-approved event contract.
- Phase PN-4 if using transport receiver events for live/local presentation.

## Scope

Implement one or more small feedback/tooling slices:

1. Pickup HUD/inventory feedback.
2. Audio event presentation for pickup and door/gate events.
3. BSP authoring/toolchain definitions and examples.

Keep everything presentation-only.

## Slice A — Pickup HUD/inventory feedback

### Recommended design

Add a tiny client-side presentation model, not authoritative inventory.

Suggested files:

```text
include/stellar/client/HudPresentation.hpp
src/client/HudPresentation.cpp
tests/client/HudPresentation.cpp
```

Possible API:

```cpp
struct HudPresentationState {
    std::uint32_t collected_pickup_count = 0;
    std::vector<std::string> recent_messages;
};

void apply_gameplay_event(HudPresentationState& state,
                          const network::GameplayEvent& event);
```

Rules:

- Server event/snapshot drives HUD state.
- HUD state is disposable presentation cache.
- No gameplay logic reads from HUD state.
- If no text rendering exists, keep it data-only with display-free tests and document rendering as deferred.

### Tests

- Pickup collected event increments count.
- Duplicate event sequence handling is deterministic.
- Door/gate event adds a recent message only if desired.
- HUD state reset on new map/session.

## Slice B — Audio event presentation

### Recommended design

Add an audio event router that consumes server-approved events and invokes existing audio/no-op device APIs if present.

Suggested files:

```text
include/stellar/audio/AudioEventRouter.hpp
src/audio/AudioEventRouter.cpp
tests/audio/AudioEventRouter.cpp
```

If no audio device exists yet, add only a no-op/test fake interface. Do not wire miniaudio deeply unless the project already has that foundation.

Rules:

- Audio never affects gameplay state.
- Missing audio assets should not fail gameplay.
- No-op audio path must be explicit and testable.
- Keep event mapping data-driven or small/static.

Event examples:

```text
PickupCollected -> one-shot "pickup"
DoorStateChanged/open -> one-shot "door_open"
DoorStateChanged/closed -> one-shot "door_close"
ScriptError -> optional debug beep/log only, not gameplay
```

### Tests

- Event maps to expected audio request.
- No-op device accepts events without error.
- Missing sound id is reported as presentation diagnostic only.

## Slice C — BSP toolchain/editor workflow polish

### Recommended design

Add docs and optional editor definition assets for known Stellar BSP keys.

Potential files:

```text
docs/BspAuthoring.md
tools/bsp/stellar_entities.fgd
tests/tools/...
```

If adding `tools/bsp/stellar_entities.fgd`, include definitions for:

- `info_player_start`
- `info_stellar_spawn`
- `trigger_stellar`
- `stellar_object_collider`
- `stellar_sprite`
- static collision brush notes
- keys:
  - `stellar.script`
  - `stellar.table`
  - `stellar.extents`
  - `stellar.sprite`
  - `stellar.texture`
  - `stellar.size`
  - `stellar.alpha`
  - `stellar.collider`
  - `stellar.enabled`
  - `stellar.collision`

Document script source resolution from Phase PN-1.

### Tests

A full editor test is not required. Add a simple text/fixture validation only if the repo already has tooling tests.

## Validation commands

Run focused tests for whichever slices are implemented:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

ctest --test-dir build -R '^(hud_presentation|audio_event_router|client_world_receiver|gameplay_presentation|bsp_authoring_smoke)$' --output-on-failure
```

Then:

```bash
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- HUD/inventory feedback, if implemented, is client presentation cache only.
- Audio event routing, if implemented, is presentation only and supports no-op fallback.
- BSP authoring docs/tool definitions reflect current script/runtime/presentation behavior.
- No gameplay authority moved into renderer, audio, HUD, or client scripts.
