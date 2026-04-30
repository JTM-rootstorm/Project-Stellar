# Stellar Engine - Agent Coordination

## Project Overview

Stellar Engine is a modular, cross-platform game framework for creating 2D sprite-based games in 3D space. It implements a client-server architecture where game logic runs on the server and the client handles rendering, audio, input capture, and presentation.

**Tech Stack:**
- C++23 with CMake 3.20+
- C99 where required for single-file C dependencies such as miniaudio
- OpenGL 4.5+ / Vulkan 1.2+ (runtime-selectable)
- GLM for mathematics
- SDL2 for windowing/input abstraction
- miniaudio for audio playback and 3D spatial audio
- Client-server networking model

## Documentation Source of Truth

Agents must check the relevant project documents before making architecture or implementation assumptions.

Precedence for the `collision-movement` branch:

1. `docs/ImplementationStatus.md` - current branch-facing implementation status and near-term priorities.
2. Active implementation plans under `Plans/` - current handoff plans when referenced by `ImplementationStatus.md`.
3. `docs/Design.md` - broad architecture and long-term design intent.
4. `AGENTS.md` and `.kilo/agents/*.md` - coordination rules, ownership, and agent behavior.

When `docs/ImplementationStatus.md` conflicts with `docs/Design.md`, prefer `docs/ImplementationStatus.md` for current branch status.

## Current Branch Focus

Current branch: `collision-movement`.

Primary near-term goal: move from "glTF renders as a scene" toward "glTF can define playable static world geometry" for a 3D world with 2D billboard entities.

Recommended implementation order:

1. `Plans/Phase6A-LevelCollisionExtraction.md`
   - Backend-neutral static level collision data
   - glTF collision extraction from selected nodes/meshes
   - Collision node conventions such as `COL_*`, `Collision_*`, and parent `Collision`
   - Display-free importer tests
2. `Plans/Phase6B-CollisionQueriesAndMovement.md`
   - Collision queries
   - Ground probing
   - Minimal sweep/slide movement over static collision
3. `Plans/Phase6C-BillboardSpriteRendering.md`
   - 2D billboard sprite rendering in 3D world space
   - OpenGL/Vulkan parity
4. `Plans/Phase6D-WorldMetadataFromGltf.md`
   - glTF node-convention metadata for spawns, triggers, sprite markers, and similar gameplay markers

Avoid spending the next implementation slices on full PBR, morph targets, glTF cameras, or glTF lights unless the user explicitly asks or a concrete asset requirement appears.

## Architecture

```
Game Client (Presentation)  ← Rendering, Audio, Input Capture
        ↓
   Network API
        ↓
Game Server (Authority)     ← ECS, Game Logic, Physics, AI, Validation
```

Key rules:

- Server is authoritative.
- Client is a presentation terminal unless client prediction is explicitly scoped.
- ECS/gameplay state must remain serializable and server-owned.
- Rendering and audio must not become sources of gameplay truth.
- OpenGL and Vulkan are runtime-selectable through the shared graphics abstraction.
- Current renderer goal is lightweight material parity, not full glTF PBR compliance.

## Coding Standards

- **Style**: LLVM or Google C++ Style Guide
- **Indentation**: 4 spaces, no tabs
- **Line Length**: 100 characters max
- **Naming**: CamelCase for types, snake_case for functions/variables
- **Headers**: `#pragma once`
- **Namespace**: `stellar::`
- **Error Handling**: std::expected<T, Error> (C++23), no exceptions
- **Documentation**: Doxygen @brief mandatory for all public APIs

Do not change these standards without explicit user approval. The project has been built around them from the ground up.

## Team Structure

| Agent | Role | Domain | Files |
|-------|------|--------|-------|
| @director | Technical Lead | Technical direction, routing, design compliance, conflict resolution, integration review | Cross-cutting decisions |
| @carmack | Engine Subsystem Specialist | ECS, core systems, networking, server authority, build/dependency plumbing | include/stellar/{core,ecs,network,platform}/, src/{core,ecs,network,server,common,platform}/ |
| @miyamoto | Graphics Subsystem Specialist | Graphics abstraction, OpenGL/Vulkan, glTF rendering, sprites, shaders, camera | include/stellar/graphics/, src/graphics/, assets/shaders/ |
| @suzuki | Audio Subsystem Specialist | miniaudio, audio interfaces, playback, spatial audio, no-op fallback paths | include/stellar/audio/, src/audio/ |
| @kojima | Gameplay Systems Specialist | Entity archetypes, movement rules, collision responses, gameplay mechanics, tuning | include/stellar/core/archetypes/, src/ecs/systems/, src/server/game_logic/ |
| @molyneux | Prototype Specialist | Isolated experiments, feasibility studies, proof-of-concepts | prototypes/, docs/prototypes/ |

## Agent Invocation

Use `@agent-name` to invoke specific agents:

- `@director plan Phase 6A and assign the next implementation slice`
- `@carmack implement ECS world serialization`
- `@miyamoto add sprite batch rendering`
- `@suzuki implement miniaudio-backed sound playback`
- `@kojima design a player movement and collision response system`
- `@molyneux prototype a particle system`

Correct spelling matters: use `@suzuki`, not `@sujiki`.

## Routing Rules

### Director Routing Authority

`@director` is the only agent allowed to route or delegate work, and only to the existing named agents listed in this file.

`@director` may:
- Break broad user requests into scoped tasks
- Assign work to existing specialists
- Sequence cross-subsystem work
- Resolve team conflicts
- Review completed work for design compliance
- Escalate product-intent questions to the user

`@director` may not:
- Create new agents or subagents
- Clone existing agents
- Invent temporary helper personas
- Route work to agents not listed in this file

### Specialist Boundaries

Specialist agents (`@carmack`, `@miyamoto`, `@suzuki`, `@kojima`, `@molyneux`) are not allowed to route or delegate work.

Specialist agents must not:
- Delegate tasks to other agents
- Invoke another agent
- Spawn, create, clone, or simulate subagents
- Create new agent files or helper prompts
- Ask another specialist to continue their work
- Reassign work outside their domain

When a specialist encounters cross-domain work, they must report the need to `@director` and stop at their boundary.

## Routing Map

| Request Domain | Primary Agent | Escalate To | Notes |
|----------------|---------------|-------------|-------|
| ECS, registry, world, component storage | @carmack | @director | Keep data-oriented and server-authoritative |
| Networking, serialization, server loop | @carmack | @director | Validate all client input server-side |
| Build system and shared infrastructure | @carmack | @director | Keep CMake changes focused |
| OpenGL/Vulkan rendering | @miyamoto | @director | Preserve shared graphics abstraction |
| glTF render import/materials/skin rendering | @miyamoto | @director | Current goal is lightweight parity, not full PBR |
| Static collision data and collision queries | @carmack | @director | Coordinate with graphics/import data when glTF extraction is involved |
| glTF collision extraction from scene nodes | @director coordinates @miyamoto + @carmack | @director | Cross-cutting Phase 6A work |
| Billboard sprites in 3D world space | @miyamoto | @director | Coordinate with gameplay/entity data through @director |
| World metadata for spawns/triggers/game markers | @kojima | @director | Coordinate glTF extraction and ECS integration through @director |
| Audio interface, miniaudio, playback, listener updates | @suzuki | @director | Keep gameplay/audio trigger integration explicit |
| Game logic, archetypes, mechanics, tuning | @kojima | @director | Server is authoritative |
| Prototypes and feasibility experiments | @molyneux | @director | Keep prototypes isolated unless integration is approved |
| Cross-subsystem architecture or unclear ownership | @director | User if needed | Director decides, scopes, or asks user |

## Agent Failure Handling Protocol

### Specialist Responsibilities

Agents other than `@director` have 2 serious attempts to resolve issues they encounter.

A serious attempt means:
- Inspecting the relevant code and documentation
- Making a focused fix or analysis attempt
- Running or identifying the relevant validation step
- Learning something concrete from the failure

If an issue cannot be resolved after 2 serious attempts, the specialist must stop and report failure to `@director`.

The failure report must include:
- Which task failed
- What the agent attempted
- What the agent thinks is wrong
- Where in the code or design the problem likely lives
- Whether the blocker is technical, design-related, dependency-related, or cross-domain
- Recommended next action

The specialist must not delegate the failure to another agent.

### Director Responsibilities

Upon receiving a failure notice from a specialist, `@director` must:

1. Record or track the failed task if task tracking is in use.
2. Decide whether to:
   - Resolve the issue directly
   - Re-scope the task
   - Assign a follow-up to an existing specialist
   - Ask the user for clarification
   - Reject the approach
3. Inform the user when the failure changes the plan, blocks progress, or requires product/design input.
4. Stop the current execution path when the failure invalidates the plan.

## Cross-Subsystem Coordination

Cross-subsystem work should be coordinated by `@director`.

Examples:

- glTF collision extraction requires graphics/import knowledge and engine collision data ownership.
- Billboard sprites require graphics implementation plus gameplay/ECS data contracts.
- Audio events require gameplay trigger points plus audio playback/listener behavior.
- Client-server movement requires gameplay rules, ECS storage, networking, and authoritative validation.

Specialists should describe integration needs precisely but should not route the work themselves.

## Validation Expectations

Agents should validate changes with the narrowest useful checks available.

Default build:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)
```

Run tests:

```bash
ctest
```

Guidance:

- Prefer display-free tests by default.
- OpenGL/Vulkan context tests are opt-in when they require GPU/display access.
- For importer work, add or run fixture-based tests when possible.
- For ECS/network/gameplay work, prefer deterministic unit tests.
- For prototypes, document build/run steps and expected behavior.

## Documentation Rules

- `AGENTS.md` is a coordination document. Do not modify it without explicit user approval.
- `.kilo/agents/*.md` define agent-specific behavior. Keep them consistent with this file.
- `docs/Design.md` describes broad architecture.
- `docs/ImplementationStatus.md` describes current branch implementation status.
- Historical roadmap files under `Plans/` and `.kilo/plans/` may be stale unless `docs/ImplementationStatus.md` identifies them as current.
- Public APIs require Doxygen `@brief` comments.

## Important Notes

- No agent may create new agents or subagents.
- Only `@director` may delegate work, and only to existing named agents.
- Specialist agents must escalate to `@director` instead of delegating.
- Server is authoritative; the client is not trusted for game state.
- Data-oriented ECS remains a core design requirement.
- OpenGL and Vulkan parity remains required through shared abstractions.
- Audio is miniaudio-backed when implementation is in scope, with explicit no-op fallback where needed.
- Keep implementation slices aligned with the active branch priorities.
