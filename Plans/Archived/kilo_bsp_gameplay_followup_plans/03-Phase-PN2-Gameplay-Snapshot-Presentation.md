# Phase PN-2 — Authoritative Gameplay Snapshot Presentation

Optimized for Kilo Code intake.

## Owner

- Primary graphics/presentation: `@miyamoto`
- Runtime data support: `@carmack`
- Gameplay presentation semantics: `@kojima`
- Coordinator: `@director`

## Goal

Render server-owned gameplay snapshots in the live client: sprite markers, active pickups, and basic door/gate interaction state.

This phase must present authoritative data only. It must not add renderer-owned gameplay state.

## Required reading

- `AGENTS.md`
- `docs/ImplementationStatus.md`
- `docs/Design.md`, especially client responsibilities, graphics, ECS, and server authority
- `docs/BspAuthoring.md`
- `include/stellar/server/GameplayWorld.hpp`
- `include/stellar/server/WorldSession.hpp`
- `include/stellar/graphics/BillboardSprite.hpp`
- `include/stellar/graphics/RenderLevel.hpp`
- `include/stellar/graphics/LevelRenderer.hpp`
- `src/graphics/LevelRenderer.cpp`
- `src/client/Application.cpp`
- Phase PN-1 result

## Current implementation anchors

- `WorldSnapshot` already includes `GameplayWorldSnapshot gameplay_world`.
- `GameplayWorldSnapshot` contains deterministic `GameplayEntity` records with `kind`, `transform`, `metadata`, `active`, and `open`.
- `GameplayEntityMetadata` preserves `sprite_id`, `alpha`, `size`, object-collider id, and collision mesh index/name info.
- `RenderLevel` already has overloads for billboard sprite rendering.
- `LevelRenderer` currently accepts only a camera `LevelRenderView`; it does not accept gameplay sprites/entities.
- Live client already computes player camera from the authoritative local snapshot.

## Scope

Implement a presentation adapter that converts authoritative snapshot data into renderable, backend-neutral draw data.

Initial presentable entities:

- `EntityKind::kSprite`: render when active.
- `EntityKind::kPickup`: render when active; hide after collection.
- `EntityKind::kDoor`: optionally render a debug/status marker or tint/indicator when open/closed if static geometry cannot be hidden yet.
- `EntityKind::kTrigger` and `kObjectCollider`: do not render by default unless a debug visualization flag is added.
- `EntityKind::kPlayer`: already drives camera for local player; do not render first-person local player model in this phase.

## Recommended design

### 1. Add a presentation conversion module

Suggested files:

```text
include/stellar/client/GameplayPresentation.hpp
src/client/GameplayPresentation.cpp
tests/client/GameplayPresentation.cpp
```

Possible API:

```cpp
struct GameplayPresentationConfig {
    bool show_debug_interaction_markers = false;
    std::array<float, 2> default_sprite_size{24.0F, 24.0F};
    std::array<float, 4> default_sprite_color{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 4> pickup_color{1.0F, 1.0F, 0.25F, 1.0F};
    std::array<float, 4> door_closed_color{1.0F, 0.25F, 0.25F, 1.0F};
    std::array<float, 4> door_open_color{0.25F, 1.0F, 0.25F, 0.5F};
};

struct GameplayPresentationFrame {
    std::vector<stellar::graphics::BillboardSprite> sprites;
};

GameplayPresentationFrame make_gameplay_presentation_frame(
    const stellar::server::WorldSnapshot& snapshot,
    const GameplayPresentationConfig& config = {});
```

No GPU handles should be stored in server snapshots. The adapter may emit color-only billboards with invalid texture handles until a texture resolver exists.

### 2. Extend `LevelRenderer` state API

Add a backend-neutral per-frame or retained presentation state.

Preferred simple API:

```cpp
struct LevelPresentationState {
    std::vector<BillboardSprite> sprites;
};

void set_presentation_state(LevelPresentationState state) noexcept;
void clear_presentation_state() noexcept;
```

Alternative if avoiding retained vectors in renderer:

```cpp
void render(..., std::span<const BillboardSprite> sprites);
```

The current `Renderer` interface likely only calls `render(elapsed, delta, width, height)`, so retained state is probably lower churn.

### 3. Build `BillboardView` from render state

`RenderLevel` needs `BillboardView` for billboard expansion.

In `LevelRenderer::render()` after computing `LevelRenderState`, derive:

```cpp
BillboardView billboard_view;
billboard_view.view_projection = state.view_projection;
billboard_view.view = state.view;
billboard_view.camera_right = ... from view matrix or original camera basis;
billboard_view.camera_up = ... from view matrix or original camera basis;
```

Preserve backend-neutral math.

### 4. Feed presentation state in `Application::run()`

After runtime update and camera extraction:

1. Get latest authoritative snapshot.
2. Compute player camera state as today.
3. Compute gameplay presentation frame from snapshot.
4. Send both to `LevelRenderer`.
5. Render.

Pseudo-flow:

```cpp
const auto& snapshot = runtime->local_loopback_runtime->latest_snapshot();

if (auto player_state = make_player_presentation_state(snapshot, local_player_id)) {
    renderer->set_render_view(...);
}

renderer->set_presentation_state(
    make_level_presentation_state(make_gameplay_presentation_frame(snapshot)));
```

If no runtime exists, clear presentation state.

### 5. Preserve no-map fallback

No-map debug cube should render without runtime/presentation data.

### 6. Do not implement full texture resolver yet unless cheap

If the renderer cannot safely draw invalid texture billboards, add or reuse a deterministic default material/texture path. Keep this presentation-only.

Do not add an asset manager or sprite atlas in this phase unless already present and trivial.

## Tests

Add display-free tests:

1. `make_gameplay_presentation_frame()` converts active `kSprite` to a billboard.
2. Inactive pickup is filtered out.
3. Active pickup appears with deterministic size/color.
4. Door open/closed state produces deterministic debug marker only when debug markers enabled.
5. Unknown/empty metadata uses finite defaults.
6. `LevelRenderer` or render inspection path includes billboards after static level geometry.
7. Existing camera override/culling tests still pass.
8. No-map fallback clears presentation state.

Possible test files:

```text
tests/client/GameplayPresentation.cpp
tests/graphics/RenderSceneInspection.cpp
tests/client/PlayerPresentation.cpp
```

## Validation commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
  stellar_player_presentation_test \
  stellar_render_level_inspection_test \
  stellar_graphics_backend_selection_test \
  stellar-client \
  -j$(nproc)

ctest --test-dir build -R '^(player_presentation|render_level_inspection|graphics_backend_selection|gameplay_presentation)$' --output-on-failure
```

Then full tests:

```bash
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- Server `GameplayWorldSnapshot` drives live client presentation.
- Active sprite/pickup entities produce billboards.
- Collected/inactive pickups stop presenting.
- Door/gate state can be observed in presentation data without moving brush simulation.
- Renderer and client remain presentation-only.
- No default test requires GPU/display.
- No client-side gameplay scripts added.
