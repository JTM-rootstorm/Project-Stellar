# Project Stellar — BSP Gameplay Loop Follow-up Master Plan

Optimized for Kilo Code intake.

## Plan identity

- Repository: `JTM-rootstorm/Project-Stellar`
- Implementation base: current `bsp-gameplay-loop` branch state
- Plan package: BSP gameplay-loop follow-up implementation
- Recommended active scope name: `bsp-presentation-networking-polish`
- Date prepared: 2026-05-01
- Coordinator: `@director`
- Primary implementation agents:
  - `@carmack`: server authority, client runtime seam, snapshot/delta contracts, networking, build/tests
  - `@miyamoto`: graphics presentation, billboard/entity rendering, render inspection tests
  - `@kojima`: gameplay semantics for pickups, interaction feedback, HUD/inventory meaning
  - `@suzuki`: audio event presentation and no-op fallback, if audio polish is accepted
  - `@director`: routing, cross-subsystem integration, docs/archive/final validation

## Current state summary

The completed `bsp-gameplay-loop` branch already has:

- BSP as canonical playable map format.
- Inch-scale world policy: 1 gameplay unit = 1 inch, Y-up, BSP imports 1:1.
- Live client BSP loading, `RuntimeWorld` construction, local loopback movement, and authoritative camera-driven rendering.
- Minimal server-owned `GameplayWorld` snapshots for players, sprite markers, pickup candidates, doors/gates, triggers, and object colliders.
- Server-side pickup collection and scripted gate/door collision toggling validated through scripting tests.
- `RenderLevel` support for static level rendering plus billboard sprites.
- Completed Phase 0-8 documentation and validation.

Important gap this implementation run closes:

- The live client currently constructs a plain `LocalLoopbackRuntime` over `server::WorldSession`.
- `scripting::ScriptedWorldSession` exists and works in tests, but is not yet integrated into the live client runtime preparation path.
- Gameplay entity snapshots exist, but the live renderer is not yet fed sprites/pickups/door state from authoritative snapshots.
- Networking is still local loopback only; snapshot/delta/event contracts need to be made transport-ready before remote transport work.

## Non-negotiable invariants

Do not violate these during any phase:

- Server authority remains mandatory.
- Client-side gameplay scripting is forbidden.
- Lua remains server-authoritative and sandboxed.
- Import never executes scripts.
- Rendering/audio/UI are presentation only and never sources of gameplay truth.
- Default tests remain display-free.
- OpenGL/Vulkan stay runtime-selectable through shared graphics abstractions.
- BSP remains canonical playable level format.
- Do not reintroduce retired pre-BSP importer functionality.
- Do not add Source/VBSP, dynamic rigid bodies, moving brush simulation, full PBR, or broad model/animation systems unless explicitly requested.
- `AGENTS.md` says it should not be modified without explicit user approval. This plan does not require editing it.

## Required reading before edits

Every Kilo run should read these before editing:

1. `AGENTS.md`
2. `.kilo/agents/director.md`
3. `docs/ImplementationStatus.md`
4. `Plans/NEXT.md`
5. `docs/Design.md`
6. `docs/BspAuthoring.md`
7. Current files named in each phase plan

## Phase order and dependencies

```text
Phase 0: Archive old plans + publish active follow-up plan docs
    ↓
Phase 1: Live scripted authoritative runtime integration
    ↓
Phase 2: Authoritative gameplay snapshot presentation
    ↘
     Phase 3: Snapshot/delta/event transport contracts
          ↓
     Phase 4: Local/remote transport bridge and client/server message path
          ↓
Phase 5: HUD/audio/toolchain polish over server-approved events
    ↓
Phase 6: Final documentation, archive hardening, validation, and NEXT.md handoff
```

Parallelization rules:

- Phase 0 must run first and should be doc-only.
- Phase 1 must complete before any live-client interaction presentation is considered complete.
- Phase 2 and Phase 3 can overlap after Phase 1 exposes stable `WorldSnapshot` and `GameplayWorldSnapshot` data to the client frame loop.
- Phase 4 depends on Phase 3.
- Phase 5 can begin after Phase 3 event types are stable; visual-only pieces may start after Phase 2.
- Phase 6 must run last.

## Deliverables expected in repo after full implementation

New or updated plan/docs:

- Create `Plans/BspPresentationNetworkingPolish-AgentPlan.md`
- Create `Plans/ProjectStellar-BSP-PresentationNetworkingPolish-AgentPlan.md`
- Move older completed gameplay-loop plans into `Plans/Archived/bsp_gameplay_loop/`
- Update `Plans/NEXT.md`
- Update `docs/ImplementationStatus.md`
- Update `docs/Design.md`
- Update `docs/BspAuthoring.md`

Likely code areas:

- `include/stellar/client/Application.hpp`
- `include/stellar/client/ApplicationConfig.hpp`
- `src/client/Application.cpp`
- `src/client/ApplicationConfig.cpp`
- `include/stellar/client/LocalLoopbackRuntime.hpp`
- `src/client/LocalLoopbackRuntime.cpp`
- New or updated client runtime/script-loading helper files
- `include/stellar/scripting/ScriptRegistry.hpp` or adjacent helpers
- `include/stellar/server/WorldSession.hpp`
- `include/stellar/server/GameplayWorld.hpp`
- `include/stellar/graphics/LevelRenderer.hpp`
- `src/graphics/LevelRenderer.cpp`
- `include/stellar/graphics/RenderLevel.hpp`
- New `include/stellar/network/` and `src/network/` files if the networking phase adds the first transport-neutral layer

Likely test areas:

- `tests/client/`
- `tests/scripting/`
- `tests/server/`
- `tests/graphics/`
- `tests/network/` if added
- `tests/integration/`

## Suggested commit boundaries

Kilo can implement without committing, but if commits are used, keep boundaries like this:

1. `docs: archive completed bsp gameplay-loop plans`
2. `client: run scripted loopback runtime for scripted bsp maps`
3. `graphics: present gameplay snapshots as billboards`
4. `network: add snapshot event serialization contracts`
5. `network: add local transport bridge`
6. `presentation: add pickup hud/audio event polish`
7. `docs: finalize presentation networking polish status`

## Final success definition

The implementation run is complete when:

- A live `stellar-client --map <scripted-room.bsp>` path can execute authoritative Lua trigger/object-collider scripts through the same runtime used by display-free tests.
- Active pickups disappear or otherwise stop presenting after collection based on server-owned snapshot state.
- Scripted door/gate state is mirrored into presentation state without renderer-owned authority.
- Snapshot/event data has a transport-ready contract and deterministic serialization/delta tests.
- Local loopback can use the same message/snapshot contract planned for remote transport.
- Docs clearly mark old gameplay-loop plans archived and this follow-up run complete or in progress.
- Full default CTest and focused CTest suites pass.
