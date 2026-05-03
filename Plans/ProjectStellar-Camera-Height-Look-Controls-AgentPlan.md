# Project Stellar — Camera Height and Look Controls Fix Plan

Branch target: `trenchbroom-compat`

## Purpose

Fix live map play so the camera starts at the intended character eye height and supports turning left/right plus looking up/down. This plan is optimized for AI implementation agents. Do not commit unless explicitly instructed by the user.

## Current user-visible symptoms

- Running from a normal desktop terminal works, but running directly from the VS Code integrated terminal may still fail or behave inconsistently because of display/session environment issues. Treat this as a documentation/diagnostics concern unless reproducible outside VS Code.
- In a compiled TrenchBroom map, the camera appears at or near floor level instead of at the intended 72 inch character height.
- Keyboard movement exists, but there is no camera yaw or pitch control. The player cannot turn left/right or look up/down.

## Key repo evidence and likely root causes

### 1. Camera height contract is internally inconsistent

Relevant files:

- `include/stellar/core/WorldUnits.hpp`
- `include/stellar/client/PlayerPresentation.hpp`
- `src/client/PlayerPresentation.cpp`
- `tests/client/PlayerPresentation.cpp`
- `include/stellar/server/MovementSimulation.hpp`
- `include/stellar/physics/CharacterController.hpp`

Current constants include:

```cpp
kPlayerHeightInches = 72.0F;
kPlayerEyeHeightInches = 64.0F;
```

`PlayerSnapshot::position` and `MovementState::position` are documented and used as authoritative character **center** position, not feet position. TrenchBroom spawn guidance places the default player start at `z = 36` on a floor at `z = 0`, matching a 72 inch capsule center.

Current `PlayerCameraConfig::follow_offset` defaults to:

```cpp
{0.0F, 0.0F, kPlayerEyeHeightInches}
```

That means the current default camera eye should be:

```text
eye_z = character_center_z + 64
```

For the documented spawn center at `z = 36`, that produces `z = 100`, above an 8 foot / 96 inch room. That is not the intended "72 inch character height" camera and indicates the code is mixing "eye height above floor" with "eye offset from character center."

If the user sees floor-level, there may also be a runtime/snapshot path issue, but the first required fix is to make the contract unambiguous and covered by tests.

### 2. No look input reaches the camera path

Relevant files:

- `include/stellar/platform/Input.hpp`
- `src/platform/Input.cpp`
- `include/stellar/client/MovementInputMapper.hpp`
- `src/client/MovementInputMapper.cpp`
- `src/client/NetworkedClientRuntime.cpp`
- `src/client/Application.cpp`
- `include/stellar/server/MovementSimulation.hpp`
- `include/stellar/server/WorldSession.hpp`
- `src/server/WorldSession.cpp`
- `include/stellar/network/Messages.hpp`
- `src/network/SnapshotCodec.cpp`

Current `platform::Input` tracks only key held state. It does not track mouse motion, relative mouse mode, per-frame mouse deltas, or look keys.

Current `MovementInputMapper` converts WASD directly to world axes:

```text
W = +Y
S = -Y
A = -X
D = +X
```

There is no yaw/pitch accumulation, no mouse-look data in `MovementCommand`, and no server-owned view orientation state.

`PlayerSnapshot::rotation` exists and `PlayerPresentation` can compute yaw/pitch camera frames from it, but `WorldSession::make_snapshot()` currently emits identity rotation for the player snapshot every tick. Therefore, camera direction remains fixed unless code elsewhere mutates the snapshot, which it currently does not.

## Target behavior

Implement a minimal FPS-style camera/control contract consistent with server authority:

1. The player spawn remains authored at `origin "0 0 36"` for a 72 inch capsule center on floor `z = 0`.
2. The live camera eye should be at the requested 72 inch character height above the floor unless design chooses a separate eye-height constant.
3. Because `PlayerSnapshot::position` is character center, the default camera follow offset must be:
   - `kPlayerHeightInches * 0.5F` if eye should be at top of the 72 inch capsule, or
   - `kPlayerEyeHeightInches - (kPlayerHeightInches * 0.5F)` if design wants a 64 inch human eye height.
4. For this user report, use `72 inches above floor` as the acceptance target unless the user explicitly selects 64 inches.
5. Mouse relative motion should turn the camera:
   - mouse X: yaw around +Z.
   - mouse Y: pitch around camera right, clamped to a safe range such as `[-89°, +89°]`.
6. Provide keyboard fallback look controls:
   - Left/Right arrows: yaw.
   - Up/Down arrows: pitch.
7. Movement should become camera-relative on the X/Y plane:
   - W moves along camera yaw forward projected onto X/Y.
   - S moves backward.
   - A/D strafe along camera yaw right/left.
   - Pitch must not affect movement direction.
8. Camera orientation should be replicated through the existing player snapshot rotation path so local loopback and remote socket mode behave consistently.
9. Client-side renderer/audio state must not own gameplay truth. The server should own/sanitize the authoritative orientation used in snapshots.

## Implementation phases

### Phase 0 — Reproduce and add diagnostics

1. Build and validate:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build -j$(nproc)
   build/stellar-client --validate-map maps/compiled/test_room.bsp
   ```

2. Run live client from a working desktop terminal:
   ```bash
   build/stellar-client --map maps/compiled/test_room.bsp
   ```

3. Add temporary or permanent debug logging for the first authoritative snapshot and first camera frame:
   - player position
   - player rotation quaternion
   - computed camera eye
   - computed camera target
   - near/far planes
   - whether runtime is local `NetworkedClientRuntime` or remote `RemoteClientRuntime`

4. Suggested permanent option:
   - Add `--debug-camera` or `STELLAR_DEBUG_CAMERA=1`.
   - Log only the first few frames or on state changes.

5. Acceptance:
   - A display-free test can verify camera eye math.
   - A manual run can verify the logged eye height against the map floor and spawn.

### Phase 1 — Correct camera height semantics

1. Update `WorldUnits.hpp` to add explicit derived constants:
   ```cpp
   inline constexpr float kPlayerHalfHeightInches = kPlayerHeightInches * 0.5F;
   inline constexpr float kDefaultCameraEyeHeightInches = kPlayerHeightInches; // 72 per user target
   inline constexpr float kDefaultCameraEyeOffsetFromCenterInches =
       kDefaultCameraEyeHeightInches - kPlayerHalfHeightInches;
   ```

2. Update `PlayerCameraConfig::follow_offset` to use the center-relative offset:
   ```cpp
   {0.0F, 0.0F, kDefaultCameraEyeOffsetFromCenterInches}
   ```

3. Update comments to avoid ambiguity:
   - `PlayerSnapshot::position` is character center.
   - `kDefaultCameraEyeHeightInches` is height above the floor when the player is grounded.
   - `follow_offset` is offset from character center.

4. Update `tests/client/PlayerPresentation.cpp`:
   - Replace expectations that add full `kPlayerEyeHeightInches` to the center.
   - Add a test named like `default_camera_for_spawn_center_is_character_height_above_floor`.
   - Example:
     ```cpp
     player.position = {0.0F, 0.0F, 36.0F};
     frame.eye[2] == 72.0F;
     ```

5. Add a map/runtime presentation test if possible:
   - Build a runtime world from the minimal Z-up room fixture.
   - Obtain the first snapshot.
   - Compute camera frame.
   - Assert floor is `z = 0`, spawn center is `z = 36`, camera eye is `z = 72`.

6. Acceptance:
   - The camera is no longer at floor level.
   - In a 96 inch tall room, the camera is inside the room at `z = 72`.
   - Existing spawn convention `origin "0 0 36"` remains unchanged.

### Phase 2 — Extend input with mouse and look state

1. Update `include/stellar/platform/Input.hpp` and `src/platform/Input.cpp`.

2. Add persistent and per-frame input state:
   ```cpp
   int mouse_delta_x_ = 0;
   int mouse_delta_y_ = 0;
   bool relative_mouse_requested_ = false; // optional, if managed here
   bool keys_pressed_this_frame_[SDL_NUM_SCANCODES] = {};
   ```

3. Handle SDL events:
   - `SDL_MOUSEMOTION`: accumulate `event.motion.xrel` and `event.motion.yrel`.
   - `SDL_KEYDOWN`: set held state and pressed-this-frame state when `event.key.repeat == 0`.
   - `SDL_KEYUP`: clear held state.

4. Expose methods:
   ```cpp
   [[nodiscard]] int mouse_delta_x() const noexcept;
   [[nodiscard]] int mouse_delta_y() const noexcept;
   [[nodiscard]] bool was_key_pressed(SDL_Scancode key) const noexcept;
   ```

5. Update `reset_frame_state()`:
   - clear mouse deltas
   - clear pressed-this-frame flags
   - keep held keys intact

6. Add display-free tests:
   - mouse deltas accumulate for multiple `SDL_MOUSEMOTION` events.
   - reset clears deltas.
   - key held state survives reset.
   - pressed-this-frame clears after reset.

### Phase 3 — Add relative mouse capture and look-toggle policy

1. Decide where to own relative mouse mode:
   - Preferred: `platform::Window` owns SDL calls; `Application` decides policy.

2. Add platform methods:
   ```cpp
   std::expected<void, Error> Window::set_relative_mouse_mode(bool enabled) noexcept;
   [[nodiscard]] bool Window::relative_mouse_mode() const noexcept;
   ```

3. Use SDL:
   ```cpp
   SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE);
   SDL_CaptureMouse(enabled ? SDL_TRUE : SDL_FALSE); // optional, platform-dependent
   ```

4. Application behavior:
   - Enable relative mouse mode when entering live play.
   - `Escape` should first release mouse capture.
   - A second `Escape` or window close should quit.
   - Add a recapture key/mouse click policy, for example:
     - Left mouse click recaptures.
     - `F1` toggles mouse capture.
   - Preserve the current ability to exit quickly.

5. Important: current `Window::handle_event()` closes immediately on ESC. Refactor so Application can decide:
   - Option A: remove ESC close from `Window` and expose key events through `Input`.
   - Option B: keep default ESC close only when relative mouse mode is disabled.
   - Preferred: `Window` should not encode gameplay/app-level key policy.

6. Add tests:
   - Display-free tests for Input state.
   - Window relative mouse mode may need an opt-in SDL/display test; keep default CI display-free.

### Phase 4 — Add server-authoritative look orientation

1. Extend `MovementCommand` or introduce `PlayerViewCommand`.

Recommended minimal change:
```cpp
struct MovementCommand {
    std::array<float, 3> wish_direction{};
    bool jump = false;
    float view_yaw_degrees = 0.0F;
    float view_pitch_degrees = 0.0F;
    bool has_view_angles = false;
};
```

2. Add a deterministic view state to `MovementState` or `WorldSession`:
```cpp
float yaw_degrees = 0.0F;
float pitch_degrees = 0.0F;
```

3. Add sanitization:
   - yaw: finite, normalized to `[0, 360)` or `[-180, 180]`.
   - pitch: finite, clamped to `[-89, 89]`.
   - if not supplied, preserve previous yaw/pitch.
   - no NaN/Inf in snapshots.

4. Convert yaw/pitch to quaternion in `WorldSession::make_snapshot()`.
   - Z-up convention:
     - yaw around +Z
     - pitch around local/camera right
   - Match `PlayerPresentation::z_up_forward_from_rotation()` tests.

5. Update snapshot serialization.
   - `PlayerSnapshot::rotation` is already serialized in `SnapshotCodec.cpp`.
   - If only `MovementCommand` gains view angles, update `write_movement_command()` and `read_movement_command()`.
   - Bump codec version if needed or ensure both client/server in this branch are updated together.
   - Update malformed/trailing-byte tests as needed.

6. Update tests:
   - `MovementSimulation` sanitizes and preserves look fields if placed there.
   - `WorldSession` accepts look command and emits non-identity player rotation.
   - `SnapshotCodec` round-trips view angles in player commands.
   - `NetworkedClientRuntime` sends look state and receives rotated snapshots.
   - `RemoteClientRuntime` also sends look state.

### Phase 5 — Add look input mapper and camera-relative movement

1. Add new types:
```cpp
struct LookInputBindings {
    SDL_Scancode yaw_left = SDL_SCANCODE_LEFT;
    SDL_Scancode yaw_right = SDL_SCANCODE_RIGHT;
    SDL_Scancode pitch_up = SDL_SCANCODE_UP;
    SDL_Scancode pitch_down = SDL_SCANCODE_DOWN;
    SDL_Scancode toggle_mouse_capture = SDL_SCANCODE_F1;
};

struct LookInputMapperConfig {
    float mouse_sensitivity_degrees_per_pixel = 0.08F;
    float keyboard_yaw_degrees_per_second = 120.0F;
    float keyboard_pitch_degrees_per_second = 90.0F;
    float min_pitch_degrees = -89.0F;
    float max_pitch_degrees = 89.0F;
    LookInputBindings bindings{};
};
```

2. Add a client-side accumulator:
```cpp
struct ClientViewState {
    float yaw_degrees = 0.0F;
    float pitch_degrees = 0.0F;
};
```

3. Place accumulator in:
   - `NetworkedClientRuntime` for local loopback.
   - `RemoteClientRuntime` for socket client.
   - Or shared helper owned by both runtimes.

4. Update each runtime `update(input, delta_seconds)`:
   - accumulate mouse and keyboard look.
   - clamp pitch.
   - normalize yaw.
   - send view angles with command.
   - map movement relative to current yaw:
     - forward = yaw direction on X/Y
     - right = perpendicular on X/Y
     - wish = forward/back + right/left
   - pitch should not affect movement.

5. Update `MovementInputMapper`:
   - either add overload that accepts yaw:
     ```cpp
     make_movement_command(input, view_state, config)
     ```
   - or add a separate helper:
     ```cpp
     make_camera_relative_movement_command(input_state, yaw_degrees, config)
     ```

6. Tests:
   - yaw 0: W moves +Y if that remains world forward.
   - yaw 90: W moves +X or -X depending chosen yaw convention; document and test.
   - pitch does not change movement vector.
   - mouse X changes yaw.
   - mouse Y changes pitch with inversion policy documented.
   - pitch clamps at ±89.
   - diagonal movement remains normalized.

### Phase 6 — Presentation camera consumes authoritative rotation

1. Keep `PlayerPresentation` consuming `PlayerSnapshot::rotation`.
2. After Phase 4, snapshots should contain yaw/pitch-driven rotation.
3. Ensure `Application.cpp` does not directly apply client-local camera state to rendering except through latest authoritative snapshot.
4. If latency feels poor in local mode, do not add prediction in this fix unless explicitly scoped. For local loopback, authoritative snapshot should arrive every frame anyway.

5. Tests:
   - Given player snapshot with yaw/pitch rotation, camera target changes correctly.
   - Given live `NetworkedClientRuntime` after a look input, latest snapshot rotation is non-identity and camera target changes.
   - Remote runtime command encoding includes look state.

### Phase 7 — VS Code terminal launch caveat and diagnostics

1. Document that VS Code integrated terminal can lack the same `DISPLAY`, `WAYLAND_DISPLAY`, `XAUTHORITY`, `XDG_RUNTIME_DIR`, or session permissions as a normal desktop terminal.
2. Add a troubleshooting section to `docs/TrenchBroom.md` or a dedicated runtime launch doc:
   ```bash
   echo "$DISPLAY"
   echo "$WAYLAND_DISPLAY"
   echo "$XAUTHORITY"
   echo "$XDG_RUNTIME_DIR"
   build/stellar-client --validate-map maps/compiled/test_room.bsp
   build/stellar-client --map maps/compiled/test_room.bsp
   ```
3. Include suggested launch methods:
   - run from a normal desktop terminal.
   - launch VS Code from the working desktop terminal so it inherits the environment:
     ```bash
     code .
     ```
   - configure VS Code `terminal.integrated.env.linux` only if needed.
4. Also complete the prior SDL diagnostics improvement:
   - include `SDL_GetError()` in `Window::create()` failures.
   - include `SDL_GetError()` in `SDL_CreateWindow` and `SDL_GL_CreateContext` failures.
   - preserve the high-level error prefix.

## Acceptance criteria

### Display-free automated acceptance

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R 'player_presentation|client_movement_input_mapper|networked_client_runtime|client_connect|snapshot_codec|server_world_session|movement_simulation|render_level_inspection|input' --output-on-failure
ctest --test-dir build --output-on-failure
```

Required assertions:

- `info_player_start origin "0 0 36"` implies a camera eye at `z = 72` for the current user target.
- Player snapshot rotation changes after look input.
- Mouse and keyboard look input modify yaw/pitch.
- Pitch clamps safely.
- WASD movement becomes camera-relative on the X/Y plane.
- Snapshot/command serialization round-trips look fields.
- No client-side renderer/audio state becomes authoritative gameplay state.

### Manual acceptance

1. Compile a known test map:
   ```bash
   tools/bsp/compile_trenchbroom_bsp30.sh \
     --map tests/fixtures/trenchbroom/src/minimal_zup_room.map \
     --out maps/compiled/minimal_zup_room.bsp \
     --profile fast
   ```

2. Launch:
   ```bash
   build/stellar-client --map maps/compiled/minimal_zup_room.bsp
   ```

3. Verify:
   - camera begins at human/player height, not on the floor.
   - camera is inside a 96 inch tall room, not above the ceiling.
   - moving mouse left/right turns view left/right.
   - moving mouse up/down looks up/down.
   - arrow keys also look if mouse capture is not available.
   - W moves in the direction the camera is facing on the floor plane.
   - ESC/F1 behavior for mouse capture is understandable and documented.
   - `--validate-map` still exits without creating a window.

## Suggested implementation order for agents

1. Add debug logging behind `STELLAR_DEBUG_CAMERA=1`.
2. Fix camera height constants and tests.
3. Extend `Input` with mouse deltas and per-frame key presses.
4. Add relative mouse mode support and capture policy.
5. Add look state and input mapper helpers.
6. Extend command/snapshot/server orientation path.
7. Make movement camera-relative.
8. Update render/client runtime tests.
9. Update docs for controls and VS Code terminal caveat.
10. Run full CTest and manual map smoke.

## Do not do in this slice

- Do not add client-side prediction/reconciliation.
- Do not add free-fly/no-clip camera as the default player camera.
- Do not move gameplay authority into renderer or audio layers.
- Do not change the TrenchBroom spawn convention from `z = 36`.
- Do not make pitch affect horizontal movement.
- Do not require display/GPU tests in default CI.
