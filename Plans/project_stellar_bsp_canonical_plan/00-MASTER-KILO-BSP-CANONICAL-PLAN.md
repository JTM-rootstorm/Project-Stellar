# Project Stellar — BSP Canonical Level Format Migration Plan

Prepared: 2026-05-01  
Branch target: `bsp-integration`  
Audience: Kilo Code agents / Codex-style implementation agents  
Primary objective: make classic Half-Life/Quake-style BSP maps the canonical level format and remove glTF functionality from the engine.

---

## 0. Executive decision

The user has selected BSP maps as the canonical level format. Implement this as a hard direction change, not as an optional importer next to glTF.

Required outcome:

- BSP is the default and canonical playable level source.
- glTF import, glTF startup validation, glTF CMake options, glTF test fixtures, cgltf dependency, glTF-specific docs, and glTF-specific runtime assumptions are removed from active code and active docs.
- Existing server-authoritative movement, collision, Lua scripting, trigger, object-collider, billboard sprite, and display-free validation boundaries are preserved.
- `Plans/NEXT.md` is updated at the beginning of the work to point to this migration and is rewritten after completion to point at the next post-BSP scope.

Do not treat this as “add BSP support while keeping glTF.” Treat it as a migration to BSP-first level assets.

---

## 1. Current branch context to preserve

Read these files before implementing:

1. `docs/ImplementationStatus.md`
2. `Plans/NEXT.md`
3. `docs/Design.md`
4. `AGENTS.md`
5. The current collision/runtime/server/scripting code under:
   - `include/stellar/assets/`
   - `include/stellar/world/`
   - `include/stellar/physics/`
   - `include/stellar/server/`
   - `include/stellar/scripting/`
   - matching `src/` and `tests/`

Existing branch facts to preserve:

- The server is authoritative.
- Client remains presentation only.
- Lua scripting is mandatory server-authoritative infrastructure.
- Existing collision and runtime world code is backend-neutral and display-free.
- Default validation must remain display-free.
- OpenGL and Vulkan remain runtime-selectable through the shared graphics abstraction.
- Avoid third-party physics, dynamic rigid bodies, client-side scripting, full PBR, and model/animation systems unless explicitly reintroduced by a later user decision.

---

## 2. Scope

### In scope

- Introduce source-neutral level asset structures centered on BSP maps.
- Implement a BSP importer for the classic Quake/GoldSrc BSP family.
- Make BSP import feed static level geometry, collision, metadata/entities, triggers, object-collider sensors, script bindings, and sprite markers.
- Refactor runtime world assembly to consume `LevelAsset` rather than glTF-shaped `SceneAsset`.
- Refactor client startup validation from `--asset` glTF loading to BSP map loading.
- Render BSP level geometry through the existing OpenGL/Vulkan graphics abstraction.
- Keep billboard sprites as the entity/object presentation path.
- Remove glTF importer code, cgltf dependency, glTF CMake option, glTF tests, glTF fixtures, and active glTF documentation.
- Update `docs/Design.md`, `docs/ImplementationStatus.md`, `AGENTS.md`, and `Plans/NEXT.md` to reflect the BSP canonical direction.

### Out of scope

- Source engine VBSP support.
- Full external WAD/material library tooling beyond a minimal resolver or placeholder material path.
- Full Quake/Half-Life gameplay entity emulation.
- Moving brush model simulation.
- Dynamic rigid bodies.
- Navigation meshes/pathfinding.
- Full PBR.
- 3D model/skinning/animation support.
- Client-side gameplay scripting.

### BSP family assumption

Implement the classic BSP family in a versioned way:

- Required initial targets: Quake BSP29 and GoldSrc/Half-Life BSP30 style data.
- Reject unknown BSP versions with deterministic errors.
- Parse and store classic lumps behind version-aware structs.
- Defer Source/VBSP and modern derivative formats.

When exact lump differences matter, encode the distinction explicitly in tests and in the importer API. Do not silently guess.

---

## 3. Agent routing

Use the existing agents only. Do not create new agents or subagents.

| Work | Primary | Notes |
| --- | --- | --- |
| Overall sequencing, docs, conflict resolution | `@director` | Only director delegates and updates active handoff docs. |
| Asset contracts, BSP parser, collision, runtime/server integration, CMake removal | `@carmack` | Core implementation owner. |
| BSP rendering, OpenGL/Vulkan upload/draw parity, billboard preservation | `@miyamoto` | Coordinate with `@director` when touching shared asset contracts. |
| BSP entity metadata mapping, trigger/object-collider semantics, script-binding vocabulary | `@kojima` | Coordinate with server/runtime through `@director`. |
| Audio | `@suzuki` | No required work in this migration unless level entities produce audio events later. |
| Prototypes | `@molyneux` | Optional isolated BSP parsing prototype only if directed. |

---

## 4. Serial gates and parallel work

### Serial gates

Do not skip these gates:

1. **Gate 0 — Decision/doc lock-in:** active docs and `NEXT.md` point to BSP canonical migration.
2. **Gate 1 — LevelAsset contract:** source-neutral level asset API exists and tests compile without introducing BSP parser yet.
3. **Gate 2 — BSP parser output:** importer parses minimal BSP fixtures into `LevelAsset` with geometry, collision, and entity metadata.
4. **Gate 3 — Runtime/server/client integration:** `RuntimeWorld`, `WorldSession`, movement, triggers, object-collider sensors, scripts, and client validation operate from BSP-backed `LevelAsset`.
5. **Gate 4 — BSP rendering:** OpenGL/Vulkan render BSP static geometry plus billboards through shared abstractions.
6. **Gate 5 — glTF deletion:** active code/build/tests/docs have no functional glTF/cgltf dependency.
7. **Gate 6 — final docs/NEXT:** `ImplementationStatus.md` has completion notes, the plan is archived, and `NEXT.md` points at the next post-BSP scope.

### Parallelizable work after Gate 1

These can run in parallel after the LevelAsset contract is committed or otherwise stable:

- `@carmack`: BSP binary parser, lump validation, fixture builder, and geometry/collision conversion.
- `@kojima`: entity lump key/value parser and metadata mapping tests using parser-independent strings.
- `@miyamoto`: render-scene refactor from scene graph/glTF pose assumptions to static level geometry/billboard draw input, initially with synthetic `LevelAsset` data.
- `@director`: documentation and removal inventory, without deleting active code until replacements compile.

### Do not parallelize these writes

Only one agent should own each of these at a time:

- `CMakeLists.txt`
- `include/stellar/assets/LevelAsset.hpp`
- `include/stellar/assets/SceneAsset.hpp` deletion/replacement
- `include/stellar/world/RuntimeWorld.hpp`
- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `Plans/NEXT.md`
- `AGENTS.md`

---

## 5. Required phase files

This package contains the following implementation plans:

1. `01-Phase0-Decision-Docs-NEXT.md`
2. `02-Phase1-LevelAsset-SourceNeutralization.md`
3. `03-Phase2-BSPImporter-Entities-Collision.md`
4. `04-Phase3-Runtime-Server-Client-Integration.md`
5. `05-Phase4-BSP-Rendering-Billboards.md`
6. `06-Phase5-Remove-glTF-Cleanup.md`
7. `07-Phase6-Final-Hardening-NEXT-Completion.md`

The recommended implementation branch path is one phase per PR/branch slice. If the implementation environment supports safe parallel worktrees, use the parallelization rules above after Phase 1.

---

## 6. Final validation target

At the end of the migration, these commands should pass without any glTF option:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Additional active-source search target:

```bash
rg -n -i "gltf|cgltf|STELLAR_ENABLE_GLTF" \
  --glob '!Plans/Archived/**' \
  --glob '!build*/**'
```

Expected result: no active code, active tests, active CMake, active docs, or active agent-routing references to glTF/cgltf. Historical archived plans may retain old references if clearly archived and not referenced as current work.

---

## 7. NEXT.md policy

`Plans/NEXT.md` must not remain pointed at the old removable-complexity cleanup.

Required handling:

1. **Phase 0:** Replace `Plans/NEXT.md` with a short active handoff that says the current active scope is BSP canonical migration and points to the new plan under `Plans/`.
2. **Each phase:** Update `docs/ImplementationStatus.md` with completion notes. Keep `NEXT.md` short; only update it when the active phase pointer changes or the next implementation entry point changes.
3. **Final phase:** Move the detailed BSP migration plan to `Plans/Archived/` and rewrite `NEXT.md` as the next active handoff after BSP migration. Suggested next post-BSP scope: BSP map authoring polish, BSP PVS/render culling hardening, sprite/entity authoring pipeline, or gameplay loop expansion. Do not leave `NEXT.md` pointing at completed work.

---

## 8. Stop conditions

Stop and report to `@director` if any of these occur:

- BSP format requirements are unclear enough to change file format support scope.
- The LevelAsset contract would collapse server authority or require renderer-owned gameplay state.
- The renderer requires glTF scene graph/skinning/animation to remain in active code for BSP rendering.
- Removing glTF breaks unrelated mandatory systems in a way that cannot be repaired in two focused attempts.
- Any test requires a GPU/display by default.
- A specialist reaches a cross-domain decision boundary.

