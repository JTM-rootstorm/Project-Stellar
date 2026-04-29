# Stellar Engine - Game Framework Development Document

**Project:** Stellar Engine  
**Target Platform:** Linux (Kernel 6.19.8+)  
**Language:** C++23, C99 (miniaudio)  
**Build System:** CMake 3.20+  
**Version:** 0.1.3 (Design Phase)  
**Last Updated:** 2026-04-24

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [System Architecture](#2-system-architecture)
3. [Core Engine Subsystem](#3-core-engine-subsystem)
4. [Graphics Subsystem](#4-graphics-subsystem)
5. [Audio Subsystem](#5-audio-subsystem)
6. [Entity Component System](#6-entity-component-system)
7. [Networking & Client-Server Model](#7-networking--client-server-model)
8. [Build System & Dependencies](#8-build-system--dependencies)
9. [Directory Structure](#9-directory-structure)
10. [Team Responsibilities](#10-team-responsibilities)
11. [Development Guidelines](#11-development-guidelines)
12. [Documentation Standards](#12-documentation-standards)
13. [Testing Strategy](#13-testing-strategy)
14. [Future Considerations](#14-future-considerations)
15. [Change Log](#15-change-log)

---

## 1. Project Overview

Stellar Engine is a modular, cross-platform game framework designed for creating 2D sprite-based games in 3D space. The framework implements a client-server architecture where game logic runs on a server (local or remote) and the client handles rendering and audio.

### Key Design Principles

- **Modularity**: Graphics, audio, and core systems are abstracted behind interfaces to allow easy swapping of implementations
- **Separation of Concerns**: Clear distinction between engine (game logic) and client (presentation)
- **Data-Oriented Design**: ECS architecture for performance and flexibility
- **Platform Native**: Linux-first development with Vulkan/OpenGL support
- **Documentation-Driven**: Doxygen annotations required for all public APIs

### Technical Stack

- **Rendering**: OpenGL 4.5+ and Vulkan 1.2+ (runtime-selectable)
- **Graphics Style**: 3D environment with 2D billboarded sprites
- **Networking**: Local loopback for single-player, extensible for multiplayer
- **Audio**: Stubbed interface (no implementation required initially)
- **Math**: GLM (OpenGL Mathematics) library
- **Windowing/Context**: SDL2 (abstraction layer required)

---

## 2. System Architecture

### High-Level Architecture

```
┌─────────────────┐
│   Game Client   │  ← Handles: Rendering, Audio, Input (forwarded)
│  (Presentation) │
├─────────────────┤
│   Network API   │  ← TCP/UDP sockets, serialization
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   Game Server   │  ← Handles: ECS, Game Logic, Physics, AI
│  (Authority)    │
├─────────────────┤
│   World State   │  ← Entity components, game rules
└─────────────────┘
```

### Communication Flow

1. **Client Initialization**
   - Initialize window and graphics context
   - Connect to server (localhost for single-player)
   - Receive world snapshot
   - Begin render loop

2. **Game Loop (Client-Side)**
   ```
   while (running) {
       Receive world state from server
       Interpolate entity positions
       Process input → send to server
       Render frame (sprites, UI)
   }
   ```

3. **Game Loop (Server-Side)**
   ```
   while (running) {
       Receive input from client(s)
       Update ECS world (tick)
       Run game logic, physics, AI
       Broadcast world state to clients
   }
   ```

### Server Authority Model

- Server is the single source of truth for game state
- Client is a "dumb" terminal that renders what the server says
- Client prediction may be added later for responsiveness
- All game rules validated on server

---

## 3. Core Engine Subsystem

**Owner:** Engine Team  
**Responsibility:** Game logic, ECS, world management, serialization

### 3.1 Core Components

#### Application (Client Entry Point)
- Initialize subsystems (graphics, audio, network)
- Create and manage main window
- Run main loop
- Handle shutdown

#### Server (Server Entry Point)
- Initialize ECS world
- Start network listener
- Run simulation loop
- Handle client connections/disconnections

### 3.2 ECS Framework

**Design Pattern:** Entity Component System (data-oriented)

#### Core Types

```cpp
// Entity: unique identifier (uint32_t)
using Entity = uint32_t;

// Component: plain data struct (no methods)
struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };
struct Sprite { uint64_t texture_id; float scale; };

// System: processes entities with specific components
class MovementSystem : public System {
    void Update(World& world, float dt) override;
};
```

#### World Manager
- Entity registry (Sparse Set or Archetype-based)
- Component storage (contiguous arrays per component type)
- System scheduler (parallel execution where possible)
- Queries (filter entities by component mask)

#### Serialization
- Component data serialization (binary format)
- Delta compression for network updates
- Snapshot creation for new connections

### 3.3 Game Logic

#### Entity Archetypes
- `Player` (Position, Velocity, Sprite, Input, Health)
- `NPC` (Position, Velocity, Sprite, AI, Health)
- `Item` (Position, Sprite, Collectible)
- `Projectile` (Position, Velocity, Sprite, Damage)
- `Environment` (Static Position, Sprite, Collider)

#### Systems (Initial Set)
- `MovementSystem` → updates Position from Velocity
- `CollisionSystem` → broadphase + narrowphase collision detection
- `SpriteRenderSystem` → prepares sprite batches for client
- `AISystem` → updates NPC behavior
- `PhysicsSystem` → gravity, forces, integration
- `InputSystem` → processes client input, applies to player
- `HealthSystem` → tracks damage, death, respawn

### 3.4 Configuration Management

- `Config` singleton or service locator
- Load from JSON/YAML files
- Runtime-adjustable (with save/load)
- Categories: graphics, gameplay, network, audio

### 3.5 Time & Timing

- Fixed timestep for simulation (e.g., 60Hz physics)
- Variable rendering (as fast as possible)
- Interpolation buffer for smooth client-side motion

---

## 4. Graphics Subsystem

**Owner:** Graphics Team  
**Responsibility:** Rendering, resource management, shader compilation

### 4.1 Abstraction Layer

```cpp
// Graphics API abstraction (interface)
class GraphicsDevice {
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void DrawSprite(const Sprite&, const Transform&) = 0;
    virtual Texture2D LoadTexture(const std::string& path) = 0;
    // ...
};

// Implementations
class OpenGLDevice : public GraphicsDevice { /* ... */ };
class VulkanDevice : public GraphicsDevice { /* ... */ };
```

### 4.2 Renderer Architecture

#### Render Pipeline (Sprite-Based)

1. **Culling**: Frustum culling, distance culling
2. **Batching**: Group sprites by texture atlas/layer
3. **Sorting**: Depth/priority sorting for transparency
4. **Submission**: Batch draw calls per texture
5. **Present**: Swap buffers

#### Render Targets
- Main game view (3D camera)
- UI overlay (2D orthographic)
- Post-processing (optional, later)

### 4.3 Resource Management

#### Texture Atlas
- Pack all sprite textures into atlases
- Generate UV coordinates
- Mipmap generation

#### Sprite Sheet System
- Animation frames from texture atlases
- Sheet metadata (JSON)

#### Shaders
- **Vertex Shader**: Billboard calculation, transform to world space
- **Fragment Shader**: Texturing, alpha testing, basic lighting
- **Uniforms**: Model-view-projection matrices, lighting params

### 4.4 Camera System

- 3D perspective camera
- Follow player (smooth tracking)
- Bounds clamping
- View/projection matrix generation

### 4.5 Sprite Rendering

- Billboarding: Sprites always face camera
- Depth sorting for transparency
- Sprite scale/rotation in 3D space
- Per-sprite tint/color modulation

### 4.6 OpenGL vs Vulkan

- Both must be implemented behind the common `GraphicsDevice` interface
- Runtime selection is available via configuration or command-line flag
- OpenGL is currently the render-capable path for static and skinned glTF scene rendering
- Vulkan is runtime-selectable and initializes/stores backend-neutral upload and draw metadata,
  but swapchain drawing and presentation are pending
- Shared resource formats remain the goal, with shaders compiled to SPIR-V for Vulkan and GLSL
  for OpenGL where applicable
- Feature parity remains required; the current Vulkan path is an implementation-in-progress toward
  that goal rather than a complete render-equivalent backend

---

## 5. Audio Subsystem

**Owner:** Sound Team  
**Responsibility:** Audio playback, mixing, future implementation

### 5.1 Implementation Status

Full audio implementation required. Library selection: **miniaudio** (see rationale in Appendix C).

### 5.2 Audio Interface

```cpp
// Use miniaudio for 3D spatial audio
// Header: #include "miniaudio.h" (single-file library)
// Link: -ldl (Linux), no external dependencies on other platforms

// Audio device wrapper around miniaudio engine
class AudioDevice {
public:
    // Initialize with default config, optional 3D spatialization
    virtual void Initialize(bool enable_spatialization = true) = 0;
    virtual void Shutdown() = 0;

    // Sound management (supports 3D positioning)
    virtual ma_sound LoadSound(const std::string& path) = 0;
    virtual void PlaySound(ma_sound* sound, float volume = 1.0f,
                          const glm::vec3* position = nullptr) = 0;
    virtual void PlayMusic(const std::string& path, bool loop = true) = 0;

    // Listener for 3D audio (attach to player camera)
    virtual void UpdateListener(const glm::vec3& position,
                               const glm::vec3& forward,
                               const glm::vec3& up) = 0;

    // Volume control
    virtual void SetMasterVolume(float volume) = 0;
};
```

### 5.3 Implementation Notes

- **Library**: miniaudio (MIT, header-only)
- **3D Features**: Doppler effect, inverse/linear/exponential attenuation, multi-listener support
- **Backends**: Auto-detected (ALSA/PulseAudio on Linux, WASAPI on Windows, Core Audio on macOS)
- **Decoding**: Built-in support for WAV, FLAC, MP3
- **Spatialization**: Enabled by default on sounds; disable with MA_SOUND_FLAG_NO_SPATIALIZATION
- **Performance**: Resource management for streaming music vs one-shot effects

---

## 6. Entity Component System

**Owner:** Engine Team  
**Responsibility:** Core data structures and system execution

### 6.1 Design Philosophy

- Data-oriented: Components are POD (plain old data)
- Cache-friendly: Contiguous memory for component arrays
- Zero virtual calls in hot loops
- Parallel system execution where dependencies allow

### 6.2 Component Model

#### Component Registry
- Each component type has unique ID
- Type-indexed storage (sparse set or archetype)
- Component bitmask for entity queries

#### Archetype-Based Storage (Recommended)
- Entities grouped by component composition
- Each archetype stores contiguous arrays per component
- Fast iteration over homogeneous entity sets

#### Sparse Set Storage (Alternative)
- Dense array of entities + sparse array of indices
- Simpler, but less cache-coherent for multi-component queries

### 6.3 World Class

```cpp
class World {
public:
    Entity CreateEntity();
    void DestroyEntity(Entity e);
    void RegisterComponentType<C>();
    template<typename C> void AddComponent(Entity, C&&);
    template<typename C> C* GetComponent(Entity);
    template<typename... C> View<C...> Query();
    void UpdateSystems(float dt);
};
```

### 6.4 Systems

```cpp
class System {
public:
    virtual ~System() = default;
    virtual void Update(World& world, float dt) = 0;
    virtual const char* GetName() const = 0;
};

// System dependencies (component requirements)
class MovementSystem : public System {
    ComponentMask GetRequiredComponents() const override {
        return { typeid(Position), typeid(Velocity) };
    }
    void Update(World& world, float dt) override;
};
```

### 6.5 Events

- Event bus for decoupled communication
- Examples: `EntityDestroyed`, `Collision`, `PlayerKilled`
- Systems subscribe to events of interest

---

## 7. Networking & Client-Server Model

**Owner:** Engine Team (with Graphics integration)  
**Responsibility:** Network communication, state synchronization

### 7.1 Transport Layer

#### Protocol Choice
- **UDP** recommended for game state updates (lower latency, unordered)
- **TCP** for reliable messages (connection setup, chat, etc.)
- Header format: magic number, sequence ID, payload size, checksum

#### Connection Setup
1. Client sends `CONNECT` packet
2. Server responds with `WELCOME` + initial world snapshot
3. Heartbeat mechanism (ping/pong) for liveness

### 7.2 Message Types

| Type | Direction | Reliability | Description |
|------|-----------|-------------|-------------|
| `INPUT` | Client→Server | Unreliable, ordered | Input state (keys, mouse) |
| `WORLD_STATE` | Server→Client | Unreliable, unordered | Full/partial world snapshot |
| `ENTITY_SPAWN` | Server→Client | Reliable | New entity creation |
| `ENTITY_DESTROY` | Server→Client | Reliable | Entity removal |
| `PLAYER_JOIN` | Server→Client | Reliable | New player joined |
| `PLAYER_LEAVE` | Server→Client | Reliable | Player disconnected |

### 7.3 State Synchronization

#### Snapshot Interpolation
- Server sends world snapshots at fixed rate (e.g., 20Hz)
- Client maintains buffer of past snapshots (e.g., 200ms)
- Interpolate between snapshots for smooth rendering

#### Entity Authority
- Server owns all entity state
- Client only has read-only ghost copies
- Input from client applied by server to authoritative entities

#### Delta Compression
- Send only changed components
- Baseline reference from previous ack'd snapshot

### 7.4 Serialization

```cpp
class Serializer {
    void WriteEntity(Entity e, const Position& p);
    void WriteEntity(Entity e, const Velocity& v);
    // One method per component type

    void ReadEntity(Entity e, Position& p);
    // ...
};
```

- Component-by-component serialization
- Compress with run-length or bit-packing (optional)
- Endianness handling (Linux is little-endian, assume homogeneous)

### 7.5 Client Prediction (Future)

- Predict local player movement immediately
- Reconcile with server state (rewind/replay)
- Entity reconciliation for other entities

---

## 8. Build System & Dependencies

**Owner:** Engine Team  
**Responsibility:** Build configuration, dependency management

### 8.1 CMake Requirements

- Minimum CMake: 3.20
- C++ Standard: C++23
- Target architecture: x86_64 Linux

### 8.2 External Dependencies

| Library | Purpose | Version | License |
|---------|---------|---------|---------|
| GLM | Mathematics (vectors, matrices) | 0.9.9+ | MIT |
| SDL2 | Window creation, input handling | 2.28+ | zlib |
| miniaudio | 3D spatial audio, sound playback | 0.11+ | MIT |
| Vulkan SDK | Vulkan loader & headers | 1.2+ | Apache 2.0 |
| GLEW or glad | OpenGL extension loading | 2.2+ / 1.20+ | MIT |
| Enet or RakNet | Optional: UDP networking library | - | BSD/MIT |
| rapidjson or cereal | JSON/config + serialization | 1.1+ / 1.3+ | MIT |
| catch2 or doctest | Unit testing | 3.0+ / 2.0+ | Boost/MIT |
| Doxygen | Documentation generation | 1.9+ | GPL |

### 8.3 Project Structure (CMake)

```
CMakeLists.txt (root)
├── cmake/           (modules, FindXXX.cmake)
├── src/
│   ├── client/
│   ├── server/
│   ├── common/
│   └── main.cpp     (entry points)
├── include/
│   ├── stellar/
│   │   ├── core/
│   │   ├── ecs/
│   │   ├── graphics/
│   │   ├── audio/
│   │   ├── network/
│   │   └── platform/
├── tests/
├── thirdparty/      (vendored deps, if any)
└── docs/
```

### 8.4 Build Targets

- `stellar-client` → client executable
- `stellar-server` → server executable (headless)
- `stellar-shared` → common library (ECS, math, config)
- `tests` → unit test runner

### 8.5 Configuration Types

- `Debug` → asserts, debug symbols, no optimization
- `Release` → full optimization, no debug info
- `RelWithDebInfo` → optimized + debug symbols

### 8.6 Dependency Management

- Prefer system packages (apt install) for development
- CMake `find_package()` for all dependencies
- FetchContent/ExternalProject for vendoring (if needed)

---

## 9. Directory Structure

```
stellar-engine/
├── .kilo/                      # Kilo CLI configuration
├── cmake/                      # CMake modules
│   ├── FindXXX.cmake
│   └── CompilerFlags.cmake
├── docs/                       # Doxygen output, design docs
│   ├── Design.md               # This document
│   ├── API.md                  # Generated API reference
│   └── Architecture.md         # Architecture diagrams
├── include/                    # Public headers
│   └── stellar/
│       ├── common/             # Shared types, utilities
│       │   ├── Types.hpp
│       │   ├── Config.hpp
│       │   ├── Time.hpp
│       │   └── Log.hpp
│       ├── core/               # Engine core (ECS, world)
│       │   ├── World.hpp
│       │   ├── Entity.hpp
│       │   ├── Component.hpp
│       │   ├── System.hpp
│       │   └── archetypes/
│       ├── ecs/                # ECS implementation
│       │   ├── Registry.hpp
│       │   ├── View.hpp
│       │   ├── SparseSet.hpp
│       │   └── Archetype.hpp
│       ├── graphics/           # Graphics abstraction
│       │   ├── GraphicsDevice.hpp
│       │   ├── Renderer.hpp
│       │   ├── Sprite.hpp
│       │   ├── Texture.hpp
│       │   ├── Camera.hpp
│       │   ├── Shader.hpp
│       │   └── opengl/         # OpenGL implementation
│       │   └── vulkan/         # Vulkan implementation
│       ├── audio/              # Audio abstraction (stub)
│       │   ├── AudioDevice.hpp
│       │   ├── Sound.hpp
│       │   └── Music.hpp
│       ├── network/            # Networking
│       │   ├── Socket.hpp
│       │   ├── Packet.hpp
│       │   ├── Client.hpp
│       │   └── Server.hpp
│       └── platform/           # Platform abstraction
│           ├── Window.hpp
│           ├── Input.hpp
│           └── Linux/           # Linux-specific
├── src/                        # Implementation
│   ├── common/
│   ├── core/
│   ├── ecs/
│   ├── graphics/
│   ├── audio/
│   ├── network/
│   ├── platform/
│   ├── client/                # Client executable
│   │   ├── main.cpp
│   │   ├── Client.hpp
│   │   └── Client.cpp
│   └── server/                # Server executable
│       ├── main.cpp
│       ├── Server.hpp
│       └── Server.cpp
├── tests/                      # Unit tests
│   ├── ecs/
│   ├── network/
│   ├── graphics/
│   └── core/
├── assets/                     # Game assets (textures, configs)
│   ├── textures/
│   ├── atlases/
│   ├── configs/
│   └── shaders/
├── thirdparty/                 # Vendored dependencies (if needed)
├── build/                      # Build directory (gitignored)
├── CMakeLists.txt
├── stellar-config.cmake.in     # Config file for find_package
├── README.md
├── LICENSE
└── AGENTS.md                   # Agent task definitions
```

---

## 10. Team Responsibilities

### 10.1 Engine Team

**Lead:** carmack

**Areas of Responsibility:**
- ECS framework design and implementation
- World management and entity lifecycle
- Game logic systems (movement, collision, AI)
- Serialization and networking protocol
- Server authority and state synchronization
- Configuration and time management
- Core utilities (math, logging, assertions)
- Platform integration (window creation, input handling, file system)

**Key Files:**
- `include/stellar/core/*`
- `include/stellar/ecs/*`
- `include/stellar/network/*`
- `include/stellar/platform/*`
- `src/ecs/`
- `src/server/`
- `src/common/`
- `src/platform/`

**Deliverables:**
- Working ECS with ≥60 FPS for 10k entities on mid-range hardware
- Stable client-server protocol with <100ms round-trip latency
- Serializable world snapshots
- Config files for game parameters
- Platform abstraction layer for window/input

### 10.2 Graphics Team

**Lead:** miyamoto

**Areas of Responsibility:**
- Graphics device abstraction (OpenGL + Vulkan)
- Sprite rendering pipeline
- Texture atlas management
- Shader compilation and management
- Camera and viewport handling
- Render batching and sorting
- Window/context creation integration

**Key Files:**
- `include/stellar/graphics/*`
- `src/graphics/`
- `src/graphics/opengl/`
- `src/graphics/vulkan/`
- `assets/shaders/`

**Deliverables:**
- Both OpenGL and Vulkan renderers with identical behavior
- Batch sprite rendering: ≥10,000 sprites at 60 FPS on integrated GPU
- Smooth camera following with no stutter
- Texture atlas packer tool

### 10.3 Sound Team

**Lead:** suzuki

**Areas of Responsibility:**
- Audio device interface definition
- Future audio implementation planning
- Integration points with engine (sound triggers, positional audio)

**Key Files:**
- `include/stellar/audio/*`
- `src/audio/` (stubs only)

**Deliverables:**
- Audio device implementation using miniaudio
- 3D positional audio integration with camera system
- Sound loading and playback system
- Music streaming with loop support
- Listener management tied to player position

### 10.4 Game Logic Team

**Lead:** kojima

**Areas of Responsibility:**
- Entity archetype definitions and component compositions
- Gameplay system design (movement, collision, health, inventory)
- System balance and tuning
- Game rules enforcement on server
- Emergent gameplay patterns
- Content design guidelines

**Key Files:**
- `include/stellar/core/archetypes/`
- `src/ecs/systems/gameplay/`
- `src/server/game_logic/`

**Deliverables:**
- Complete set of entity archetypes (Player, NPC, Item, Projectile, Environment)
- Core gameplay systems (Movement, Collision, Health, AI)
- Game rules validation logic
- Balance parameters and tuning guide

### 10.5 Prototyping Team

**Lead:** molyneux

**Areas of Responsibility:**
- Rapid proof-of-concept implementation
- Experimental feature exploration
- New system prototyping
- Evaluation of unproven approaches
- Documentation of findings
- Integration recommendations

**Key Files:**
- `prototypes/` directory for experimental code
- `docs/prototypes/` for analysis documents

**Deliverables:**
- Working prototypes of new features
- Performance evaluations and analysis
- Integration feasibility studies
- Documentation of experimental results

---

*Note: Platform responsibilities (window management, input handling, file system) are incorporated into the Engine Team under @carmack's leadership, as they are core infrastructure requirements.*

## 11. Development Guidelines

### 11.1 Coding Standards

- **Style**: Follow LLVM or Google C++ Style Guide (choose one, document)
- **Indentation**: 4 spaces, no tabs
- **Line Length**: 100 characters max
- **Naming**:
  - `CamelCase` for types (classes, structs)
  - `snake_case` for functions, variables, files
  - `kPrefix` for constants (or `CONSTANT_CASE` - decide)
  - `g_` prefix for globals (avoid globals when possible)
- **Headers**: `#pragma once` for include guards
- **Namespaces**: `stellar::` for all library code

### 11.2 API Design Rules

1. **Pure virtual interfaces** for swappable subsystems
2. **PIMPL idiom** for implementation hiding in public headers
3. **No exceptions** (use `std::expected` or error codes)
4. **RAII** for all resource management
5. **Move semantics** for performance (large objects)
6. **`const` correctness** everywhere possible

### 11.3 Interface Contracts

Each module must define a clear interface header:

```cpp
// GraphicsDevice.hpp (interface)
namespace stellar::graphics {
    class GraphicsDevice {
    public:
        virtual ~GraphicsDevice() = default;
        virtual void BeginFrame() = 0;
        // ...
    };
    std::unique_ptr<GraphicsDevice> CreateOpenGLDevice();
    std::unique_ptr<GraphicsDevice> CreateVulkanDevice();
}
```

Implementation in separate `.cpp` files, not exposed in public headers.

### 11.4 Error Handling

- Prefer return codes or `std::expected<T, Error>` (C++23)
- Log errors with `std::cerr` or logging service (to be implemented)
- Crashing on unrecoverable errors is acceptable (with message)
- Never use `assert()` for production checks; use `ENSURE()` macro

### 11.5 Threading Model

- **Main thread**: Rendering, window messages
- **Worker threads**: ECS systems (parallelizable), network I/O
- Use thread pools, not raw `std::thread` per task
- Mutexes only around shared resource access (World modifications)
- Consider job system for system parallelization

---

## 12. Documentation Standards

### 12.1 Doxygen Configuration

**File:** `.doxygen` at project root

**Key Settings:**
- `PROJECT_NAME = "Stellar Engine"`
- `OUTPUT_DIRECTORY = docs/doxygen`
- `EXTRACT_ALL = YES` (document everything)
- `WARN_IF_UNDOCUMENTED = YES`
- `GENERATE_HTML = YES`
- `GENERATE_LATEX = NO`

### 12.2 Documentation Requirements

Every public-facing element must have Doxygen comment:

```cpp
/**
 * @brief Moves the entity based on velocity.
 * @param world The world containing entities.
 * @param dt Time step in seconds.
 *
 * This system iterates over all entities with Position and Velocity
 * components and updates their position using semi-implicit Euler.
 */
void MovementSystem::Update(World& world, float dt);
```

**Required tags:**
- `@brief` (one-line summary) — mandatory
- `@param` for each parameter
- `@return` for non-void functions
- `@see` for related functions/classes
- `@note` for important implementation notes

### 12.3 Architecture Documentation

- UML diagrams for class relationships (use PlantUML or Mermaid)
- Sequence diagrams for network protocol
- Component diagrams for ECS archetypes
- Rendering pipeline flowchart

Store in `docs/architecture/`.

### 12.4 In-Code Examples

Include usage examples in documentation where helpful:

```cpp
/**
 * @example create_entity.cpp
 * Demonstrates entity creation and component addition.
 */
```

Place examples in `docs/examples/` or inline in header documentation.

---

## 13. Testing Strategy

### 13.1 Unit Testing

**Framework:** Catch2 or doctest (choose one)  
**Location:** `tests/` mirroring source structure

**Coverage Goals:**
- ECS: ≥90% (entity lifecycle, component storage, queries)
- Networking: ≥80% (serialization, packet handling)
- Math utilities: ≥95%
- Graphics abstraction: ≥70% (interface contracts)

**Test Files:**
- `tests/ecs/test_world.cpp`
- `tests/ecs/test_archetype.cpp`
- `tests/network/test_serialization.cpp`
- `tests/network/test_packet.cpp`
- `tests/core/test_config.cpp`

### 13.2 Integration Testing

- Client-server handshake test
- World snapshot round-trip
- Input → state update pipeline
- Renderer initialization with both OpenGL and Vulkan

### 13.3 Performance Testing

**Benchmarks:**
- Entity creation/destruction rate
- System update throughput (entities/sec)
- Network bandwidth (bytes/sec for N entities)
- Sprite batch render rate (sprites/frame)

**Tools:** Custom benchmark harness or Google Benchmark

### 13.4 CI/CD (Future)

- GitHub Actions or GitLab CI
- Build on Ubuntu LTS (22.04 or 24.04)
- Run unit tests on each PR
- Generate Doxygen site on main branch

---

## 14. Future Considerations

### 14.1 Phase 2 Features

- **Physics Engine**: Integrate Bullet or Box2D (2D physics in 3D space)
- **Asset Pipeline**: Tools for texture packing, atlas generation, shader compilation
- **Editor**: Simple entity/level editor (ImGui-based)
- **Scripting**: Lua or AngelScript for game logic
- **Multiplayer**: Dedicated server, multiple clients, lobby system
- **Audio Implementation**: Full 3D positional audio
- **Post-Processing**: Bloom, tone mapping, anti-aliasing
- **Model Support**: 3D models as billboards or with skeletal animation

### 14.2 Platform Expansion

- Windows port (DirectX 12 backend)
- macOS port (Metal backend)
- WebAssembly (Emscripten, OpenGL ES)

### 14.3 Optimization Targets

- SIMD for component processing (Eigen or custom)
- Job system with work stealing
- Memory arenas for entity allocations
- GPU-based culling (compute shaders)

---

## 15. Change Log

| Date | Version | Author | Changes |
|------|---------|--------|---------|
| 2026-04-20 | 0.1.0 | Initial draft | Initial document creation |
| 2026-04-23 | 0.1.1 | TBD | Replace SDL2 with SFML for windowing and input |
| 2026-04-23 | 0.1.2 | TBD | Add miniaudio for 3D spatial audio, update build to support C and C++ |
| 2026-04-24 | 0.1.3 | TBD | Replace SFML with SDL2 for windowing and input |

---

## Appendix A: Glossary

- **Billboard**: A 2D sprite that always faces the camera
- **Archetype**: Group of entities sharing the same component composition
- **Snapshot**: Full or partial copy of world state for network transmission
- **Interpolation**: Estimating entity state between received snapshots
- **Client prediction**: Applying input locally before server confirmation
- **Server authority**: Server is the source of truth for game state
- **Fixed timestep**: Simulating at constant rate independent of frame rate

## Appendix B: Reference Materials

- **Game Programming Patterns** — Robert Nystrom
- **ECS in C++** — EnTT library documentation
- **Vulkan Tutorial** — vulkan-tutorial.com
- **OpenGL 4.5 Specification** — Khronos Group
- **GDC Vault** — Networking, optimization talks

## Appendix C: Audio Library Selection

### Criteria
- 3D spatialization with doppler and attenuation
- Cross-platform (Linux, future Windows/macOS)
- No complex external dependencies
- Permissive license (MIT/BSD)
- Active maintenance

### Evaluation

| Library | 3D Audio | License | Dependencies | Complexity |
|---------|-----------|---------|--------------|------------|
| miniaudio | ✓ Doppler, attenuation | MIT | None (single header) | Low |
| OpenAL | ✓ Basic | LGPL | External | Medium |
| SFML Audio | ✓ Basic positioning | zlib | SFML (already using) | Low |
| FMOD | ✓✓ Full | Proprietary | Large SDK | High |

### Decision: miniaudio

Rationale:
1. First-class 3D audio with doppler effect (SFML lacks this)
2. Single-file library, no build system integration needed
3. MIT license, no commercial restrictions
4. Uses same backends as SFML audio on Linux (ALSA/PulseAudio)
5. Multiple listener support for future multiplayer
6. Active development with growing ecosystem

---

**Document Status:** Draft — Subject to revision as implementation progresses
