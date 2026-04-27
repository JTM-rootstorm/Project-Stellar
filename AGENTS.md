# Stellar Engine - Agent Coordination

## Project Overview

Stellar Engine is a modular, cross-platform game framework for creating 2D sprite-based games in 3D space. Implements client-server architecture where game logic runs on server and client handles rendering/audio.

**Tech Stack:**
- C++23 with CMake 3.20+
- OpenGL 4.5+ / Vulkan 1.2+ (runtime-selectable)
- GLM for mathematics
- SDL2 or GLFW for windowing
- Client-server networking model

## Architecture

```
Game Client (Presentation)  ← Rendering, Audio, Input
        ↓
   Network API
        ↓
Game Server (Authority)    ← ECS, Game Logic, Physics, AI
```

## Coding Standards

- **Style**: LLVM or Google C++ Style Guide
- **Indentation**: 4 spaces, no tabs
- **Line Length**: 100 characters max
- **Naming**: CamelCase for types, snake_case for functions/variables
- **Headers**: `#pragma once`
- **Namespace**: `stellar::`
- **Error Handling**: std::expected<T, Error> (C++23), no exceptions
- **Documentation**: Doxygen @brief mandatory for all public APIs

## Team Structure

| Agent | Domain | Files |
|-------|--------|-------|
| @carmack | Engine/ECS, Networking, Server Logic | include/stellar/{core,ecs,network}/, src/{ecs,server}/ |
| @miyamoto | Graphics (OpenGL/Vulkan) | include/stellar/graphics/, src/graphics/ |
| @suzuki | Audio (stub/planning) | include/stellar/audio/ |
| @kojima | Game Logic, Entity Archetypes | Entity archetypes, gameplay |
| @molyneux | Prototypes, Features | Experimental work |
| @director | Project Orchestration, Team Routing | (coordination role) |

## Agent Invocation

Use `@agent-name` to invoke specific agents:

- `@carmack implement ECS world serialization`
- `@miyamoto add sprite batch rendering`
- `@sujiki create audio interface`
- `@kojima design new entity archetype`
- `@molyneux prototype particle system`

## Agent Failure Handling Protocol

### Subagent Responsibilities
- Agents other than @director have 2 attempts to resolve issues they encounter
- If an issue cannot be resolved after 2 attempts:
  1. The subagent must report failure to @director
  2. Report must include:
     - What they think is wrong
     - Where in the code the problem might be located
  3. The subagent then terminates execution
- Subagents should use the question tool to communicate failure to @director when needed

### Director Responsibilities
- Upon receiving failure notice from a subagent:
  1. Mark the task as failed in the task tracking system
  2. Inform the user of:
     - Which subagent reported the failure
     - What the subagent thinks is wrong
     - Where in the code the problem might be
  3. Terminate execution
- Maintain a task list in a file for future reference to pick up where left off
- Control the format and ordering of tasks in the tracking file

## Build Instructions

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)
```

Run tests: `ctest`

## Documentation

- Design.md - Full architecture specification
- Doxygen: `doxygen .doxygen` (generate HTML docs)

## Important Notes

- AGENTS.md is write-protected - do not modify without explicit user approval
- Server is authoritative - client is "dumb terminal"
- Data-oriented design for ECS - components are POD
- Dual renderer parity required (OpenGL and Vulkan must behave identically)
- only @director can create sub-agents. 
- @miyamoto , @carmack , @kojima , @molyneux , and @suzuki can not create agents or delegate work.