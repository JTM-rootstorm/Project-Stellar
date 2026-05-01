# Project Stellar — BSP Gameplay Loop Implementation Plan

Prepared for AI implementation agents working on `JTM-rootstorm/Project-Stellar`, branch `bsp-gameplay-loop`.

Do not commit directly to the branch unless the user explicitly asks. This file is a planning handoff.

---

## 0. Branch posture and evidence

### Active branch goal

`Plans/NEXT.md` and `docs/ImplementationStatus.md` agree that the active scope is gameplay loop expansion over BSP maps. The branch should build on the completed collision, movement, trigger, object-collider, Lua scripting, BSP canonical migration, BSP rendering, and BSP hardening work. Do not restart those foundations.

### Required active focus areas

Implement the next gameplay loop around:

1. ECS/entity spawn from BSP metadata.
2. Player presentation from authoritative snapshots.
3. Sprite, animation, and interaction loop.
4. Item pickup and scripted doors/gates through the existing sandboxed Lua command path.

### Existing foundations to reuse

- `stellar::server::WorldSession` already owns authoritative player movement state, trigger events, object-collider events, runtime collision state, and player snapshots.
- `stellar::client::LocalLoopbackRuntime` already maps platform input into authoritative `PlayerCommand` data and advances a local server session by fixed ticks.
- `stellar::client::PlayerPresentation` already extracts a player presentation state and follow camera frame from authoritative snapshots.
- `stellar::graphics::RenderLevel` and `stellar::graphics::LevelRenderer` already render BSP/`LevelAsset` geometry and can submit billboards.
- BSP import/validation and generated BSP fixtures already exist, but the current synthetic fixture uses tiny meter-like dimensions and should not become the standard gameplay-room scale.

### Current gap summary

The next implementation should not begin by adding another movement or collision system. The main gap is integration: `stellar-client` validates BSP maps, but the live application path does not yet carry the loaded `LevelAsset` into the renderer, build a `RuntimeWorld`, instantiate `LocalLoopbackRuntime`, advance authoritative ticks from input, or render from the authoritative player camera. The current unit defaults also read as meters (`height = 1.8`, camera offset `2/6`) and must be converted to the requested world rule: **1 world unit = 1 inch**.

---

## 1. Global invariants

- BSP remains the canonical playable level format.
- Server authority remains mandatory.
- Lua scripting remains mandatory and sandboxed.
- Runtime collision, movement, triggers, object colliders, and scripting remain backend-neutral.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable through the shared graphics abstraction.
- Renderer/audio must not become gameplay authority.
- Do not add Source/VBSP, dynamic rigid bodies, third-party physics, full PBR, client-side gameplay scripting, retired importer functionality, or model/skinning/animation systems unless explicitly requested.
- Public APIs require Doxygen `@brief`.
- Keep data plain and serializable where practical.
- Update `docs/ImplementationStatus.md` after each implemented phase.
- Update `docs/Design.md` and `docs/BspAuthoring.md` when the unit policy or authoring conventions change.
- Do not modify `AGENTS.md` or `.kilo/agents/*.md` without explicit user approval.

---

## 2. Unit policy

### Decision

Adopt this as a branch-wide gameplay convention:

```text
1 Stellar gameplay world unit = 1 inch
Y is up
BSP authored coordinates are imported without scale conversion
Player capsule center spawn y should be half the capsule height above the floor
```

### Initial physical tuning

Use these defaults unless implementation discovers a collision-specific reason to tune them slightly:

```cpp
inline constexpr float kUnitsPerInch = 1.0F;
inline constexpr float kInchesPerFoot = 12.0F;
inline constexpr float kPlayerHeightInches = 72.0F;
inline constexpr float kPlayerRadiusInches = 16.0F;
inline constexpr float kPlayerEyeHeightInches = 64.0F;
inline constexpr float kPlayerStepHeightInches = 18.0F;
inline constexpr float kPlayerSkinWidthInches = 0.5F;
inline constexpr float kGroundSnapDistanceInches = 4.0F;
inline constexpr float kWalkSpeedInchesPerSecond = 160.0F;
inline constexpr float kAccelerationInchesPerSecondSquared = 1200.0F;
inline constexpr float kGravityInchesPerSecondSquared = 800.0F;
inline constexpr float kTerminalFallSpeedInchesPerSecond = 2400.0F;
```

Rationale: the old defaults are meter-like and would make a 1-inch unit world feel unusable. These values are intentionally game-feel defaults rather than real-world simulation.

---

## 3. Recommended phase order

```text
Documentation/unit lane:
  Phase 0 -> Phase 1

Asset/tooling lane:
  Phase 2 can run after Phase 1 policy is accepted

Client loop lane:
  Phase 3 -> Phase 4 -> Phase 6

Gameplay entity lane:
  Phase 5 -> Phase 6 -> Phase 7

Final integration:
  Phase 8 last
```

### Safe parallelization

- Phase 1 and Phase 2 can run in parallel after both agree on the inch unit convention.
- Phase 3 can run in parallel with Phase 2 if it does not touch BSP fixture generation or material fallback code.
- Phase 4 should wait for Phase 3 because it depends on live runtime/session integration.
- Phase 5 can begin after Phase 1, but Phase 6 should wait for Phases 3, 4, and 5.
- Phase 7 should wait until Phase 6 has a controllable single-room loop.
- Phase 8 should run last.

---

## Phase 0 — Active gameplay-loop handoff lock-in

### Task card

- Branch: `bsp-gameplay-loop`
- Primary agent: `@director`
- Type: planning/docs alignment
- Depends on: current branch docs
- Can run in parallel with: no code implementation
- Must finish before: broad implementation begins

### Objective

Create or update an active implementation plan under `Plans/` for the BSP gameplay loop so agents stop treating old collision/BSP hardening plans as the current handoff.

### Required reading

- `Plans/NEXT.md`
- `docs/ImplementationStatus.md`
- `docs/Design.md`
- `docs/BspAuthoring.md`
- `AGENTS.md`
- this downloaded plan

### Implementation steps

1. Add a current plan file under `Plans/`, for example:
   - `Plans/BspGameplayLoop-AgentPlan.md`
2. Keep `Plans/NEXT.md` short and pointed at `docs/ImplementationStatus.md` plus the active gameplay-loop plan.
3. Add a new `docs/ImplementationStatus.md` section for the BSP gameplay loop phases.
4. Record that 1 unit = 1 inch is the user-requested branch convention.

### Acceptance criteria

- Active docs point to gameplay-loop implementation, not archived collision/BSP plans.
- No source behavior changes are introduced.
- The plan clearly states non-goals and phase ordering.

### Validation

```bash
git diff -- Plans/NEXT.md docs/ImplementationStatus.md docs/Design.md docs/BspAuthoring.md Plans/
```

---

## Phase 1 — Inch-based world scale and gameplay tuning

### Task card

- Branch: `bsp-gameplay-loop`
- Primary agent: `@carmack`
- Review: `@kojima` for gameplay tuning, `@director` for architecture
- Type: backend-neutral gameplay/world constants
- Depends on: Phase 0 or equivalent active plan agreement
- Can run in parallel with: Phase 2 if files do not overlap
- Must finish before: Phase 4 and Phase 6

### Objective

Make the unit system explicit and convert default movement/capsule/camera values to the user-requested convention: 1 unit = 1 inch.

### Preferred files

New:

- `include/stellar/core/WorldUnits.hpp` or `include/stellar/world/WorldUnits.hpp`
- `tests/world/WorldUnits.cpp` or incorporate constants coverage into movement/presentation tests

Update:

- `include/stellar/physics/CharacterController.hpp`
- `include/stellar/server/MovementSimulation.hpp`
- `include/stellar/client/PlayerPresentation.hpp`
- `tests/physics/CharacterController.cpp`
- `tests/server/MovementSimulation.cpp`
- `tests/client/PlayerPresentation.cpp`
- `docs/Design.md`
- `docs/BspAuthoring.md`
- `docs/ImplementationStatus.md`

### Implementation steps

1. Add unit constants.
   - Prefer a small header with no dependencies.
   - Keep constants `constexpr float` and Doxygen-documented.
   - Include helpers only if they are display-free and trivial, such as `feet_to_units(float)`.
2. Update default `CharacterControllerConfig` values.
   - `radius = 16.0F`
   - `height = 72.0F`
   - `skin_width = 0.5F`
   - `step_height = 18.0F`
   - `ground_snap_distance = 4.0F`
   - Preserve `max_slope_degrees` and `max_slide_iterations` unless tests expose a need.
3. Update default `MovementSimulationConfig` values.
   - `max_speed = 160.0F`
   - `acceleration = 1200.0F`
   - `gravity = 800.0F`
   - `terminal_fall_speed = 2400.0F`
   - Preserve fixed tick at `1.0F / 60.0F`.
4. Update default `PlayerCameraConfig` values.
   - Follow-camera offset should be inch-based, for example `{0.0F, 72.0F, 144.0F}` for a third-person debug camera, or another documented debug value.
   - Look-at offset should target eye/chest height, for example `{0.0F, 60.0F, 0.0F}`.
   - Far plane should be scaled for room/debug maps, for example `4096.0F`.
5. Keep tests deterministic.
   - Tests requiring small synthetic geometry should explicitly override radius/height/speed values rather than relying on inch defaults.
   - Add tests that assert the default player height/radius/speed/camera offsets are inch-scale.
6. Update docs.
   - `docs/Design.md`: add the unit convention to project overview or world authoring.
   - `docs/BspAuthoring.md`: state that BSP coordinates are inches and show spawn center height examples.

### Acceptance criteria

- A new contributor can find the unit policy in code and docs.
- Default player/controller/camera values no longer imply meter-scale gameplay.
- Existing synthetic tests pass by overriding tiny test values where needed.
- No importer scale conversion is introduced.

### Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
  stellar_character_controller_test \
  stellar_server_movement_simulation_test \
  stellar_player_presentation_test -j$(nproc)
ctest --test-dir build -R '^(character_controller|server_movement_simulation|player_presentation)$' --output-on-failure
```

---

## Phase 2 — Procedural developer textures for inch-scale BSP authoring

### Task card

- Branch: `bsp-gameplay-loop`
- Primary agent: `@miyamoto`
- Consult: `@carmack` for asset/import boundaries, `@director` for authoring policy
- Type: import/render asset support and tooling
- Depends on: Phase 1 unit policy
- Can run in parallel with: Phase 3 if renderer/application files are not shared
- Must finish before: final single-room visual acceptance

### Objective

Provide deterministic developer textures that make inch/foot scale readable in a BSP room without requiring external WAD tooling.

### Recommended texture naming convention

Support known BSP texture names even when no WAD or embedded miptex pixels are available:

```text
stellar_dev_grid_12      // 1-foot major grid, 1-inch authored units
stellar_dev_grid_16      // 16-inch tile/checker
stellar_dev_grid_32      // 32-inch tile/checker
stellar_dev_grid_64      // 64-inch tile/checker
stellar_dev_player_72    // 72-inch player-height reference strip
stellar_dev_wall_96      // 8-foot wall-height reference strip
```

If slash-style names are supported safely by the importer/toolchain, aliases may also be accepted:

```text
dev/grid_12
dev/grid_16
dev/grid_32
dev/grid_64
dev/player_72
dev/wall_96
```

### Preferred implementation approach

Generate textures procedurally during BSP import/material fallback, not as binary committed image assets. This keeps tests deterministic and avoids adding external asset dependencies.

Preferred files:

- `include/stellar/import/bsp/DeveloperTextures.hpp` or private `src/import/bsp/DeveloperTextures.*`
- `src/import/bsp/BspLevelBuilder.cpp`
- `tests/import/bsp/BspMaterials.cpp`
- `docs/BspAuthoring.md`

### Implementation steps

1. Add a small procedural developer texture resolver.
   - Input: BSP texture/source material name.
   - Output: optional `ImageAsset`, `TextureAsset`, `SamplerAsset`, and `LevelSurfaceMaterial` binding.
   - Use `ImageFormat::kR8G8B8A8` unless the current renderer path strongly prefers RGB.
   - Use nearest filtering by default so grid lines remain crisp.
2. Integrate into BSP material fallback.
   - If a texture name is one of the developer names, generate a deterministic texture instead of the generic missing-material fallback.
   - Preserve original `source_name` for diagnostics.
   - Do not require WAD files.
3. Add tests.
   - Known developer names produce images/textures/material bindings.
   - Unknown missing texture still uses existing deterministic fallback.
   - Generated images have stable dimensions and non-empty pixels.
   - Sampler uses repeat wrapping so BSP texcoords show repeated scale marks.
4. Update docs.
   - Document the names and intended scale.
   - State that with standard BSP texture axes, one texel/texture unit corresponds to one world inch unless the map author changes texture scaling.

### Acceptance criteria

- BSP maps can reference developer texture names without external WADs.
- The generated textures make 12-inch, 64-inch, 72-inch, and 96-inch scale visually obvious.
- Material fallback remains deterministic.
- Default tests remain display-free.

### Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
  stellar_bsp_materials_test \
  stellar_render_level_upload_test \
  stellar_render_level_inspection_test -j$(nproc)
ctest --test-dir build -R '^(bsp_materials|render_level_upload|render_level_inspection)$' --output-on-failure
```

---

## Phase 3 — Load the configured BSP map into the live client path

### Task card

- Branch: `bsp-gameplay-loop`
- Primary agent: `@director` for integration, implementation likely `@carmack` + `@miyamoto`
- Type: client/runtime/render integration
- Depends on: existing BSP validation and renderer foundations
- Can run in parallel with: Phase 2 if import/material files are separate
- Must finish before: player-controlled room acceptance

### Objective

Make `stellar-client --map path/to/map.bsp` use the loaded BSP level in the live renderer and runtime world, not just validation.

### Current gap to fix

`Application::run()` validates `config_` and receives an `ApplicationValidation`, but after validation it creates a renderer with `std::nullopt`, which triggers the debug/fallback level path. The live loop also does not create `RuntimeWorld` or `LocalLoopbackRuntime`.

### Preferred files

Update:

- `include/stellar/client/Application.hpp`
- `src/client/Application.cpp`
- `include/stellar/client/ApplicationConfig.hpp`
- `src/client/ApplicationConfig.cpp`
- `include/stellar/graphics/RendererFactory.hpp`
- `src/graphics/RendererFactory.cpp`
- `CMakeLists.txt`
- `tests/client/AssetValidationSmoke.cpp`
- add a new display-free application/bootstrap test if practical

### Implementation steps

1. Preserve the loaded level after validation.
   - `validate_application_config` already returns `ApplicationValidation::level`.
   - Do not re-import the map in the live path if validation already loaded it.
2. Build runtime world once when a map is present.
   - Keep `LevelAsset` alive for the full lifetime of `RuntimeWorld`, `WorldSession`, and `LocalLoopbackRuntime`.
   - Avoid dangling references: `RuntimeWorld` references `LevelAsset` collision data.
3. Pass the loaded `LevelAsset` into `create_renderer`.
   - Replace `std::nullopt` with the moved/copied loaded level when present.
   - Preserve debug cube fallback only when no `--map` is supplied.
4. Instantiate `LocalLoopbackRuntime` for maps with a runtime world.
   - Use inch-scale movement defaults from Phase 1.
   - Avoid gameplay authority in renderer or client presentation.
5. Link `stellar-client` to `stellar_client_runtime`.
   - `Application.cpp` will need `LocalLoopbackRuntime` and `PlayerPresentation`.
6. Add tests where possible without opening a window.
   - Factor a display-free bootstrap helper if needed, e.g. `prepare_application_runtime(config)`.
   - Assert that a `.bsp` map produces a loaded level, runtime diagnostics, and optional loopback runtime state.

### Non-goals

- Do not add networking.
- Do not add client prediction.
- Do not add scripts to the client.
- Do not require a window/GPU for default tests.

### Acceptance criteria

- `stellar-client --map valid.bsp` renders the BSP level rather than the fallback cube.
- The loaded `LevelAsset` lifetime safely outlives `RuntimeWorld` and the local loopback session.
- Default validation tests remain headless.
- No renderer/audio authority is introduced.

### Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
  stellar-client \
  stellar_client_map_validation_smoke \
  stellar_client_local_loopback_runtime_test \
  stellar_render_level_upload_test -j$(nproc)
ctest --test-dir build -R '^(client_map_validation_smoke|client_cli_map_validation|client_cli_validate_map|client_local_loopback_runtime|render_level_upload)$' --output-on-failure
```

---

## Phase 4 — Authoritative player camera drives level rendering

### Task card

- Branch: `bsp-gameplay-loop`
- Primary agent: `@miyamoto`
- Consult: `@carmack` for snapshot/runtime boundaries, `@director` for interface decisions
- Type: presentation integration
- Depends on: Phase 1 and Phase 3
- Can run in parallel with: Phase 5 if files do not overlap
- Must finish before: controllable single-room acceptance

### Objective

Render the BSP room from a camera derived from the authoritative player snapshot, using the existing `PlayerPresentation` path.

### Preferred design

Avoid making `stellar_graphics` depend on `stellar_client`. Add a small graphics-facing camera/view struct if needed, and have `Application.cpp` convert `PlayerCameraFrame` into graphics render state.

Possible implementation options, in preference order:

1. Add a backend-neutral `graphics::LevelRenderView` and a `LevelRenderer::set_render_view(...)` / `set_billboards(...)` state update method before `render()`.
2. Add a `RendererFrameState` virtual method to `Renderer` with a no-op default, implemented by `LevelRenderer`.
3. If minimal risk is preferred, use `LevelRenderer` directly in `Application.cpp` instead of the generic `Renderer` pointer, while keeping `RendererFactory` for tests and fallback paths.

### Preferred files

Update:

- `include/stellar/graphics/LevelRenderer.hpp`
- `src/graphics/LevelRenderer.cpp`
- `include/stellar/graphics/Renderer.hpp` only if choosing a virtual frame-state API
- `src/client/Application.cpp`
- `tests/graphics/RenderSceneInspection.cpp`
- `tests/client/PlayerPresentation.cpp`

### Implementation steps

1. Add a way for `Application` to submit a player-follow camera to `LevelRenderer`.
2. Each frame:
   - Process input.
   - Advance `LocalLoopbackRuntime` with `delta_seconds`.
   - Extract local player presentation state from `latest_snapshot` or the frame result.
   - Build `PlayerCameraFrame`.
   - Convert to renderer view/projection state.
   - Render the level using player camera position for optional BSP visibility culling.
3. Preserve fallback camera fit when no runtime/player state exists.
4. Add tests for view selection without requiring a graphics context.
   - Camera override wins over bounds-fit camera.
   - Fallback camera remains used when no player is present.
   - Camera world position is passed to `RenderLevel` visibility culling.

### Acceptance criteria

- A map launched with `--map` renders from a camera following the authoritative local player.
- The camera is presentation-only and cannot modify authoritative gameplay state.
- Fallback cube/no-map renderer behavior still works.
- OpenGL/Vulkan selection remains through shared abstractions.

### Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
  stellar_player_presentation_test \
  stellar_render_level_inspection_test \
  stellar_graphics_backend_selection_test \
  stellar-client -j$(nproc)
ctest --test-dir build -R '^(player_presentation|render_level_inspection|graphics_backend_selection)$' --output-on-failure
```

---

## Phase 5 — Minimal ECS/entity spawn from BSP metadata

### Task card

- Branch: `bsp-gameplay-loop`
- Primary agent: `@carmack`
- Consult: `@kojima` for archetypes, `@director` for boundaries
- Type: server-authoritative ECS/gameplay foundation
- Depends on: BSP metadata import and world metadata validation
- Can run in parallel with: Phase 4 if files do not overlap
- Must finish before: item pickup and richer interaction loop

### Objective

Add a minimal server-owned entity layer that can spawn gameplay entities from BSP metadata without becoming a full ECS rewrite.

### Recommended scope

Start with the smallest useful entity model:

```cpp
using EntityId = std::uint32_t;

enum class EntityKind {
    kPlayer,
    kStaticSprite,
    kPickup,
    kDoorGate,
    kTriggerMarker,
    kObjectColliderMarker,
};

struct EntityTransformComponent {
    std::array<float, 3> position{};
    std::array<float, 4> rotation{0.0F, 0.0F, 0.0F, 1.0F};
    std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
};
```

Keep it serializable and server-owned. Use simple vectors/maps first; do not overbuild archetype storage until gameplay pressure justifies it.

### Preferred files

New:

- `include/stellar/server/GameplayEntity.hpp`
- `include/stellar/server/GameplayWorld.hpp`
- `src/server/GameplayWorld.cpp`
- `tests/server/GameplayWorld.cpp`

Update:

- `CMakeLists.txt`
- `include/stellar/server/WorldSession.hpp` only if player entity id must appear in snapshots
- `docs/Design.md`
- `docs/ImplementationStatus.md`

### Implementation steps

1. Add a minimal entity id allocator with deterministic ordering.
2. Spawn one local player entity from the first player spawn marker.
   - Bind `PlayerId` to `EntityId` but do not break existing `WorldSnapshot` consumers.
3. Spawn sprite marker entities from `WorldMarkerType::kSprite`.
   - Preserve marker name, archetype/sprite id, transform, size/alpha properties if already parsed.
4. Spawn pickup candidates from object-collider markers or `info_stellar_spawn` markers with `archetype=pickup`/`item`.
5. Spawn door/gate metadata from named collision meshes, trigger markers, or `func_door`/`func_wall` metadata when available.
6. Expose a display-free snapshot/query API.
   - Do not expose renderer handles.
   - Do not execute scripts during import/entity spawn.
7. Integrate with `WorldSession` only as needed.
   - Preferred: `WorldSession` owns or references the minimal `GameplayWorld` and includes stable entity ids in optional snapshots/events.
   - Preserve current `WorldSession` APIs if possible.

### Non-goals

- Full ECS archetype storage.
- Remote replication format.
- Client-side entity authority.
- Model/skinning/animation.
- Dynamic rigid bodies.

### Acceptance criteria

- BSP metadata can produce deterministic server-owned entities.
- Local player has a stable entity id tied to authoritative movement state.
- Sprite/pickup/door metadata is represented without renderer handles or script execution.
- Tests are display-free and deterministic.

### Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
  stellar_server_gameplay_world_test \
  stellar_server_world_session_test \
  stellar_runtime_world_test \
  stellar_world_metadata_validation_test -j$(nproc)
ctest --test-dir build -R '^(server_gameplay_world|server_world_session|runtime_world|world_metadata_validation)$' --output-on-failure
```

---

## Phase 6 — Single-room controllable player loop

### Task card

- Branch: `bsp-gameplay-loop`
- Primary agent: `@director` for integration, implementation split between `@carmack`, `@miyamoto`, and `@kojima`
- Type: end-to-end local gameplay loop
- Depends on: Phases 1, 3, 4, and ideally Phase 5
- Can run in parallel with: no broad client-loop changes
- Must finish before: item pickup/scripted door polish

### Objective

Create the first real branch milestone: a user can launch a BSP room and control a player inside it through the authoritative local loopback session.

### Room scale target

Use an inch-scale single-room fixture/map:

```text
Room footprint: 192 x 192 inches (16 ft x 16 ft)
Wall height:    96 inches (8 ft)
Player height:  72 inches
Player radius:  16 inches
Spawn center:   36 inches above floor
Door/gate:      optional blocker sized for human scale
Trigger/pickup: authored in inches
```

Coordinate convention: Y up. A center-origin room can use approximately:

```text
floor y = 0
ceiling y = 96
x/z = -96..96
player spawn = 0 36 0
```

### Preferred files

Update:

- `tests/fixtures/BspFixture.hpp`
- `tests/fixtures/BspFixtureWriter.cpp`
- `tests/integration/BspPlayableWorldSmoke.cpp`
- `src/client/Application.cpp`
- `tests/client/LocalLoopbackRuntime.cpp`
- `docs/BspAuthoring.md`

Possibly add:

- `tests/integration/BspGameplayLoopSmoke.cpp`
- `tests/client/ApplicationRuntimeBootstrap.cpp`

### Implementation steps

1. Add a new named fixture, for example `gameplay_room`.
   - Do not replace all old tiny fixtures at once; keep old fixtures where tests depend on small coordinates.
   - Use developer texture names from Phase 2 on floor/walls/ceiling.
2. Ensure fixture includes:
   - `worldspawn`
   - `info_player_start` at y = 36
   - static collision floor/walls/ceiling
   - one sprite marker for a visible placeholder/player or torch
   - one object collider marker for future pickup work
   - one trigger marker for future door/gate work
3. Add display-free smoke coverage.
   - Load BSP.
   - Build `RuntimeWorld`.
   - Create `LocalLoopbackRuntime`.
   - Advance forward/right input for multiple ticks.
   - Assert player moves in expected direction.
   - Assert player cannot leave the room through walls.
   - Assert snapshots remain authoritative and deterministic.
4. Add live-client path acceptance.
   - `stellar-client --validate-map generated/gameplay_room.bsp` passes.
   - `stellar-client --map generated/gameplay_room.bsp` should open and render the room when run manually in a display environment.
5. Preserve no-map fallback cube behavior.

### Acceptance criteria

- There is a generated inch-scale BSP room fixture.
- Headless integration proves the player can move and collide inside the room.
- Live client path uses map + runtime + authoritative local input loop.
- No client-side gameplay authority is introduced.

### Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
  stellar_bsp_fixture_writer \
  stellar_bsp_playable_world_smoke_test \
  stellar_client_local_loopback_runtime_test \
  stellar-client -j$(nproc)
ctest --test-dir build -R '^(bsp_fixture_writer|bsp_playable_world_smoke|client_local_loopback_runtime|client_cli_validate_map|client_cli_map_validation)$' --output-on-failure
```

Manual display validation, not part of default CTest:

```bash
./build/stellar-client --map ./build/tests/fixtures/bsp/gameplay_room.bsp --renderer opengl
./build/stellar-client --map ./build/tests/fixtures/bsp/gameplay_room.bsp --renderer vulkan
```

If the environment lacks a display/GPU, record this as skipped manual validation, not as a default-test failure.

---

## Phase 7 — First interaction loop: pickup and scripted door/gate

### Task card

- Branch: `bsp-gameplay-loop`
- Primary agent: `@kojima` for gameplay behavior, `@carmack` for server/scripting seams
- Review: `@director`
- Type: server-authoritative gameplay mechanics
- Depends on: Phase 6 and existing Lua command path
- Can run in parallel with: limited sprite presentation polish if files do not overlap
- Must finish before: final gameplay-loop branch completion

### Objective

Turn the controllable room into a minimal gameplay loop: move, overlap a pickup, update authoritative state, and use a trigger/script to toggle a door/gate collision mesh.

### Required foundations to reuse

- `ScriptedWorldSession`
- `TriggerScriptSystem`
- `ObjectColliderScriptSystem`
- `ScriptCommandProcessor`
- `WorldSession::set_collision_mesh_enabled`
- `WorldSession::set_object_collider_enabled`

### Implementation steps

1. Define minimal gameplay state.
   - Pickup entity: active/inactive, optional count or event.
   - Door/gate entity: collision mesh name, open/closed state.
2. Pickup behavior.
   - Object-collider enter event for pickup emits a server-authoritative event.
   - Native code validates and disables/removes the pickup collider and presentation entity.
   - Snapshot/event exposes pickup state for presentation.
3. Door/gate behavior.
   - Trigger enter/stay/exit invokes existing Lua hook path.
   - Script emits `collision.set_mesh_enabled` with a named gate mesh and `enabled=false` or toggles on/off according to authored policy.
   - Native command processor validates and applies the collision state change through `WorldSession`.
4. Add authored fixture/script coverage.
   - Add tiny Lua scripts under tests/fixtures or embedded test registry consistent with existing scripting tests.
   - Ensure script path sandbox rules remain enforced.
5. Add tests.
   - Pickup can be collected once.
   - Pickup disables object collider and stops repeated enter events.
   - Door trigger toggles named collision mesh through Lua command path.
   - Invalid script command fails deterministically.
   - Default tests remain display-free.

### Non-goals

- Inventory UI.
- Sound playback.
- Network replication.
- Moving brush animation.
- Client-side scripts.
- Dynamic rigid body doors.

### Acceptance criteria

- The room has at least one collectable item or equivalent pickup.
- A scripted trigger can change a named gate/door collision state through the existing sandboxed Lua command path.
- Gameplay state is server-owned and reflected to presentation through snapshots/events.
- Tests prove deterministic behavior without a display.

### Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
  stellar_scripted_world_session_test \
  stellar_trigger_script_system_test \
  stellar_object_collider_script_system_test \
  stellar_script_command_processor_test \
  stellar_bsp_playable_world_smoke_test -j$(nproc)
ctest --test-dir build -R '^(scripted_world_session|trigger_script|object_collider_script|script_command_processor|bsp_playable_world_smoke|bsp_scripted_playable_world_smoke|bsp_scripted_collision_smoke|bsp_scripted_object_collider_smoke)$' --output-on-failure
```

---

## Phase 8 — Final branch hardening and documentation

### Task card

- Branch: `bsp-gameplay-loop`
- Primary agent: `@director`
- Type: integration, validation, docs, handoff
- Depends on: all selected implementation phases
- Can run in parallel with: no code churn except small docs/tests fixes
- Must finish before: branch ready for merge/review

### Objective

Validate the branch end-to-end, align docs, and record clear completion notes.

### Implementation steps

1. Update docs.
   - `docs/ImplementationStatus.md`: completion notes for implemented phases.
   - `docs/Design.md`: unit policy, live BSP loop, entity spawn direction, interaction loop status.
   - `docs/BspAuthoring.md`: inch-scale room examples and developer texture names.
   - `Plans/NEXT.md`: point to the next recommended scope after this branch.
2. Audit active references.
   - No stale GLTF/retired importer references outside archived docs/build outputs.
   - No meter-scale default language remains for gameplay units.
3. Run full validation.
4. Record known deferrals.
   - Remote networking/snapshot expansion.
   - Client prediction/reconciliation.
   - Toolchain/editor workflow polish.
   - Rich animation system.
   - Audio/event presentation.
   - UI/HUD/inventory.
   - Moving brush simulation if desired later.

### Acceptance criteria

- The branch has a documented inch-scale BSP gameplay loop.
- Default CTest passes display-free.
- The live client can run a BSP room manually where display/GPU are available.
- The next active scope is clear and not confused with archived plans.

### Final validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(bsp_|render_level|runtime_world|client_|player_presentation|server_world_session|scripted_world_session|trigger_script|object_collider_script|script_command_processor|world_metadata_validation|collision_validation|character_controller)' --output-on-failure
git grep -n -i 'STELLAR_ENABLE_GLTF\|cgltf\|SceneAsset\|gltf' -- . ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'meter\|metre\|1\.8F\|0\.35F\|0\.8F\|6\.0F' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
```

---

## 4. Product assumptions and non-blocking questions

No blocking user answer is required to start Phase 0/1/3. These assumptions should be documented and adjusted later if desired:

1. The first player is a human-scale capsule: 72 inches tall, 16 inch radius.
2. The first room target is 16 ft x 16 ft x 8 ft.
3. Y remains up, and BSP coordinates import 1:1 with no hidden scale conversion.
4. The first live loop is single-player local loopback only.
5. The first visible player may be camera-only or a simple debug billboard/capsule marker; no model/animation system is required.
6. Developer textures can be procedural rather than committed binary images.
7. Pickup/door interaction can initially be proven through headless snapshots/events before UI/audio polish.

---

## 5. Suggested immediate next implementation slice

Start with Phases 1, 3, and 6 in that order if implementation time is limited:

1. Lock the inch unit policy and update defaults.
2. Make `stellar-client --map` actually use the loaded BSP level and local loopback runtime.
3. Add the inch-scale single-room fixture and headless controllable-player smoke test.

Phase 2 developer textures can be implemented in parallel or immediately after; they improve authoring clarity but should not block the headless playable loop.
