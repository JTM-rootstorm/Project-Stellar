# Stellar Engine - Game Framework Development Document

**Project:** Stellar Engine  
**Target Platform:** Linux-first, with cross-platform architecture  
**Language:** C++23, C99 where required for single-file C dependencies such as miniaudio  
**Build System:** CMake 3.20+  
**Version:** 0.1.4 (collision-movement branch alignment)  
**Last Updated:** 2026-04-29

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Documentation Authority](#2-documentation-authority)
3. [Current Branch Direction](#3-current-branch-direction)
4. [System Architecture](#4-system-architecture)
5. [Core Engine Subsystem](#5-core-engine-subsystem)
6. [Asset, Import, and World Authoring](#6-asset-import-and-world-authoring)
7. [Graphics Subsystem](#7-graphics-subsystem)
8. [Audio Subsystem](#8-audio-subsystem)
9. [Entity Component System](#9-entity-component-system)
10. [Networking and Client-Server Model](#10-networking-and-client-server-model)
11. [Build System and Dependencies](#11-build-system-and-dependencies)
12. [Directory Structure](#12-directory-structure)
13. [Team Responsibilities and Agent Boundaries](#13-team-responsibilities-and-agent-boundaries)
14. [Development Guidelines](#14-development-guidelines)
15. [Documentation Standards](#15-documentation-standards)
16. [Testing Strategy](#16-testing-strategy)
17. [Roadmap and Deferred Work](#17-roadmap-and-deferred-work)
18. [Change Log](#18-change-log)

---

## 1. Project Overview

Stellar Engine is a modular, cross-platform game framework for creating games that use
3D world or level geometry with 2D billboard entities, objects, pickups, projectiles,
and props. The engine uses a client-server architecture: game logic and authoritative
state live on the server, while the client handles rendering, audio, input capture,
and presentation.

The current engine direction is not to become a general-purpose glTF viewer or a full
physically based renderer. glTF is used as a practical authored scene and level source
format for the engine's world representation. The current rendering target is lightweight
OpenGL/Vulkan material parity suitable for game content.

### Key Design Principles

- **Server authority:** the server owns game state, game rules, ECS simulation, physics
  decisions, and validation.
- **Client as presentation:** the client renders and presents server state, captures input,
  and forwards input to the server. Rendering and audio are never sources of gameplay truth.
- **Modularity:** graphics, audio, import, networking, and core systems are isolated behind
  explicit interfaces.
- **Data-oriented ECS:** gameplay state should remain serializable, cache-friendly, and
  server-owned.
- **Backend parity:** OpenGL and Vulkan remain runtime-selectable through the shared graphics
  abstraction.
- **Display-free default validation:** default tests must not require a GPU, display, or
  OpenGL/Vulkan runtime context.
- **Documentation-driven public API:** public APIs require Doxygen `@brief` documentation.

### Technical Stack

- **Language:** C++23 plus C99 where required for C dependencies.
- **Build:** CMake 3.20+.
- **Rendering:** OpenGL 4.5+ and Vulkan 1.2+, selected at runtime.
- **World style:** 3D static world geometry with 2D billboard entities and props.
- **Asset format:** glTF `.gltf` and `.glb` for runtime scene import and authored world data.
- **Math:** GLM.
- **Windowing/input abstraction:** SDL2.
- **Audio:** miniaudio-backed audio when audio implementation is in scope, with explicit
  no-op fallback behavior where needed.
- **Networking:** local loopback for single-player, extensible toward remote multiplayer.

---

## 2. Documentation Authority

This file describes broad architecture and long-term design intent. For the
`collision-movement` branch, it is not the highest authority on current implementation
status.

Precedence for resolving conflicts:

1. `docs/ImplementationStatus.md` — current branch-facing implementation status and
   near-term priorities.
2. Active Phase 6 plans under `Plans/` — current implementation handoff plans when referenced
   by `ImplementationStatus.md`.
3. `docs/Design.md` — broad architecture and long-term design intent.
4. `AGENTS.md` and `.kilo/agents/*.md` — coordination rules, ownership, and agent behavior.

Historical roadmap files under `Plans/` and `.kilo/plans/` may contain stale intermediate
state unless `docs/ImplementationStatus.md` identifies them as current. Completion notes in
plan files remain useful for understanding what was implemented, validated, or explicitly
deferred.

---

## 3. Current Branch Direction

Current branch: `collision-movement`.

Primary near-term goal: move from "glTF renders as a scene" toward "glTF can define playable
static world geometry" for a 3D world with 2D billboard entities.

Recommended implementation order:

1. **Phase 6A — Static Level Collision Extraction From glTF**
   - Add backend-neutral static level collision data.
   - Extract collision triangles from selected glTF nodes and meshes.
   - Support initial node-name conventions such as `COL_*`, `Collision_*`, and a parent node
     named exactly `Collision`.
   - Transform collision triangles into scene/world space.
   - Keep collision data independent of OpenGL and Vulkan.
   - Keep tests display-free.

2. **Phase 6B — Collision Queries And Minimal Movement Resolution**
   - Add ray or segment queries against Phase 6A static collision data.
   - Add ground probing.
   - Add minimal sweep/slide movement over static collision.
   - Do not introduce a full third-party physics engine in this slice.

3. **Phase 6C — Billboard Sprite Rendering For Entities**
   - Add backend-neutral 2D billboard sprite draw data in 3D world space.
   - Preserve OpenGL/Vulkan behavior parity.
   - Support alpha mask/blend behavior and deterministic sorting tests.
   - Keep ECS/gameplay integration out of scope unless explicitly added.

4. **Phase 6D — World Metadata From glTF Node Conventions**
   - Extract player spawns, generic entity spawns, triggers, sprite markers, and optional portal
     markers from glTF node conventions.
   - Keep runtime spawning and trigger behavior separate from metadata extraction unless a later
     plan explicitly adds them.

Avoid spending the next implementation slices on full PBR, morph targets, glTF-authored cameras,
or glTF-authored lights unless a concrete asset requirement appears.

---

## 4. System Architecture

### High-Level Architecture

```text
┌─────────────────┐
│   Game Client   │  ← Rendering, audio, input capture, presentation
│  (Presentation) │
├─────────────────┤
│   Network API   │  ← Messages, snapshots, commands, serialization
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   Game Server   │  ← ECS, game logic, physics/collision, AI, validation
│  (Authority)    │
├─────────────────┤
│   World State   │  ← Entity components, game rules, authoritative simulation
└─────────────────┘
```

### Client Responsibilities

- Initialize platform windowing, graphics, audio, input, and asset presentation paths.
- Select OpenGL or Vulkan at runtime through the graphics abstraction.
- Receive world snapshots or render commands from the server.
- Interpolate presentation state when appropriate.
- Render static level geometry, skinned/static glTF scenes, billboard sprites, and UI.
- Capture input and forward it to the server.
- Play local presentation audio based on server-approved events or presentation state.

### Server Responsibilities

- Own authoritative ECS state.
- Process client input as requests, not trusted state.
- Run deterministic gameplay systems where practical.
- Own collision and movement decisions.
- Validate game rules.
- Produce world snapshots or state deltas for clients.
- Keep gameplay state serializable and independent of renderer/audio backends.

### Server Authority Model

- Server is the single source of truth for gameplay state.
- Client prediction may be added later, but it must be explicitly scoped and reconciled against
  server state.
- Rendering, audio, and client-only interpolation cannot become sources of authoritative gameplay
  truth.
- Imported collision and world metadata must be usable by server/gameplay code without OpenGL or
  Vulkan dependencies.

---

## 5. Core Engine Subsystem

**Primary ownership:** `@carmack`, coordinated by `@director` for cross-subsystem work.  
**Responsibility:** core systems, ECS surfaces, world management, serialization, networking,
platform infrastructure, and static collision/query infrastructure.

### 5.1 Application and Server Entry Points

#### Client Application

- Initialize platform, window, graphics backend, audio, input, and client asset paths.
- Connect to local or remote server.
- Receive and present server-owned state.
- Submit graphics work through the shared abstraction.
- Keep presentation behavior separate from server authority.

#### Server Application

- Initialize ECS world and authoritative game systems.
- Load or receive backend-neutral world data.
- Own collision queries and movement validation.
- Run simulation ticks.
- Serialize snapshots or deltas for clients.

### 5.2 Core World Data

Core world data should remain backend-neutral and serializable. Render-specific resources, audio
handles, Vulkan/OpenGL objects, and client-only presentation caches must not be stored as
authoritative gameplay state.

Initial gameplay/world data categories:

- Entity identifiers and component storage.
- Transform, velocity, and gameplay components.
- Server-owned input state or command history.
- Static collision data extracted from authored levels.
- World metadata such as spawns, triggers, and sprite/entity markers.
- Serialized snapshots and deltas.

### 5.3 Static Collision Direction

Phase 6A introduces static collision data as an asset/import concern, not as renderer state.

Preferred public asset direction:

```cpp
namespace stellar::assets {

/**
 * @brief Triangle in backend-neutral static level collision data.
 */
struct CollisionTriangle {
    std::array<float, 3> a{};
    std::array<float, 3> b{};
    std::array<float, 3> c{};
    std::array<float, 3> normal{};
};

/**
 * @brief Named set of static collision triangles and bounds.
 */
struct CollisionMesh {
    std::string name;
    std::vector<CollisionTriangle> triangles;
    std::array<float, 3> bounds_min{};
    std::array<float, 3> bounds_max{};
};

/**
 * @brief Imported static collision data for a playable level.
 */
struct LevelCollisionAsset {
    std::vector<CollisionMesh> meshes;
    std::array<float, 3> bounds_min{};
    std::array<float, 3> bounds_max{};
};

} // namespace stellar::assets
```

This model may be represented as `std::optional<LevelCollisionAsset>` on `SceneAsset` or as an
always-present `LevelCollisionAsset` where an empty mesh list means no collision.

### 5.4 Collision Query Direction

Phase 6B builds on Phase 6A data with a small backend-neutral collision query module. It should
support:

- Finite segment/raycast queries.
- Nearest-hit selection.
- Ground probing.
- Minimal sweep-and-slide movement.
- Simple deterministic behavior over static collision geometry.
- Display-free tests.

Do not introduce full rigid body simulation, dynamic bodies, navigation meshes, or broad physics
engine integration in this slice.

---

## 6. Asset, Import, and World Authoring

**Primary ownership:** import/render asset routing is usually `@miyamoto`; backend-neutral
collision/world metadata work is usually `@carmack`, coordinated by `@director` when work crosses
graphics/import/gameplay boundaries.

### 6.1 glTF Support Status

Implemented on the current branch:

- `.gltf` and `.glb` importer coverage through generated fixtures.
- Static glTF mesh import for triangle primitives.
- Bounds, tangents, UV0/UV1, vertex colors, textures, samplers, and materials.
- Runtime scene loading through the client asset path.
- Base color, normal, metallic-roughness, occlusion, emissive, alpha mask/blend, double-sided
  material state, UV routing, vertex color modulation, and conservative tangent generation.
- `KHR_texture_transform` for supported material texture slots.
- `KHR_materials_unlit`.
- Skin and animation asset import.
- Runtime animation evaluation and scene pose generation.
- GPU skinning in both OpenGL and Vulkan.
- Shared runtime skin palette cap of 256 joints per draw.

The current renderer targets lightweight material parity. It does not claim full glTF
metallic-roughness PBR compliance.

### 6.2 Required and Optional glTF Extensions

Supported required extensions currently accepted:

- `KHR_texture_transform`
- `KHR_materials_unlit`

Policy:

- Unknown or unsupported `extensionsRequired` entries should fail clearly and include the extension
  name in the error.
- Optional unsupported data should degrade predictably without creating runtime state that implies
  support.
- New required-extension support should only be accepted after importer support and both render
  backends can represent and render the feature consistently.
- Static collision extraction is an engine node-convention/import policy, not a glTF extension.

### 6.3 Current glTF Deferrals

Intentionally deferred unless a concrete requirement appears:

- Full metallic-roughness PBR and full glTF viewer-style visual fidelity.
- Morph target deformation and animation `weights` channels.
- glTF-authored cameras and lights.
- Broad extension coverage beyond the supported material extensions listed above.
- Deterministic GPU readback validation for rendered pixel output.
- Full physics engine integration.
- Dynamic rigid bodies.
- Navigation mesh/pathfinding.
- Runtime trigger behavior.
- ECS/server spawning directly from glTF metadata.

### 6.4 glTF Collision Node Conventions

Initial Phase 6A collision conventions:

- Node names starting with `COL_` are collision-only.
- Node names starting with `Collision_` are collision-only.
- A node named exactly `Collision` marks descendant mesh nodes as collision-only.

Collision-only means triangles are extracted into backend-neutral collision data. Renderer behavior
should not be changed by Phase 6A unless the existing import/render path already supports doing so
without broad renderer churn.

### 6.5 World Metadata Node Conventions

Initial Phase 6D metadata conventions:

- `SPAWN_Player` creates a player spawn marker.
- `SPAWN_<Archetype>` creates a generic entity spawn marker.
- `TRIGGER_<Name>` creates a trigger marker.
- `SPRITE_<Name>` creates a sprite placement marker.
- `PORTAL_<Name>` may be parsed if trivial, but runtime behavior remains deferred.

Transform interpretation:

- Node world translation is marker position.
- Node world rotation is marker orientation if available.
- Node scale may be used for trigger volume extents.

Optional `extras` support may preserve raw JSON text if available through the importer without adding
a new parser solely for this phase.

---

## 7. Graphics Subsystem

**Primary ownership:** `@miyamoto`, coordinated by `@director` for cross-subsystem changes.  
**Responsibility:** graphics abstraction, OpenGL/Vulkan backends, render scene upload, shader paths,
materials, cameras, glTF rendering, billboard sprite rendering, and graphics tests.

### 7.1 Graphics Abstraction

Graphics backends are hidden behind a shared interface. OpenGL and Vulkan must remain
runtime-selectable and should receive the same backend-neutral scene, material, texture, skinning,
and sprite data.

Representative direction:

```cpp
namespace stellar::graphics {

/**
 * @brief Backend-neutral graphics device interface.
 */
class GraphicsDevice {
public:
    virtual ~GraphicsDevice() = default;

    virtual void begin_frame() noexcept = 0;
    virtual void end_frame() noexcept = 0;

    // Mesh, texture, material, scene, and future billboard APIs remain backend-neutral.
};

} // namespace stellar::graphics
```

Frame functions and draw paths that are currently `noexcept` must remain safe: invalid or unsupported
draw commands should log, skip, or return an error from fallible setup/upload paths rather than throw.

### 7.2 Rendering Backend Status

Current branch status:

- OpenGL and Vulkan are runtime-selectable through the shared abstraction.
- OpenGL is render-capable for the current lightweight glTF path.
- Vulkan is not an upload-only or no-op backend. It initializes, creates swapchain resources,
  records draw commands, submits frames, presents, handles resize/recreate paths, and has opt-in
  context smoke coverage.
- Vulkan resource lifetime has been hardened with deferred deletion behavior for in-flight resources.
- Default tests remain display-free.
- GPU/display-dependent OpenGL and Vulkan context tests remain opt-in.

### 7.3 Lightweight Material Parity

The current target is OpenGL/Vulkan parity for a lightweight material model, including:

- Base color factor and texture.
- Vertex color modulation.
- Normal maps when tangents are available.
- Metallic-roughness texture and scalar factors as lightweight shader inputs.
- Occlusion texture and strength as lightweight shader inputs.
- Emissive texture and factor.
- Alpha opaque, mask, and blend behavior.
- Double-sided material/culling behavior.
- `KHR_texture_transform` on supported texture slots.
- `KHR_materials_unlit`.
- Static and skinned mesh rendering.
- Shared skin palette behavior with a conservative runtime cap of 256 joints per draw.

Do not describe this as full glTF PBR. Full PBR requires future decisions around image-based
lighting, environment maps, tone mapping, color management, physically based BRDF parity, and
deterministic validation strategy.

### 7.4 Sprite and Billboard Rendering Direction

Phase 6C adds support for 2D billboard sprites in 3D world space.

Preferred behavior:

- Sprite center is a world-space position.
- Sprite size is world-space width and height.
- Camera right/up vectors produce spherical billboarding.
- Depth testing lets level geometry occlude sprites.
- Alpha blend sprites are sorted back-to-front by camera-space depth.
- Alpha mask and opaque sprites can be handled separately from alpha blend sprites.
- OpenGL and Vulkan behavior remain parity targets.
- Display-free tests validate draw data, sorting, alpha classification, and preservation of size,
  color, and texture handles.

Sprite atlas packing, sprite sheet animation, particles, ECS integration, and glTF-authored sprite
spawn behavior are not part of Phase 6C unless explicitly added by a later plan.

### 7.5 Camera System

Current long-term camera goals:

- 3D perspective camera for main world rendering.
- Follow-player or scripted camera support.
- View/projection matrix generation.
- Camera basis export for billboard generation.
- Bounds/framing utilities for rendering tests and tools.

glTF-authored cameras are currently deferred. Runtime camera selection should remain engine-owned
until a future plan explicitly adds imported camera support.

---

## 8. Audio Subsystem

**Primary ownership:** `@suzuki`, coordinated by `@director` for gameplay/audio integration.  
**Responsibility:** audio interfaces, miniaudio-backed playback, listener updates, spatial audio,
and no-op fallback paths where appropriate.

### 8.1 Audio Direction

Audio should be implemented through an interface that keeps gameplay/server logic separate from
presentation playback. The client owns playback; gameplay/server systems should produce validated
events or state that the client can present.

### 8.2 miniaudio Target

The selected audio library is miniaudio because it is permissively licensed, cross-platform, supports
3D spatial audio features, and can be integrated without complex external dependencies.

Target capabilities:

- Sound loading and one-shot playback.
- Music streaming with loop support.
- 3D spatial audio with listener updates.
- Master volume control.
- Explicit no-op fallback behavior where audio is unavailable or out of scope.
- Clear ownership of audio resource lifetime in the client/presentation layer.

### 8.3 Audio Interface Direction

Representative direction:

```cpp
namespace stellar::audio {

/**
 * @brief Client-side audio device abstraction.
 */
class AudioDevice {
public:
    virtual ~AudioDevice() = default;

    virtual void initialize(bool enable_spatialization) = 0;
    virtual void shutdown() noexcept = 0;

    virtual void update_listener(const glm::vec3& position,
                                 const glm::vec3& forward,
                                 const glm::vec3& up) = 0;

    virtual void set_master_volume(float volume) = 0;
};

} // namespace stellar::audio
```

Do not make audio state authoritative. Runtime sound playback is presentation behavior.

---

## 9. Entity Component System

**Primary ownership:** `@carmack` for ECS and `@kojima` for gameplay/archetype design, coordinated
by `@director` when boundaries overlap.

### 9.1 ECS Design Philosophy

- Components are plain, serializable data where practical.
- Hot iteration should be cache-friendly.
- Avoid virtual calls in hot ECS loops.
- Use data-oriented component storage.
- Keep gameplay state server-owned.
- Keep renderer and audio handles out of authoritative components.

### 9.2 Component Model

Component storage may use archetype-based storage, sparse sets, or a hybrid model. The selected
implementation should prioritize:

- Fast iteration over homogeneous component sets.
- Stable serialization.
- Clear query APIs.
- Deterministic behavior in gameplay tests.
- Minimal coupling between server/gameplay and client presentation.

Representative component direction:

```cpp
namespace stellar::ecs {

using Entity = std::uint32_t;

/**
 * @brief World-space transform owned by gameplay state.
 */
struct TransformComponent {
    glm::vec3 position{};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

/**
 * @brief Server-owned velocity state.
 */
struct VelocityComponent {
    glm::vec3 linear{};
};

} // namespace stellar::ecs
```

### 9.3 Gameplay Archetypes

Long-term archetypes include:

- `Player`
- `NPC`
- `Item`
- `Projectile`
- `Environment`
- Trigger and spawn markers derived from authored metadata
- Sprite/billboard presentation markers for client rendering

The active branch should prioritize collision/world geometry and metadata foundations before broad
gameplay systems.

### 9.4 Systems

Long-term gameplay systems include:

- Input processing.
- Movement.
- Collision and ground probing.
- Health/damage.
- AI.
- Projectile behavior.
- Trigger processing.
- Server snapshot production.

On the current branch, minimal collision queries and movement resolution should come before broad
physics or gameplay system expansion.

---

## 10. Networking and Client-Server Model

**Primary ownership:** `@carmack`, coordinated by `@director` for cross-subsystem integration.

### 10.1 Transport Direction

The networking layer should support:

- Local loopback for single-player.
- Remote multiplayer extension later.
- Reliable connection/control messages where needed.
- Low-latency state updates where appropriate.
- Serializable ECS/world snapshots.

Potential protocol split:

- UDP for frequent state updates.
- Reliable transport for connection setup, critical events, chat, or other ordered messages.

Exact protocol choices remain implementation details until a networking plan scopes them.

### 10.2 Message Categories

Representative message categories:

| Type | Direction | Description |
| --- | --- | --- |
| `INPUT` | Client to server | Input commands or state requests |
| `WORLD_STATE` | Server to client | Snapshot or state delta |
| `ENTITY_SPAWN` | Server to client | New entity presentation |
| `ENTITY_DESTROY` | Server to client | Entity removal |
| `EVENT` | Server to client | Presentation event such as audio or VFX trigger |
| `CONNECT` / `WELCOME` | Both | Connection setup |

### 10.3 State Synchronization

Long-term goals:

- Server-produced snapshots.
- Client interpolation of received state.
- Delta compression where useful.
- Client prediction only when explicitly scoped and reconciled.
- Validation of all client input on the server.

---

## 11. Build System and Dependencies

**Primary ownership:** `@carmack` for build/infrastructure, with subsystem owners for their
dependencies.

### 11.1 CMake Requirements

- Minimum CMake: 3.20.
- C++ standard: C++23.
- C99 enabled where required for miniaudio or similar C dependencies.
- Default tests should remain display-free.
- Optional GPU/display tests should be gated behind explicit CMake options.

### 11.2 External Dependencies

| Library | Purpose | Notes |
| --- | --- | --- |
| GLM | Math | Vectors, matrices, quaternions |
| SDL2 | Windowing/input | Platform abstraction and graphics context/surface support |
| miniaudio | Audio | Client-side playback and spatial audio |
| Vulkan SDK | Vulkan backend | Optional runtime path and opt-in context tests |
| OpenGL loader | OpenGL backend | Use the loader already selected by the project |
| cgltf | glTF import | Compile only when glTF support is enabled |
| stb_image | Image decode | Used by importer image paths |
| Test framework | Unit/integration tests | Keep default tests display-free |
| Doxygen | API docs | Public API documentation |

### 11.3 Build Targets

Representative targets:

- `stellar-client`
- `stellar-server`
- Shared engine/static libraries as organized by the current CMake layout.
- `stellar_import_gltf` when `STELLAR_ENABLE_GLTF=ON`.
- Display-free unit/regression tests.
- Opt-in OpenGL/Vulkan context tests behind explicit CMake flags.

### 11.4 Validation Commands

Default build and tests:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

glTF-focused validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Opt-in Vulkan context validation when a Vulkan-capable environment is available:

```bash
cmake -S . -B build-vulkan-tests \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON
cmake --build build-vulkan-tests --target stellar_vulkan_context_smoke_test -j$(nproc)
ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure
```

Opt-in OpenGL context validation should remain similarly gated.

---

## 12. Directory Structure

The exact tree may evolve, but source organization should keep public interfaces, implementation
files, tests, assets, and plans separated.

Representative structure:

```text
Project-Stellar/
├── AGENTS.md
├── CMakeLists.txt
├── Plans/
│   ├── Importer-Split.md
│   ├── Phase5E8.md
│   ├── Phase5E9.md
│   ├── Phase5E10.md
│   ├── Phase5F.md
│   ├── Phase6A-LevelCollisionExtraction.md
│   ├── Phase6B-CollisionQueriesAndMovement.md
│   ├── Phase6C-BillboardSpriteRendering.md
│   └── Phase6D-WorldMetadataFromGltf.md
├── docs/
│   ├── Design.md
│   └── ImplementationStatus.md
├── include/
│   └── stellar/
│       ├── assets/
│       ├── audio/
│       ├── core/
│       ├── ecs/
│       ├── graphics/
│       ├── import/
│       ├── network/
│       ├── physics/          # planned/Phase 6B direction if not already present
│       ├── platform/
│       └── scene/
├── src/
│   ├── audio/
│   ├── client/
│   ├── core/
│   ├── ecs/
│   ├── graphics/
│   │   ├── opengl/
│   │   └── vulkan/
│   ├── import/
│   │   └── gltf/
│   ├── network/
│   ├── physics/              # planned/Phase 6B direction if not already present
│   ├── platform/
│   ├── scene/
│   └── server/
├── tests/
│   ├── graphics/
│   ├── import/
│   │   └── gltf/
│   ├── physics/              # planned/Phase 6B tests if not already present
│   └── ...
├── assets/
│   └── shaders/
└── thirdparty/
```

Avoid adding public folders or top-level concepts that blur ownership unless an implementation plan
requires them.

---

## 13. Team Responsibilities and Agent Boundaries

`AGENTS.md` is the authoritative coordination document. This section summarizes the design-facing
ownership model.

### 13.1 Director

`@director` owns technical direction, routing, design compliance, conflict resolution, and
integration review. Cross-subsystem work should be coordinated by `@director`.

Examples of cross-subsystem work:

- glTF collision extraction, because it touches importer/scene assets and engine collision data.
- Billboard sprites, because rendering must align with gameplay/entity data contracts.
- World metadata extraction, because importer conventions must feed gameplay/world systems.
- Client-server movement, because gameplay rules, ECS storage, networking, and authority overlap.

### 13.2 Specialists

| Agent | Primary Domain |
| --- | --- |
| `@carmack` | ECS, core systems, networking, server authority, build/dependency plumbing, static collision/query infrastructure |
| `@miyamoto` | Graphics abstraction, OpenGL/Vulkan, glTF rendering, sprites, shaders, camera |
| `@suzuki` | Audio interfaces, miniaudio, playback, spatial audio, no-op fallback paths |
| `@kojima` | Entity archetypes, movement rules, collision responses, gameplay mechanics, tuning |
| `@molyneux` | Isolated experiments, prototypes, feasibility studies |

Specialists must not create new agents, delegate to other specialists, or route work. When a task
crosses domain boundaries, specialists should stop at the boundary and report the integration need
to `@director`.

### 13.3 Documentation Rules for Agent Work

- Do not modify `AGENTS.md` without explicit user approval.
- Do not modify `.kilo/agents/*.md` without explicit user approval.
- Keep active plan completion notes accurate when implementation plans require them.
- Keep `docs/ImplementationStatus.md` aligned with current branch status.
- Keep this document focused on broad architecture and long-term design intent.

---

## 14. Development Guidelines

### 14.1 Coding Standards

- **Style:** LLVM or Google C++ Style Guide.
- **Indentation:** 4 spaces, no tabs.
- **Line length:** 100 characters maximum.
- **Naming:**
  - `CamelCase` for types.
  - `snake_case` for functions and variables.
- **Headers:** `#pragma once`.
- **Namespace:** `stellar::` for library code.
- **Error handling:** prefer `std::expected<T, Error>` for fallible operations; do not use
  exceptions as normal control flow.
- **Documentation:** Doxygen `@brief` is mandatory for public APIs.

### 14.2 API Design Rules

1. Keep interfaces small and explicit.
2. Keep renderer/audio handles out of authoritative gameplay state.
3. Use RAII for resource management.
4. Keep ownership and lifetime visible in types.
5. Preserve `noexcept` on frame/draw/destruction functions where already established.
6. Return clear `std::expected` errors from fallible setup/import/upload paths.
7. Prefer backend-neutral data structures for assets and world state.
8. Keep implementation details in `.cpp` files or private headers.

### 14.3 Error Handling

- Fallible initialization, import, upload, and setup paths should return `std::expected`.
- Error messages should include enough operation-specific context for debugging.
- Unknown required glTF extensions should fail with the extension name in the error.
- Frame paths should recover or safely no-op on recoverable backend errors.
- Do not throw exceptions as normal control flow.

### 14.4 Threading Model

Long-term direction:

- Main thread handles rendering and platform/window messages.
- Worker threads may handle ECS jobs, background loading, networking, or tooling when scoped.
- Use thread pools or job systems rather than ad-hoc raw threads.
- Protect shared mutable state carefully.
- Preserve server authority and deterministic gameplay behavior where required.

---

## 15. Documentation Standards

### 15.1 Public API Documentation

Every public-facing type, function, and subsystem interface should include Doxygen comments.

Minimum required tag:

- `@brief`

Use additional tags where helpful:

- `@param`
- `@return`
- `@note`
- `@see`

Example:

```cpp
/**
 * @brief Performs a finite segment query against static level collision data.
 *
 * @param origin Segment origin in world space.
 * @param delta Segment delta in world-space units.
 * @return Nearest hit information if the segment intersects collision geometry.
 */
RaycastHit raycast(std::array<float, 3> origin,
                   std::array<float, 3> delta) const noexcept;
```

### 15.2 Architecture Documentation

Architecture documentation should describe:

- Server/client authority boundaries.
- ECS data ownership.
- Import and asset ownership.
- Renderer backend abstraction.
- Audio presentation boundaries.
- Collision/world metadata flow from authoring to runtime.

Keep diagrams under `docs/architecture/` if added.

### 15.3 Plan and Status Documentation

- Active plan files should include completion notes when their acceptance criteria require it.
- `docs/ImplementationStatus.md` should reflect current branch status and priorities.
- `docs/Design.md` should not overclaim implementation state when status docs are more specific.
- Historical plan files may be stale unless called out as active by `ImplementationStatus.md`.

---

## 16. Testing Strategy

### 16.1 Default Testing Principles

Default tests must be deterministic and display-free. They should not require:

- GPU access.
- OpenGL context creation.
- Vulkan instance/surface/swapchain creation.
- SDL display/window presentation.
- External artist assets.
- Shader compiler availability at test runtime.

### 16.2 Unit and Regression Tests

Representative coverage:

- ECS entity lifecycle and component storage.
- Serialization and packet handling.
- Math utilities.
- glTF importer regression tests.
- Asset model validation.
- Animation runtime and pose evaluation.
- Render scene upload/inspection with recording or mock graphics devices.
- Collision extraction tests.
- Collision query and movement tests.
- Backend selection behavior.

### 16.3 Importer and Asset Tests

Importer tests should cover:

- `.gltf` and `.glb`.
- External and embedded buffers/images.
- Data URI buffers/images.
- Materials, samplers, textures, texture transforms, and unlit materials.
- Bounds, tangents, UV routing, vertex colors.
- Skin and animation asset import.
- Unsupported extension behavior.
- Phase 6 collision node conventions.
- Phase 6 world metadata conventions.

### 16.4 Graphics Tests

Default graphics tests should use recording/mock devices to inspect backend-neutral behavior:

- Draw ordering.
- Alpha mask/blend grouping.
- Double-sided state.
- Texture/material binding data.
- Skin pose forwarding.
- Large scene submission.
- Future billboard sprite sorting and draw data.

OpenGL/Vulkan context tests remain opt-in and should be used for smoke coverage, not default CI.

### 16.5 Collision and Movement Tests

Phase 6A display-free tests should cover:

- Empty scenes.
- `COL_*` nodes.
- `Collision_*` nodes.
- Parent `Collision` node descendant extraction.
- Ordinary render mesh exclusion.
- Node transform application.
- Collision bounds.
- Triangle normals.

Phase 6B display-free tests should cover:

- Empty collision world misses.
- Segment/raycast floor hits.
- Nearest-hit selection.
- Outside-triangle misses.
- Wall stopping.
- Slide response.
- Ground probing.
- Empty-world movement pass-through.

### 16.6 Performance Testing

Long-term performance benchmarks may cover:

- Entity creation/destruction.
- ECS query/system throughput.
- Collision query throughput.
- Network snapshot size and serialization cost.
- Render scene submission cost.
- Sprite/billboard batch throughput.

Do not block current Phase 6 work on broad benchmark infrastructure unless a plan explicitly scopes
it.

---

## 17. Roadmap and Deferred Work

### 17.1 Active Near-Term Roadmap

1. Phase 6A — static level collision extraction from glTF.
2. Phase 6B — collision queries and minimal movement resolution.
3. Phase 6C — billboard sprite rendering in 3D world space.
4. Phase 6D — world metadata extraction from glTF node conventions.

### 17.2 Completed Recent Direction

Recent completed work includes:

- glTF importer decomposition into focused translation units.
- Vulkan skinned draw support.
- Vulkan resize/recreate/resource-lifetime hardening.
- Vulkan parity smoke matrix and Phase 5E completion.
- `KHR_texture_transform`.
- `KHR_materials_unlit`.
- Larger runtime skin palette cap of 256 joints per draw.
- Explicit deferral of full PBR, morph targets, glTF cameras, and glTF lights.

### 17.3 Deferred Rendering Work

Deferred unless future requirements justify it:

- Full metallic-roughness PBR.
- Image-based lighting and environment maps.
- Tone mapping and color-management policy.
- Deterministic GPU readback validation.
- Morph target deformation.
- glTF animation `weights` channels.
- glTF-authored cameras and lights.
- Broad glTF extension support.
- DirectX, Metal, or WebAssembly backends.

### 17.4 Deferred Gameplay and World Work

Deferred beyond active Phase 6 slices unless explicitly planned:

- Full rigid body physics engine integration.
- Dynamic collision objects.
- Character capsule/stair-step controller polish beyond minimal movement.
- Runtime trigger behavior.
- Runtime ECS/server spawning directly from glTF metadata.
- Navigation mesh/pathfinding.
- Editor tooling.
- Scripting.

### 17.5 Deferred Asset Pipeline Work

Deferred unless scoped:

- Sprite atlas packer.
- Sprite sheet animation system.
- Asset hot-reload.
- External artist asset validation suite.
- Full import/export editor workflow.

---

## 18. Change Log

| Date | Version | Author | Changes |
| --- | --- | --- | --- |
| 2026-04-20 | 0.1.0 | Initial draft | Initial document creation |
| 2026-04-23 | 0.1.1 | TBD | Replace SDL2 with SFML for windowing and input |
| 2026-04-23 | 0.1.2 | TBD | Add miniaudio for 3D spatial audio, update build to support C and C++ |
| 2026-04-24 | 0.1.3 | TBD | Replace SFML with SDL2 for windowing and input |
| 2026-04-29 | 0.1.4 | ChatGPT | Align design with `collision-movement` branch status, Phase 6 plans, AGENTS coordination rules, Vulkan parity status, current glTF support, and explicit deferred work |

---

## Appendix A: Glossary

- **Billboard:** A 2D sprite that faces the camera in 3D world space.
- **Server authority:** The server is the source of truth for gameplay state.
- **Presentation client:** Client process responsible for rendering, audio, input capture, and
  presentation of server-owned state.
- **Backend-neutral asset:** CPU-side data that does not depend on OpenGL, Vulkan, or another
  presentation backend.
- **Static level collision:** Non-dynamic world collision geometry, usually imported from authored
  level data.
- **World metadata:** Authored markers such as spawn points, trigger markers, and sprite placements.
- **Snapshot:** Serialized state sent from server to client.
- **Interpolation:** Presentation-side smoothing between received snapshots.
- **Client prediction:** Optional future local simulation that must reconcile with server authority.
- **Lightweight material parity:** OpenGL/Vulkan rendering of the engine's supported material subset
  without claiming full glTF PBR compliance.

## Appendix B: Reference Materials

- *Game Programming Patterns* — Robert Nystrom.
- EnTT documentation and ECS design references.
- Vulkan Tutorial and Vulkan specification materials.
- OpenGL 4.5 specification materials.
- Khronos glTF 2.0 specification and extension registry.
- miniaudio documentation.
