# Phase 3 — Z-up Presentation, Camera, Input Mapping, Snapshots, and Network-Adjacent Tests

Branch target: `trenchbroom-compat`

## Goal

Finish the non-authoritative client/presentation side of the Z-up migration. Rendering, HUD, audio, and network snapshots must continue to present server-owned data without becoming gameplay authority.

## Required behavior

- Client camera uses +Z as world up.
- Player eye offset is along +Z.
- Player yaw rotates on the X/Y horizontal plane.
- Input forward/strafe movement maps onto X/Y.
- Billboards use a Z-up camera basis.
- Network snapshots remain data carriers; no new client authority is introduced.
- Remote connect mode behavior remains within completed socket scope.

## Tasks

### 3.1 Player presentation camera

Audit and update:

- camera position from authoritative player center;
- eye height offset;
- camera up vector;
- yaw/pitch basis;
- no-map fallback camera;
- player presentation tests.

Expected policy:

- `camera.position = player.center + world_up * kPlayerEyeHeightInches`
- yaw is horizontal around +Z;
- pitch tilts around camera right;
- forward is horizontal-projected before pitch.

If existing conventions differ, document the chosen convention in `PlayerPresentation` Doxygen comments and tests.

### 3.2 Input mapping

Audit input-to-command mapping:

- W/S movement should drive forward/back on X/Y plane.
- A/D movement should strafe on X/Y plane.
- Jump, if present, should push +Z.
- Crouch/fall/down, if present later, should use -Z.

Do not alter authority: input commands remain requests.

### 3.3 Billboard and gameplay presentation

Audit:

- billboard view generation;
- camera right/up vectors;
- sprite size/origin assumptions;
- sorting by camera depth;
- debug marker orientation.

Z-up change should not alter server authority or metadata ownership.

### 3.4 Network snapshots and receiver tests

Network data likely stores raw position vectors and does not require protocol changes. Still update tests that assert exact coordinates or camera basis.

Audit:

```bash
git grep -n 'NetworkWorldSnapshot\|WorldSnapshot\|GameplayEvent\|ClientWorldReceiver\|PlayerPresentation\|Billboard' -- include src tests ':!build*/**'
```

Rules:

- Do not change packet protocol semantics solely because axes changed.
- Do update expected fixture values and documentation.
- Do not add prediction/reconciliation/interpolation.

### 3.5 HUD/audio checks

HUD/audio should remain presentation caches/routes based on server-approved events. Axis migration should not affect gameplay authority. Update only tests/docs that include coordinates or room examples.

## Files likely changed

- `include/stellar/client/**`
- `src/client/**`
- `include/stellar/graphics/**`
- `src/graphics/**`
- `tests/client/**`
- `tests/graphics/**`
- `tests/network/**`
- `docs/Design.md`

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(player_presentation|gameplay_presentation|render_level|render_level_inspection|client_world_receiver|networked_client_runtime|client_connect|snapshot_|hud_presentation|audio_event_router)' --output-on-failure
```

Broad:

```bash
ctest --test-dir build --output-on-failure
```

Audit:

```bash
git grep -n 'eye height\|camera up\|billboard up\|Y-up\|Y is up\|0 36 0' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
```

## Acceptance criteria

- [ ] Player presentation camera uses +Z up.
- [ ] Input movement maps to X/Y horizontal plane.
- [ ] Billboards face camera using Z-up basis.
- [ ] Snapshot/network tests pass with Z-up expected values.
- [ ] HUD/audio remain presentation-only and authority-neutral.
- [ ] Full CTest passes or remaining failures are isolated to BSP30/toolchain work in later phases.

## Parallelization

Safe workstreams:

- Agent A: player camera and input mapping.
- Agent B: billboard/gameplay presentation.
- Agent C: network snapshot expected values.
- Agent D: HUD/audio doc/test cleanup.

Coordinate on any shared client runtime tests.
