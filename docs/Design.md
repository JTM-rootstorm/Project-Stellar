# Stellar Engine - Game Framework Development Document

**Project:** Stellar Engine  
**Target Platform:** Linux/macOS with cross-platform architecture
**Language:** C++23, C99 where required for single-file C dependencies such as miniaudio  
**Build System:** CMake 3.20+  
**Version:** 0.3.5 (Linux Vulkan and macOS Metal renderer matrix)
**Last Updated:** 2026-05-07

---

## Table of Contents

1. <a href="#1-project-overview">Project Overview</a>
2. <a href="#2-documentation-authority">Documentation Authority</a>
3. <a href="#3-current-branch-direction">Current Branch Direction</a>
4. <a href="#4-system-architecture">System Architecture</a>
5. <a href="#5-core-engine-subsystem">Core Engine Subsystem</a>
6. <a href="#6-asset-import-and-world-authoring">Asset, Import, and World Authoring</a>
7. <a href="#7-graphics-subsystem">Graphics Subsystem</a>
8. <a href="#8-audio-subsystem">Audio Subsystem</a>
9. <a href="#9-entity-component-system">Entity Component System</a>
9.5. <a href="#95-server-authoritative-scripting">Server-Authoritative Scripting</a>
10. <a href="#10-networking-and-client-server-model">Networking and Client-Server Model</a>
11. <a href="#11-build-system-and-dependencies">Build System and Dependencies</a>
12. <a href="#12-directory-structure">Directory Structure</a>
13. <a href="#13-team-responsibilities-and-agent-boundaries">Team Responsibilities and Agent Boundaries</a>
14. <a href="#14-development-guidelines">Development Guidelines</a>
15. <a href="#15-documentation-standards">Documentation Standards</a>
16. <a href="#16-testing-strategy">Testing Strategy</a>
17. <a href="#17-roadmap-and-deferred-work">Roadmap and Deferred Work</a>
18. <a href="#18-change-log">Change Log</a>

---

## 1. Project Overview

Stellar Engine is a modular, cross-platform game framework for creating games that use
3D BSP level geometry with 2D billboard entities, objects, pickups, projectiles, and props.
The engine now has explicit client/server runtime modes. Authority-side code owns game rules,
ECS/runtime state, collision, gameplay scripts, and snapshot production; client-side code owns
windowing, rendering, audio, input capture, and presentation of authoritative snapshots.

The supported runtime modes are single-player no-server mode, remote client mode, listen-server host
mode, and dedicated-server mode. Single-player runs authoritative simulation in-process without a
server listener or client/server handshake. Remote clients are presentation terminals over transport.
Listen and dedicated servers share the server runtime and authority bootstrap. The current engine
direction is not to become a full physically based renderer. BSP maps are the canonical playable level
source, and active runtime, server, client validation, and rendering paths use BSP-backed,
source-neutral `LevelAsset` data.
The rendering target is lightweight BSP surface/material and billboard rendering suitable for game
content. Linux uses Vulkan as the supported/default renderer. macOS uses the native Metal backend.
OpenGL has been retired from active support after the Linux Vulkan migration. Vulkan is
intentionally Linux-only; macOS Vulkan/MoltenVK is not part of the project renderer matrix.
Display-attached smoke/readback validation remains opt-in and environment-gated.

### Key Design Principles

- **Server/authority ownership:** authority-side runtimes own game state, game rules, ECS
  simulation, physics decisions, and validation.
- **Client as presentation:** the client renders and presents server state, captures input,
  and forwards input to the server. Rendering and audio are never sources of gameplay truth.
- **Modularity:** graphics, audio, import, networking, and core systems are isolated behind
  explicit interfaces.
- **Data-oriented ECS:** gameplay state should remain serializable, cache-friendly, and
  server-owned.
- **Renderer abstraction:** Linux Vulkan and macOS Metal use the same backend-neutral graphics
  contracts.
- **Display-free default validation:** default tests must not require a GPU, display, or runtime
  graphics context.
- **Server-authoritative scripting:** gameplay scripts run on the authoritative runtime side and
  emit validated events/commands through explicit C++ APIs; they do not directly mutate renderer,
  audio, platform, OS, or raw C++ state.
- **Documentation-driven public API:** public APIs require Doxygen `@brief` documentation.

### Technical Stack

- **Language:** C++23 plus C99 where required for C dependencies.
- **Build:** CMake 3.20+.
- **Rendering:** Vulkan on Linux; Metal on Apple platforms.
- **World style:** 3D static world geometry with 2D billboard entities and props.
- **Asset format:** BSP maps are the canonical playable level format.
- **Math:** GLM.
- **Windowing/input abstraction:** SDL2.
- **Audio:** miniaudio-backed audio when audio implementation is in scope, with explicit
  no-op fallback behavior where needed.
- **Scripting:** mandatory vendored Lua 5.4.x behind the engine-owned `stellar_scripting` wrapper.
- **Networking:** protocol DTOs, local loopback transport, and Linux/POSIX TCP socket transport are
  split behind target boundaries, with remote client, listen-server, dedicated-server, and
  single-player no-server entry points.

---

## 2. Documentation Authority

This file describes broad architecture and long-term design intent. For current branch-facing work, it
is not the highest authority on implementation status.

Precedence for resolving conflicts:

1. `docs/ImplementationStatus.md` — current branch-facing implementation status and
   near-term priorities.
2. Active plans under `Plans/` — current implementation handoff plans when referenced
   by `ImplementationStatus.md`.
3. `docs/Design.md` — broad architecture and long-term design intent.
4. `AGENTS.md` and `.kilo/agents/*.md` — coordination rules, ownership, and agent behavior.

Historical roadmap files under `Plans/` and `.kilo/plans/` may contain stale intermediate
state unless `docs/ImplementationStatus.md` identifies them as current. Completion notes in
plan files remain useful for understanding what was implemented, validated, or explicitly
deferred.

---

## 3. Current Branch Direction

Current branch direction: client/server decoupling is complete through Phase CS-9, and lightweight
BSP normal/specular material sidecars are complete through SNT-8. The branch treats protocol,
transport, authority bootstrap, server runtime, dedicated server, single-player, remote client,
listen-server, presentation, and client application composition as explicit modules with
build-enforced target boundaries.

Primary near-term status: the client/server split is complete over the already completed BSP gameplay,
socket transport, TrenchBroom BSP30, and presentation-material foundations. Single-player no-server
mode, remote client mode, listen-server host mode, and dedicated-server mode are documented runtime
contracts. Historical Z-up migration, TrenchBroom BSP30 authoring, collision, movement, Lua scripting,
object-collider, BSP canonical migration, BSP hardening, BSP gameplay-loop, presentation/networking
polish, socket transport, and spec/normal sidecar work remain foundational context and must not be
restarted.

The completed scope includes a Linux/POSIX TCP-first socket backend, deterministic
`ClientHello`/`ServerWelcome`, the headless `stellar-server`, `stellar-client --connect HOST:PORT`
remote presentation mode, `stellar-client --map path/to/map.bsp` single-player authority in-process
without server startup, and `stellar-client --host --map path/to/map.bsp [--listen HOST:PORT]`
listen-server host composition.

Completed BSP hardening added actionable diagnostics, source-neutral PVS and lightmap/material data
contracts, optional presentation-only render culling, BSP entity authoring conventions, and
deterministic headless validation.

Completed BSP material sidecar work keeps BSP files responsible for geometry, face texture names,
lightmaps, visibility, entities, and collision while allowing optional `.stellar_material` files to
enrich presentation materials by BSP texture name. Missing sidecars preserve existing texture/lightmap
fallback behavior. Server-authoritative gameplay, networking, collision, and scripting do not depend
on material sidecars.

The BSP gameplay-loop branch has completed its selected Phases 0-8, and BSP presentation/networking
polish PN-0 through PN-6 is also complete. Completion notes live in `docs/ImplementationStatus.md`.
The completed gameplay-loop plans are archived under `Plans/Archived/bsp_gameplay_loop/`, and the
completed presentation/networking polish plans are archived under
`Plans/Archived/bsp_presentation_networking_polish/`.

Gameplay authoring and runtime tuning for this branch use inch-scale Z-up world units: 1 Stellar
gameplay world unit equals 1 inch, BSP30 authored coordinates are imported without hidden scale or axis
conversion, and `stellar::core::WorldUnits.hpp` plus `stellar::core::WorldAxes.hpp` record the shared
code constants. The default authoritative player capsule is 72 inches tall with a 16 inch radius, so
player spawn centers should be authored at `z = 36` above a floor at `z = 0` unless a later archetype
explicitly changes the capsule.
Default walk tuning is 160 inches/second max speed, 1200 inches/second² acceleration, 800
inches/second² gravity, and 2400 inches/second terminal fall speed; these are game-feel defaults, not
real-world simulation claims.

BSP entity metadata binds triggers, object-collider sensors, sprite markers, spawns, and script
IDs/tables, but import does not execute scripts. Runtime scripting wraps authoritative
movement/session output, emits primitive script events, and applies only native-validated
collision/object-collider commands to server-owned runtime state.
The live single-player client path can load a configured BSP map, keep the validated `LevelAsset`
alive for `RuntimeWorld`, instantiate in-process single-player authority state without a server
host/transport handshake, advance movement from captured input, and render the level from the
authoritative player presentation camera. No-map mode
initializes without authored static geometry and renders a blank/static-less presentation frame until
authored static geometry or authoritative dynamic snapshot data is available.
The live client uses the same server-authoritative scripted runtime as display-free tests when a loaded
BSP map declares trigger or object-collider script bindings. Script sources are loaded for
authoritative runtime execution only; no client-side gameplay scripting or renderer/audio script
binding is permitted.
Pickup object-collider enters are collected through a native `gameplay.collect_pickup` command emitted
by the authoritative object-collider script system, so pickup active/inactive state and collider
disablement remain server-owned. Scripted doors/gates are static named collision meshes toggled by
sandboxed Lua output events such as `collision.set_mesh_enabled`; accepted commands also update
server gameplay metadata (`open`/`active`) for presentation snapshots.
Authoritative gameplay snapshots are converted into client-side billboards, HUD data, or audio
requests, but those systems remain disposable presentation caches/routes and never mutate server
gameplay truth. Snapshot, delta, and event contracts are deterministic, serializable, and
server-authored. The implemented transport layer includes local/in-memory endpoints and a TCP-first,
Linux/POSIX socket backend using a length-prefixed packet envelope. The `stellar-server` executable
loads and validates BSP maps headlessly, loads referenced authoritative Lua scripts from the map
directory or configured script root, accepts one TCP client, performs `ClientHello`/`ServerWelcome`,
assigns the authoritative player id, ticks the server-owned runtime, and sends the first full snapshot
followed by deltas/events. The `stellar-client --connect HOST:PORT` mode creates only a socket client
runtime, sends `ClientHello`, waits for accepted `ServerWelcome`, sends input commands after acceptance,
and renders camera/billboards from `NetworkWorldSnapshot` through `ClientWorldReceiver`. Remote mode
does not load maps, load gameplay scripts, construct local authority, transfer maps, predict,
interpolate, or reconcile. Without a local presentation map feature, remote mode renders a
blank/static-less frame plus any available dynamic network state; static BSP map transfer/caching
remains deferred. True simultaneous
multiplayer simulation, interpolation, prediction, and reconciliation remain deferred until explicitly
scoped.

Lua runtime, collision filtering, scripted triggers, object-collider sensors, BSP canonical import,
and retired importer removal are complete historical implementation steps. Their completion notes
live in `docs/ImplementationStatus.md` and archived plans rather than defining the active scope here.

Avoid spending the next implementation slices on third-party physics, dynamic rigid bodies,
client-side scripting, renderer/audio scripting bindings, full PBR, Source/VBSP, moving brush classes
beyond the implemented door/button path, or model/animation systems unless a concrete requirement
appears.

---

## 4. System Architecture

### High-Level Architecture

```text
┌──────────────────────────────┐
│ Client Presentation Runtime  │  ← window, input, graphics, audio, HUD, render caches
└───────────────┬──────────────┘
                │ snapshots/events/commands
┌───────────────▼──────────────┐
│ Shared Protocol + Transport  │  ← DTOs/codecs, loopback, TCP/POSIX packet envelope
└───────────────┬──────────────┘
                │
┌───────────────▼──────────────┐
│ Authority Runtime            │  ← BSP load, Lua scripts, world/session, validation
└───────────────┬──────────────┘
                │
┌───────────────▼──────────────┐
│ Server Runtime / Host        │  ← listen/dedicated session lifecycle and snapshots
└──────────────────────────────┘

Runtime compositions:
- Single-player: client presentation + in-process authority, no server host and no handshake.
- Remote client: client presentation + transport client only; authority is remote.
- Listen server: client presentation + in-process server host + authority + optional listener.
- Dedicated server: headless server runtime + authority + listener, no client presentation.
```

### Runtime Mode Matrix

| Mode | Entry Point | Authority Owner | Transport/Listener | Presentation Ownership |
| --- | --- | --- | --- | --- |
| Single-player no-server | `stellar-client --map path/to/map.bsp` | In-process authority facade | no socket listener; no `ClientHello`/`ServerWelcome` | local client |
| Remote client | `stellar-client --connect HOST:PORT` | remote dedicated/listen server | socket client only | local client |
| Listen server | `stellar-client --host --map path/to/map.bsp [--listen HOST:PORT]` | in-process server host | loopback/in-process host path plus optional listener | host client |
| Dedicated server | `stellar-server --map path/to/map.bsp --listen HOST:PORT` | server process | socket listener | none/headless |

### Client Responsibilities

- Initialize platform windowing, graphics, audio, input, and asset presentation paths.
- Select the platform renderer through the graphics abstraction.
- Receive world snapshots or render commands from the server.
- Present received authoritative state without treating presentation smoothing as authority;
  interpolation remains deferred until explicitly scoped.
- Render static BSP level geometry, billboard sprites, and UI.
- Capture input and forward it to the server.
- Play local presentation audio based on server-approved events or presentation state.
- Compose one client-facing runtime interface for single-player, remote-client, listen-host, or
  static-less fallback modes.

### Server Responsibilities

- Own authoritative ECS state.
- Process client input as requests, not trusted state.
- Run deterministic gameplay systems where practical.
- Own collision and movement decisions.
- Run server-authoritative gameplay scripts after native validation/simulation seams, and treat
  script output as validated requests/events rather than direct state mutation.
- Validate game rules.
- Produce world snapshots or state deltas for clients.
- Keep gameplay state serializable and independent of renderer/audio backends.
- Share authority bootstrap and server runtime code between listen-server and dedicated-server modes.

### Server Authority Model

- Server is the single source of truth for gameplay state.
- Client prediction may be added later, but it must be explicitly scoped and reconciled against
  server state.
- Rendering, audio, and client-only interpolation cannot become sources of authoritative gameplay
  truth.
- Imported collision and world metadata must be usable by server/gameplay code without graphics
  backend dependencies.

---

## 5. Core Engine Subsystem

**Primary ownership:** `@carmack`, coordinated by `@director` for cross-subsystem work.  
**Responsibility:** core systems, ECS surfaces, world management, serialization, networking,
platform infrastructure, and static collision/query infrastructure.

### 5.1 Application and Server Entry Points

#### Client Application

- Initialize platform, window, graphics backend, audio, input, and client asset paths.
- Select one explicit runtime mode from CLI/config:
  - `stellar-client --map path/to/map.bsp` starts single-player no-server mode with in-process
    authority and no transport handshake.
  - `stellar-client --connect HOST:PORT` starts remote-client mode with socket transport and no local
    authority or gameplay script loading.
  - `stellar-client --host --map path/to/map.bsp [--listen HOST:PORT]` starts listen-server mode,
    tying an in-process server host lifetime to the host client.
  - no-map/static-less startup may initialize presentation without authored static geometry.
- Receive and present authoritative state through a single client-facing runtime frame path.
- Submit graphics work through the shared abstraction.
- Keep presentation behavior separate from server authority.

#### Server Application and Authority Entry Points

- Initialize ECS/runtime world and authoritative game systems through the shared authority bootstrap.
- Load backend-neutral BSP world data and referenced authoritative script sources on the authority side.
- Own collision queries and movement validation.
- Run simulation ticks.
- Serialize snapshots, deltas, and server-approved gameplay events for clients.
- Provide `stellar-server --validate-config --map path/to/map.bsp` for display-free map/script
  validation before opening sockets.
- Provide `stellar-server --map path/to/map.bsp --listen HOST:PORT` as a headless dedicated-server
  wrapper around `stellar_server_runtime`.
- Reuse the same server runtime/session lifecycle from listen-server mode; dedicated and listen hosts
  differ in process lifetime and presentation ownership, not authority rules.

### 5.2 Core World Data

Core world data should remain backend-neutral and serializable. Render-specific resources, audio
graphics API handles/objects and client-only presentation caches must not be stored as
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

This model is represented on source-neutral `LevelAsset` data. An empty mesh list means no collision.

Imported static collision remains immutable asset data. Runtime gameplay that needs to open doors,
gates, traps, or one-way blockers uses a server-owned collision-state overlay keyed by authored
collision mesh names. The overlay stores enable/disable state only; it does not rebuild assets,
mutate `CollisionWorld`, or introduce dynamic rigid bodies. Empty collision mesh names are not
targetable, and duplicate names are diagnosed deterministically and toggled together by name.

### 5.4 Collision Query Direction

Phase 6B and Phase 11 build on Phase 6A data with a small backend-neutral collision query module.
It should support:

- Finite segment/raycast queries.
- Nearest-hit selection.
- Ground probing.
- Minimal sweep-and-slide movement.
- Simple deterministic behavior over static collision geometry.
- Optional runtime mesh filters for server-owned enable/disable state.
- Character capsule movement and authoritative trigger overlap using consistent capsule dimensions.
- Display-free tests.

Do not introduce full rigid body simulation, dynamic bodies, navigation meshes, or broad physics
engine integration in this collision-state path.

---

## 6. Asset, Import, and World Authoring

**Primary ownership:** import/render asset routing is usually `@miyamoto`; backend-neutral
collision/world metadata work is usually `@carmack`, coordinated by `@director` when work crosses
graphics/import/gameplay boundaries.

### 6.1 BSP Support Status

BSP maps are the canonical playable level format for the active branch. The migration introduced a
source-neutral `LevelAsset` contract and a mandatory importer for the classic Quake/GoldSrc BSP
family.

Current BSP support covers:

- Legacy BSP and BSP30 headers, lump tables, geometry, texture names, entities, visibility, and
  lighting data where available. BSP30 is the TrenchBroom workflow target.
- Static world geometry represented as backend-neutral mesh primitives.
- Optional `.stellar_material` sidecars resolved from BSP texture names for presentation-only
  normal/specular material enrichment.
- Static collision represented as named triangle collision meshes and optional BSP hull data.
- Entity key/value metadata for player spawns, generic spawns, triggers, sprite markers,
  object-collider sensors, and script bindings.
- Display-free importer, runtime, server, scripting, and graphics submission tests.

### 6.2 BSP Entity and Metadata Conventions

BSP entity key/value data is preserved in order and mapped to source-neutral world metadata.
The current authoring guide and key reference live in `docs/BspAuthoring.md`.

Initial mappings:

- `info_player_start` creates a player spawn marker.
- `info_player_deathmatch` may create a player or entity spawn marker.
- `info_stellar_spawn` creates a generic entity spawn marker.
- `trigger_multiple`, `trigger_once`, and `trigger_stellar` create trigger markers.
- `env_sprite`, `stellar_sprite`, and entities with `stellar.sprite` create sprite markers.
- `stellar_object_collider` and entities with `stellar.collider=object` create sensor-style object
  collider markers.
- Brush entities such as `func_wall`, `func_illusionary`, and `func_detail` contribute static or
  nonblocking brush metadata. `func_door` and `func_button` are implemented as server-authoritative
  brush movers with target routing, collision overlay transforms, and replicated presentation
  transforms.

Script bindings use BSP entity keys such as `stellar.script`, importer-supported aliases such as
`_stellar_script`, `stellar.table`, and `_stellar_table`. Dotted Stellar keys must reach the compiled
BSP as dotted keys or importer-supported aliases; underscore FGD field names are editor-facing
placeholders unless the editor/toolchain remaps them before export. Import preserves bindings as
import-time metadata; the authoritative scripting layer loads and invokes them later. For object
colliders, native runtime collider ids are assigned
deterministically from metadata marker order and are used for validated commands rather than
name-based mutation.

Brush-backed trigger/object-collider markers derive bounds from `model="*N"`; point-authored
volumes use `origin` plus `stellar.extents`. Sprite script bindings are unsupported and diagnosed.

### 6.3 BSP Non-Goals

Outside the current Stellar BSP30 profile unless a concrete requirement appears:

- Source/VBSP support.
- Arbitrary third-party WAD/material library tooling beyond the safe Stellar WAD/developer-material
  workflow.
- Moving brush classes beyond the implemented `func_door`/`func_button` path, such as plats, trains,
  and rotators.
- Dynamic rigid bodies.
- Navigation mesh/pathfinding.
- Full PBR.
- 3D model/skinning/animation systems.
- Client-side gameplay scripting.

---

## 7. Graphics Subsystem

**Primary ownership:** `@miyamoto`, coordinated by `@director` for cross-subsystem changes.  
**Responsibility:** graphics abstraction, Vulkan and Metal backends, level geometry upload, shader
paths, materials, cameras, BSP level rendering, billboard sprite rendering, and graphics tests.

### 7.1 Graphics Abstraction

Graphics backends are hidden behind a shared interface. Vulkan is the supported Linux backend, and
Metal is the supported macOS backend in Apple-gated builds. Both receive backend-neutral level
geometry, material, texture, and sprite data. Future native backends should reuse the same contracts
when their implementations are explicitly scoped.

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

    // Mesh, texture, material, level, and billboard APIs remain backend-neutral.
};

} // namespace stellar::graphics
```

Frame functions and draw paths that are currently `noexcept` must remain safe: invalid or unsupported
draw commands should log, skip, or return an error from fallible setup/upload paths rather than throw.

### 7.2 Rendering Backend Status

Current branch status:

- Linux uses Vulkan as the supported/default renderer through the shared abstraction.
- macOS uses the Apple-gated native Metal backend.
- OpenGL has been retired from active support after the Linux Vulkan migration.
- Vulkan is intentionally Linux-only; macOS Vulkan/MoltenVK is not part of the project renderer
  matrix.
- Vulkan and Metal both support static mesh buffers, RGB/RGBA textures, material records,
  lightmaps, normal/specular maps, metallic/roughness, occlusion, emissive maps, alpha behavior,
  culling, unlit behavior, and camera-dependent specular.
- Default tests remain display-free.
- GPU/display-dependent Vulkan and Metal context tests remain opt-in.

### 7.3 Lightweight BSP Material Parity

The current target is lightweight BSP surface/material rendering in Vulkan and Metal, including:

- Base color factor and texture.
- Vertex color modulation.
- Normal maps when BSP texinfo-derived tangents are available.
- Lightweight specular maps, factors, and power for presentation shading.
- Emissive texture and factor.
- Alpha opaque, mask, and blend behavior.
- Double-sided material/culling behavior.
- BSP texture/material name preservation.
- Optional `.stellar_material` sidecars keyed by BSP texture name, with safe relative texture paths.
- Placeholder/default materials when texture decode, sidecar lookup, or external material lookup is
  incomplete.
- Static level mesh rendering.

Do not describe this as full PBR. Full PBR requires future decisions around image-based
lighting, environment maps, tone mapping, color management, physically based BRDF parity, and
deterministic validation strategy.

### 7.4 Sprite and Billboard Rendering Direction

The current renderer supports 2D billboard sprites in 3D world space through backend-neutral draw data.

Preferred behavior:

- Sprite center is a world-space position.
- Sprite size is world-space width and height.
- Camera right/up vectors produce spherical billboarding.
- Depth testing lets level geometry occlude sprites.
- Alpha blend sprites are sorted back-to-front by camera-space depth.
- Alpha mask and opaque sprites can be handled separately from alpha blend sprites.
- Backend-neutral sprite data should remain reusable by future renderer backends.
- Display-free tests validate draw data, sorting, alpha classification, and preservation of size,
  color, and texture handles.

Sprite atlas packing, sprite sheet animation, particles, and ECS integration are deferred unless
explicitly added by a later plan.

### 7.5 Camera System

Current long-term camera goals:

- 3D perspective camera for main world rendering.
- Follow-player or scripted camera support.
- View/projection matrix generation.
- Camera basis export for billboard generation.
- Bounds/framing utilities for rendering tests and tools.

BSP-authored camera behavior is not part of the active migration. Runtime camera selection should
remain engine-owned until a future plan explicitly adds imported camera support.

---

## 8. Audio Subsystem

**Primary ownership:** `@suzuki`, coordinated by `@director` for gameplay/audio integration.  
**Responsibility:** audio interfaces, miniaudio-backed playback, listener updates, spatial audio,
and no-op fallback paths where appropriate.

### 8.1 Audio Direction

Audio should be implemented through an interface that keeps gameplay/server logic separate from
presentation playback. The client owns playback; gameplay/server systems should produce validated
events or state that the client can present.
The current presentation polish includes a narrow `AudioEventRouter` that maps pickup/door/footstep
events and optional script-error diagnostics from server-approved `GameplayEvent` records to one-shot
sound ids. Footstep events are emitted by authoritative grounded movement cadence; the surface
category comes from BSP source texture/material names carried on collision triangles through a small
server-safe resolver. Optional `.stellar_material` render sidecars remain presentation-only and are
not an authoritative input for footstep gameplay.

`AudioEventRouter` targets an abstract request sink. Production has `NoOpAudioRequestSink` for
default no-device behavior and an optional `MiniaudioRequestSink` selected by the client when
`STELLAR_ENABLE_AUDIO=1` is set. Missing sound ids, missing local assets, uninitialized sinks, decode
failures, initialization failures, and playback failures are presentation diagnostics. They do not
affect authoritative gameplay. Generated retro footstep WAVs are placeholder presentation assets for
the current slice, and default tests validate their registry and decode path without opening an audio
device. Audio never affects authoritative gameplay.

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

For the BSP gameplay-loop branch, `server::GameplayWorld` is the minimal server-owned entity model
used before a full ECS rewrite. It allocates deterministic `EntityId` values in spawn order, binds the
configured local `PlayerId` to the first player spawn entity when available, and exposes display-free
snapshots containing plain transform plus inert metadata components. Spawn/import does not execute
scripts and never stores renderer, texture, audio, or backend handles. Current metadata-driven entity
kinds cover player, sprite marker, pickup candidate, door/gate, trigger, and object-collider records;
presentation systems may bind sprite ids later from copied metadata while the server remains the source
of gameplay truth. Pickup and door/gate interaction state is now represented in authoritative gameplay
snapshots: collected pickups become inactive, object-collider pickup sensors can be disabled through
validated native commands, and named static gate meshes mirror open/closed state after accepted Lua
collision commands.

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

## 9.5 Server-Authoritative Scripting

**Primary ownership:** `@carmack` for runtime infrastructure and server authority seams, coordinated
by `@director` when scripts need gameplay, rendering, audio, or tooling integration.

Lua scripting is gameplay code and therefore runs on the authoritative server/runtime side. The
initial implementation is intentionally narrow:

- `stellar_scripting` is core server-authoritative infrastructure and owns the Lua dependency.
- `stellar_world` and `stellar_server_core` do not link Lua directly.
- `LuaRuntime` always installs the restricted sandbox; unrestricted standard-library access is not a
  normal gameplay runtime configuration path.
- Import extracts script bindings from world metadata but never executes scripts.
- `RuntimeWorld` preserves authored script bindings on trigger and object-collider markers.
- `ScriptedWorldSession` wraps `server::WorldSession`, advances native movement first, then invokes
  trigger callbacks from authoritative `MovementTriggerEvent` data followed by object-collider
  callbacks from authoritative `WorldSnapshot::object_collider_events` data.
- Scripts receive plain event tables, not raw C++ pointers.
- Scripts emit primitive output events through `stellar.emit_event`; native server/gameplay code must
  validate and apply those outputs explicitly.
- The initial native command vocabulary supports `collision.set_mesh_enabled` with `mesh` and
  `enabled` fields. The command processor validates fields and applies approved changes through
  `server::WorldSession`, not through Lua-owned state.
- The object-collider command vocabulary supports `object_collider.set_enabled` with finite
  integer-like `id` and boolean `enabled` fields. Native code validates the id and applies approved
  changes through `server::WorldSession`; disabling an overlapped collider surfaces its deterministic
  exit synchronously on the command result rather than replaying it on later snapshot ticks.
- Script-emitted collision commands are processed after native movement and trigger callbacks for
  the completed tick. Object-collider commands are processed after object-collider callbacks for the
  completed tick. State changes affect subsequent authoritative ticks.
- Runtime script errors become deterministic `ScriptError` diagnostics and do not crash the session.

Supported initial hooks:

```lua
function Door.on_trigger_enter(event) end
function Door.on_trigger_stay(event) end
function Door.on_trigger_exit(event) end

function PickupGem.on_object_collider_enter(event) end
function PickupGem.on_object_collider_stay(event) end
function PickupGem.on_object_collider_exit(event) end
```

Initial event fields include authoritative trigger or collider names, object collider ids, player id,
and tick. Lua receives plain values and emits primitive requests/events; it does not directly mutate
C++ state or access raw engine pointers.

Backend-neutral object collider overlap support is a plain runtime sensor registry. It is not a rigid
body system, does not block movement, and does not imply ECS ownership, network replication, renderer
debug views, or moving/solid blockers until those contracts are explicitly scoped.

---

## 10. Networking and Client-Server Model

**Primary ownership:** `@carmack`, coordinated by `@director` for cross-subsystem integration.

### 10.1 Transport Direction

The networking and runtime split supports:

- Pure protocol DTOs/codecs in `stellar_protocol`.
- Transport interfaces and implementations in `stellar_transport`, including local loopback and the
  Linux/POSIX TCP-first socket backend.
- Remote client mode through `stellar_client_net` without authority, server runtime, or scripting
  links.
- Listen-server and dedicated-server modes through shared authority and server runtime modules.
- Single-player no-server mode through in-process authoritative simulation, intentionally not through
  socket startup or `ClientHello`/`ServerWelcome`.
- Serializable ECS/world snapshots and gameplay events for presentation.

Potential protocol split:

- UDP for frequent state updates.
- Reliable transport for connection setup, critical events, chat, or other ordered messages.

Exact future protocol choices remain implementation details until a networking plan scopes them. The
completed client/server split preserves deterministic `ClientHello` / `ServerWelcome` session setup for
networked modes, protocol/map compatibility rejection, server-assigned player authority before the
first snapshot is accepted, and a Linux/POSIX TCP-first transport with `host:port` endpoint parsing plus
a compact length-prefixed packet envelope (`STCP` magic, version byte, channel byte, and little-endian
`uint32` payload length). True simultaneous multiplayer simulation, prediction, interpolation,
reconciliation, UDP/unreliable transport, and map transfer/caching remain deferred until explicitly
scoped.

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

The first-stage lifecycle supports one active local player slot over a multi-client-ready protocol
shape. The client sends a bounded `ClientHello` with `ProtocolVersion`, client diagnostics, requested
map id, and nonce. The server replies with `ServerWelcome`, either accepted with a `SessionId` and
authoritative `PlayerId`, or rejected with stable protocol/map diagnostics. Input before an accepted
welcome is deterministically ignored/rejected, and the server overwrites requested player ids with the
assigned authoritative player id.

### 10.3 State Synchronization

Long-term goals:

- Server-produced snapshots.
- Client interpolation of received state.
- Delta compression where useful.
- Client prediction only when explicitly scoped and reconciled.
- Validation of all client input on the server.

Current contract behavior after the client/server split:

- Client input commands are requests and are validated by the authoritative runtime.
- Server-owned `WorldSnapshot` and `GameplayWorldSnapshot` data are the source for client
  presentation.
- Server-approved gameplay events may drive HUD, VFX, or audio presentation, but do not grant client
  gameplay authority.
- Snapshot baselines, structural deltas, and event records should use deterministic serialization with
  bounded strings/vectors and finite numeric data.
- Protocol contracts live in `stellar_protocol` as `NetworkPlayerCommand`, `ClientHello`,
  `ServerWelcome`, `MapIdentity`, `NetworkWorldSnapshot`, `NetworkGameplayEntity`, `GameplayEvent`,
  and `SnapshotDelta`. The binary `SnapshotCodec` is transport-neutral and intentionally does not add
  prediction or reconciliation.
- Transport endpoints live in `stellar_transport`; local loopback transport is a transport primitive,
  not a client-owned authority bridge.
- `stellar_authority` owns BSP/script/runtime preparation and authority-side conversion to protocol
  snapshots/events.
- `stellar_server_runtime` owns networked server session lifecycle for dedicated and listen-host modes.
- `stellar_single_player` owns local authoritative stepping for `stellar-client --map` without opening
  sockets, constructing a server host, sending `ClientHello`, or waiting for `ServerWelcome`.
- `stellar_client_net` owns remote/listen client connection state, input sequencing, and
  `ClientWorldReceiver`; it must not link authority, server runtime/core, or scripting.
- The TCP socket backend preserves the same opaque packet contract and reliable FIFO semantics for one
  accepted client. It rejects oversized payloads before allocation, handles partial read/write paths
  with bounded nonblocking loops, and keeps UDP, multi-client simulation, prediction, and
  reconciliation out of scope.
- The dedicated server entry point uses the TCP socket backend for one accepted client, validates the
  `ClientHello` protocol and any non-empty requested map identity before accepting input, sends
  `ServerWelcome`, assigns the authoritative player id, emits the first full `NetworkWorldSnapshot`,
  then emits structural deltas and server-approved gameplay events from server-owned ticks.
- The remote client connect path uses `--connect HOST:PORT` without `--map`, sends an empty requested
  map id when no presentation map exists, accepts the server's map identity in `ServerWelcome`, and
  presents only received network snapshots/events. `--map` plus `--connect` is rejected as ambiguous,
  and `--script-root` is invalid in remote mode because scripts are server-authoritative only.
- Listen-server mode uses the same server runtime/authority bootstrap as dedicated server. Its lifetime
  is bound to the host client, and the current implementation preserves the existing one accepted
  TCP-client/one active-player limit unless a later true-multiplayer plan expands it.

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
| Vulkan SDK / loader / shader tools | Linux graphics backend | Required for Linux Vulkan builds |
| Metal frameworks | macOS graphics backend | Enabled only by `STELLAR_ENABLE_METAL=ON` on Apple platforms |
| Lua 5.4.x | Server-authoritative scripting | Vendored and linked only by `stellar_scripting` |
| stb_image | Image decode | Used by importer image paths |
| Test framework | Unit/integration tests | Keep default tests display-free |
| Doxygen | API docs | Public API documentation |

### 11.3 Build Targets

Client/server split production targets:

- `stellar_protocol` — pure protocol DTOs/codecs such as session ids, hello/welcome, commands,
  snapshots, gameplay entities, events, deltas, and session codecs.
- `stellar_transport` — transport interfaces and implementations, including loopback transport,
  TCP/POSIX socket transport, endpoint parsing, and packet envelope handling.
- `stellar_authority` — backend-neutral authoritative BSP/script/runtime bootstrap, authoritative
  session ownership, and authority-side conversion to protocol snapshots/events.
- `stellar_server_runtime` — reusable networked server host/session lifecycle for dedicated and
  listen-server modes.
- `stellar_dedicated_server` — thin dedicated-server wrapper/CLI helpers around server runtime.
- `stellar_single_player` — client-process local authoritative session for no-server single-player.
- `stellar_client_net` — remote/listen client runtime, socket client connection state, command
  sequencing, and `ClientWorldReceiver`.
- `stellar_client_presentation` — presentation-only player/gameplay/HUD/audio/render-view helpers.
- `stellar_client_app` — top-level client mode selection, platform/window/input, and presentation
  frame loop composition.
- `stellar_listen_server` — client-hosted server mode with lifetime tied to the host client.
- `stellar-client` and `stellar-server` — executable entry points over the split libraries.
- Compatibility shims may remain where needed during include/namespace transition, but new code should
  target the split modules directly.

Boundary enforcement:

- CMake direct-link assertions reject forbidden links such as protocol-to-server/scripting/client,
  transport-to-server/scripting/presentation, remote-client-to-authority/server/scripting,
  dedicated-server-to-client-runtime/presentation, and server-runtime-to-client modules.
- `tools/dev/check_target_boundaries.sh` audits the build graph/source tree for the same ownership
  rules and is part of final CS-8/CS-9 validation.
- Display-free unit/regression tests remain the default. Vulkan and Metal context tests remain
  opt-in.

### 11.4 Validation Commands

Default build and tests:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
ctest --test-dir build --output-on-failure
```

Linux Vulkan-only build and tests:

```bash
cmake --preset linux-vulkan-only
cmake --build --preset linux-vulkan-only --parallel $(nproc)
ctest --preset linux-vulkan-only --output-on-failure
```

BSP-focused validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_|runtime_world|server_world_session|scripted_world_session)' --output-on-failure
```

Lua scripting and scripted playable-world validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build \
  -R '^(bsp_scripted_playable_world_smoke|scripted_world_session|trigger_script|lua_runtime)$' \
  --output-on-failure
```

Opt-in Vulkan/Metal context validation should remain similarly gated.

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
│   └── Phase6D-WorldMetadataFromPreBspImporter.md
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
│       └── scripting/        # core server-authoritative Lua wrapper APIs
├── src/
│   ├── audio/
│   ├── client/
│   ├── core/
│   ├── ecs/
│   ├── graphics/
│   │   ├── metal/
│   │   └── vulkan/
│   ├── import/
│   │   └── bsp/
│   ├── network/
│   ├── physics/              # planned/Phase 6B direction if not already present
│   ├── platform/
│   ├── server/
│   └── scripting/            # Lua runtime, registry, trigger hooks, scripted session
├── tests/
│   ├── graphics/
│   ├── import/
│   │   └── bsp/
│   ├── physics/              # planned/Phase 6B tests if not already present
│   ├── scripting/
│   ├── integration/
│   └── ...
├── assets/
│   └── shaders/
└── thirdparty/
    └── lua/                  # mandatory vendored Lua for server-authoritative scripting
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

- BSP level collision/entity metadata extraction, because it touches importer, renderer-facing level
  assets, and engine collision data.
- Billboard sprites, because rendering must align with gameplay/entity data contracts.
- World metadata extraction, because importer conventions must feed gameplay/world systems.
- Client-server movement, because gameplay rules, ECS storage, networking, and authority overlap.

### 13.2 Specialists

| Agent | Primary Domain |
| --- | --- |
| `@carmack` | ECS, core systems, networking, server authority, build/dependency plumbing, static collision/query infrastructure |
| `@miyamoto` | Graphics abstraction, Vulkan/Metal rendering, BSP level rendering, sprites, shaders, camera |
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
- Unknown or unsupported BSP versions should fail with the version number in the error.
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

### 15.0 Documentation Model

Generated Doxygen documentation is the public C++ API reference for headers under
`include/stellar/**`. Active Markdown files remain the source for architecture, current
implementation status, authoring workflows, validation runbooks, and implementation handoff notes.

Do not migrate active Markdown docs into C++ comments. Instead, include selected Markdown pages in
the generated Doxygen site and keep API details documented near public declarations.

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
- Runtime graphics context creation.
- SDL display/window presentation.
- External artist assets.
- Shader compiler availability at test runtime.

### 16.2 Unit and Regression Tests

Representative coverage:

- ECS entity lifecycle and component storage.
- Serialization and packet handling.
- Math utilities.
- BSP importer regression tests.
- Asset model validation.
- Level render upload/inspection with recording or mock graphics devices.
- Collision extraction tests.
- Collision query and movement tests.
- Backend selection behavior.

### 16.3 Importer and Asset Tests

Importer tests should cover:

- Legacy BSP and BSP30 version handling.
- Header and lump bounds validation.
- Geometry, face triangulation, texture/material name preservation, and bounds.
- Static collision mesh extraction.
- Entity key/value parsing and metadata mapping.
- Trigger, object-collider, sprite, spawn, and script binding conventions.

### 16.4 Graphics Tests

Default graphics tests should use recording/mock devices to inspect backend-neutral behavior:

- Draw ordering.
- Alpha mask/blend grouping.
- Double-sided state.
- Texture/material binding data.
- Surface material preservation.
- Large level submission.
- Billboard sprite sorting and draw data.

Vulkan/Metal context tests remain opt-in and should be used for smoke coverage, not default CI.

### 16.5 Collision and Movement Tests

BSP collision display-free tests should cover:

- Empty levels.
- Worldspawn collision meshes.
- Named brush/entity collision meshes.
- Duplicate collision mesh names.
- BSP face triangle extraction.
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

### 16.6 Scripting Tests

Scripting tests must remain display-free and deterministic by default. Coverage should include:

- Lua runtime construction, always-on sandbox restrictions, protected load/call behavior, bytecode
  rejection, instruction budget failure, and deterministic output draining.
- Trigger script callbacks for enter/stay/exit events using plain authoritative event data.
- `ScriptedWorldSession` loading, missing-script diagnostics, runtime script errors, latest snapshot
  access without callback replay, and deterministic repeatability.
- Scripted playable-world smoke coverage that imports a generated BSP fixture, validates collision
  and metadata, confirms trigger script binding, and verifies script output timing without display,
  GPU, window, or graphics context requirements.

### 16.7 Performance Testing

Long-term performance benchmarks may cover:

- Entity creation/destruction.
- ECS query/system throughput.
- Collision query throughput.
- Network snapshot size and serialization cost.
- Render scene submission cost.
- Sprite/billboard batch throughput.

Do not block current scripting work on broad benchmark infrastructure unless a plan explicitly scopes
it. Scripting hot paths should remain measurable through explicit runtime budgets and deterministic
regression tests before broader benchmark infrastructure is introduced.

---

## 17. Roadmap and Deferred Work

### 17.1 Recommended Next Options

The socket transport roadmap and client/server split are complete. Recommended next scopes are:

1. Presentation-map workflow for remote clients, explicitly separated from authority map loading and
   gameplay script execution.
2. Client interpolation/presentation smoothing over authoritative snapshots, explicitly scoped as
   presentation only.
3. Client prediction/reconciliation, explicitly scoped against server authority.
4. True multiplayer simulation beyond the current single accepted TCP client and one active
   authoritative player slot.
5. UDP/unreliable transport, transport selection, reconnect/resume, authentication, encryption,
   matchmaking, or public Internet deployment.
6. Map distribution/caching after the presentation-map ownership contract is defined.
7. Sprite atlas packing and sprite sheet animation for richer billboard presentation.
8. Richer HUD rendering, UI, inventory presentation, and VFX over server-approved events.
9. miniaudio-backed playback, local audio asset loading, and spatial audio/listener updates.
10. BSP editor/toolchain polish, including automated remapping from editor-facing FGD fields to dotted
   Stellar BSP keys.

### 17.2 Completed Recent Direction

Recent completed work includes:

- Backend-neutral BSP rendering and display-free render submission coverage.
- `KHR_texture_transform`.
- `KHR_materials_unlit`.
- Larger runtime skin palette cap of 256 joints per draw.
- Explicit deferral of full PBR, model/animation systems, imported cameras, and imported lights.
- Phase 6 playable world foundations: static collision extraction, collision queries/movement,
  billboard sprite data, and world metadata extraction.
- Phase 9 runtime playability seam: metadata validation, authoritative `WorldSession`, local
  loopback, and playable-world smoke coverage.
- Phase 10 Lua scripting seam: `stellar_scripting`, script binding metadata, trigger callbacks,
  `ScriptedWorldSession`, and scripted playable-world smoke coverage.
- Phase 11 scripted collision seam: capsule-aware trigger overlap, runtime collision state overlay,
  filtered collision queries/movement, native script collision commands, kinematic object collider
  registry foundation, and scripted collision smoke coverage.
- BSP canonical migration, BSP authoring/presentation hardening, and BSP gameplay-loop Phases 0-8:
  inch-scale BSP client runtime loading, authoritative player camera rendering, metadata-driven
  gameplay world snapshots, pickup collection, and scripted door/gate collision toggling.
- BSP presentation/networking polish PN-0 through PN-6: live scripted authoritative local runtime for
  script-bound BSP maps, non-authoritative gameplay snapshot presentation, deterministic
  snapshot/delta/event contracts, local in-memory transport bridge, HUD/audio presentation routes, and
  BSP authoring/toolchain documentation polish.
- Socket transport ST-2 through ST-7: local mapped play over transport/receiver contracts,
  deterministic session lifecycle, Linux/POSIX TCP socket transport, headless `stellar-server`,
  `stellar-client --connect HOST:PORT`, final hardening/audit, and archived ST plans.
- Client/server split CS-0 through CS-9: protocol/transport/authority/server/client module split,
  single-player no-server mode, listen-server mode, remote-client boundary isolation, build graph
  enforcement, and final architecture handoff documentation.

### 17.3 Deferred Rendering Work

Deferred unless future requirements justify it:

- Full metallic-roughness PBR.
- Image-based lighting and environment maps.
- Tone mapping and color-management policy.
- Deterministic GPU readback validation.
- Model/skinning/animation systems.
- Imported cameras and lights.
- DirectX/WebAssembly backends and full Metal material-lighting parity.

### 17.4 Deferred Gameplay and World Work

Deferred beyond active Phase 6 slices unless explicitly planned:

- Full rigid body physics engine integration.
- Dynamic collision objects.
- Dynamic rigid body collision objects.
- Character capsule/stair-step controller polish beyond current authoritative movement.
- Full ECS rewrite beyond the current minimal server-owned gameplay entity snapshot model.
- Navigation mesh/pathfinding.
- Editor tooling.
- Client-side scripting or presentation scripting.
- Entity/object scripting beyond trigger hooks.

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
| 2026-04-29 | 0.1.4 | ChatGPT | Align design with `collision-movement` branch status, Phase 6 plans, AGENTS coordination rules, then-current renderer status, importer support, and explicit deferred work |
| 2026-04-30 | 0.1.5 | Kilo | Align design with `lua-scripting` branch status, Phase 10 server-authoritative Lua scripting, `stellar_scripting`, scripted session smoke coverage, dependency/build/test updates, and deferred client/entity scripting work |
| 2026-04-30 | 0.1.6 | Kilo | Align design with Phase 11 scripted collision behavior: runtime collision state, filtered authoritative movement, native script collision commands, object collider registry foundation, and scripted collision smoke coverage |
| 2026-05-01 | 0.2.0 | Kilo | Lock active design direction to BSP maps as the canonical playable level format and begin migration from scene-shaped assets to `LevelAsset` |
| 2026-05-01 | 0.2.1 | Kilo | Adopt inch-scale gameplay defaults for world units, player capsule, movement simulation, and debug camera presentation |
| 2026-05-01 | 0.2.4 | Kilo | Mark BSP presentation/networking polish complete; document local transport bridge, remote deferrals, presentation-only HUD/audio routes, and FGD remapping constraints |
| 2026-05-01 | 0.2.8 | Kilo | Mark socket transport scope complete; document TCP-first transport, `stellar-server`, `stellar-client --connect`, single-client/single-active-player limits, and post-ST deferrals |
| 2026-05-03 | 0.3.0 | Kilo | Mark client/server split complete through CS-9; document explicit runtime modes, split CMake targets, build-boundary checks, and post-split deferred networking/presentation work |
| 2026-05-04 | 0.3.1 | Codex | Mark lightweight BSP normal/specular sidecars complete through SNT-8; document optional `.stellar_material` lookup, tangent-aware rendering, then-current backend parity, and full-PBR deferral |
| 2026-05-04 | 0.3.2 | Codex | Remove Vulkan from active renderer support, keep OpenGL as the current backend, and preserve backend-neutral seams for future DirectX/Metal work |
| 2026-05-05 | 0.3.3 | Codex | Add macOS build hygiene, POSIX socket portability, optional miniaudio playback, and an Apple-gated initial Metal backend |
| 2026-05-06 | 0.3.4 | Codex | Expand Metal material parity, runtime smoke coverage, no-device audio validation, and macOS BSP tooling parity while tracking environment-gated validation |
| 2026-05-07 | 0.3.5 | Codex | Complete the Linux Vulkan/macOS Metal renderer matrix and retire OpenGL from active support |

---

## Appendix A: Glossary

- **Billboard:** A 2D sprite that faces the camera in 3D world space.
- **Server authority:** The server is the source of truth for gameplay state.
- **Presentation client:** Client process responsible for rendering, audio, input capture, and
  presentation of server-owned state.
- **Backend-neutral asset:** CPU-side data that does not depend on Vulkan, Metal, or another
  presentation backend.
- **Static level collision:** Non-dynamic world collision geometry, usually imported from authored
  level data.
- **World metadata:** Authored markers such as spawn points, trigger markers, and sprite placements.
- **Snapshot:** Serialized state sent from server to client.
- **Interpolation:** Presentation-side smoothing between received snapshots.
- **Client prediction:** Optional future local simulation that must reconcile with server authority.
- **Lightweight material rendering:** Rendering of the engine's supported material subset without
  claiming full PBR compliance.
- **Server-authoritative scripting:** Gameplay scripts executed on the authoritative runtime side,
  producing validated events or requests instead of directly mutating presentation or native state.

## Appendix B: Reference Materials

- *Game Programming Patterns* — Robert Nystrom.
- EnTT documentation and ECS design references.
- Vulkan and Metal API documentation.
- miniaudio documentation.
