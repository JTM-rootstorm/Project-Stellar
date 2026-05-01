Prepared: 2026-05-01  
Branch target: `bsp-integration`  
Audience: Kilo Code agents / Codex-style implementation agents  
Objective: implement the next required BSP support hardening after the canonical BSP migration.

# Project Stellar — BSP Authoring and Presentation Hardening Master Plan

## 0. Executive intent

The BSP canonical migration is complete. Do **not** restart the migration and do **not** reintroduce
the retired model importer. The next implementation set should make BSP maps practical as the
authoring and presentation backbone for playable levels.

This plan expands the current `Plans/NEXT.md` direction into agent-ready implementation phases:

1. BSP diagnostics and data-contract foundation.
2. BSP PVS/leaf visibility and render culling.
3. BSP lightmap, texture, WAD/material fallback hardening.
4. BSP entity/sprite/object authoring conventions.
5. BSP validation tooling and generated-map regression fixtures.
6. Final hardening, documentation, and `NEXT.md` rewrite.

The goal is not full Quake/Half-Life feature emulation. The goal is a durable BSP-backed engine path
for Stellar: static BSP world geometry, server-authoritative collision and scripting, sprite
billboard entities/objects, lightweight materials, deterministic validation, and OpenGL/Vulkan
presentation parity.

---

## 1. Current branch facts to preserve

Read these before implementation:

1. `docs/ImplementationStatus.md`
2. `Plans/NEXT.md`
3. `docs/Design.md`
4. `AGENTS.md`
5. `include/stellar/assets/LevelAsset.hpp`
6. `include/stellar/import/bsp/Loader.hpp`
7. `src/import/bsp/BspBinary.hpp`
8. `src/import/bsp/BspLevelBuilder.cpp`
9. `include/stellar/graphics/RenderLevel.hpp`
10. `src/graphics/RenderLevel.cpp`
11. `tests/import/bsp/BspImporter.cpp`
12. `tests/integration/BspPlayableWorldSmoke.cpp`

Preserve these invariants:

- BSP maps remain the canonical playable level format.
- Server authority remains mandatory.
- Lua scripting remains mandatory and sandboxed.
- Runtime collision, movement, triggers, object colliders, and scripting remain backend-neutral.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable through the shared graphics abstraction.
- Do not add Source/VBSP support, third-party physics, dynamic rigid bodies, full PBR,
  client-side gameplay scripting, or the retired model importer.

---

## 2. Required outcomes

By the end of this plan:

- BSP importer reports actionable diagnostics for malformed, unsupported, or partially supported maps.
- `LevelVisibilityAsset` represents enough classic BSP leaf/PVS data for deterministic surface culling.
- `RenderLevel` can optionally cull static level surfaces through BSP visibility without changing
  gameplay authority.
- BSP lightmap and texture metadata is represented in source-neutral assets.
- Embedded miptex, external/WAD fallback, default materials, missing textures, and lightmaps have
  deterministic behavior and tests.
- BSP entity authoring conventions are documented and validated.
- Sprite billboard markers have enough authored metadata to be useful in real BSP maps.
- Generated BSP fixtures cover visibility, lightmaps, malformed lumps, entity conventions, and
  runtime integration.
- `docs/ImplementationStatus.md` records completion notes per phase.
- `Plans/NEXT.md` is rewritten after completion to the next active scope instead of pointing at
  completed BSP hardening work.

---

## 3. Agent routing

Use only the existing agents from `AGENTS.md`.

| Work | Primary | Notes |
| --- | --- | --- |
| Overall sequencing, active plan/NEXT/status docs, conflict resolution | `@director` | Only director routes and updates current handoff docs. |
| BSP parser, validation, LevelAsset data contracts, runtime/server invariants | `@carmack` | Owns backend-neutral data, importer safety, collision/runtime boundaries. |
| PVS render culling, lightmap/material upload, OpenGL/Vulkan parity | `@miyamoto` | Must stay behind `GraphicsDevice`/`RenderLevel`. |
| BSP entity authoring conventions, gameplay marker meaning, sprite/object metadata | `@kojima` | No client authority; coordinate through `@director`. |
| Audio | `@suzuki` | No required work unless a later phase scopes BSP audio entities. |
| Prototypes | `@molyneux` | Optional isolated experiments only if directed by `@director`. |

Specialist agents must not delegate. When a specialist hits a cross-domain decision, stop and report
to `@director`.

---

## 4. Serial gates

Gate order matters.

1. **Gate A — Active scope lock-in.** `Plans/NEXT.md` and `docs/ImplementationStatus.md` point to
   this BSP hardening plan.
2. **Gate B — Data contracts.** Visibility, lightmap/material, entity-property, and diagnostics
   contracts are added without breaking the current build.
3. **Gate C — PVS import and culling.** BSP nodes/leaves/marksurfaces/visibility import has tests,
   and renderer culling is optional/fallback-safe.
4. **Gate D — Lightmaps/materials.** Lightmap/texture/material resolver behavior is represented,
   deterministic, and tested.
5. **Gate E — Authoring conventions.** BSP entity key conventions are documented, parsed, validated,
   and covered by fixtures.
6. **Gate F — Validation toolkit.** Client/import validation surfaces meaningful diagnostics without
   requiring display/GPU.
7. **Gate G — Final handoff.** Plans archived, status updated, `NEXT.md` rewritten to post-hardening
   scope.

---

## 5. Parallelization after Gate B

After the Phase 1 data contracts land, these can run in parallel in separate worktrees/branches if
the implementation environment supports it:

- `@carmack`: BSP PVS/leaf parser and visibility asset conversion.
- `@miyamoto`: Render-level visibility-culling path against synthetic `LevelAsset` visibility data.
- `@miyamoto`: Lightmap/material upload tests against synthetic `LevelAsset` geometry.
- `@kojima`: Entity authoring conventions and parser/validation fixtures using raw entity strings.
- `@director`: Documentation inventory and authoring guide scaffolding.

Do not parallelize writes to these files:

- `CMakeLists.txt`
- `include/stellar/assets/LevelAsset.hpp`
- `include/stellar/import/bsp/Loader.hpp`
- `src/import/bsp/BspBinary.hpp`
- `src/import/bsp/BspLevelBuilder.cpp`
- `include/stellar/graphics/RenderLevel.hpp`
- `src/graphics/RenderLevel.cpp`
- `docs/ImplementationStatus.md`
- `docs/Design.md`
- `Plans/NEXT.md`
- `AGENTS.md`

---

## 6. Phase files

Implement one phase per PR/branch slice when practical:

1. `01-Phase0-Active-Handoff-NEXT.md`
2. `02-Phase1-BSP-Diagnostics-LevelAsset-Contracts.md`
3. `03-Phase2-BSP-PVS-Leaf-Visibility-Culling.md`
4. `04-Phase3-BSP-Lightmaps-Materials-WAD-Fallback.md`
5. `05-Phase4-BSP-Entity-Sprite-Authoring-Conventions.md`
6. `06-Phase5-BSP-Validation-Tooling-Fixtures.md`
7. `07-Phase6-Final-Hardening-NEXT-Completion.md`

---

## 7. Final validation target

Default validation must remain display-free:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Focused validation target after the full plan:

```bash
ctest --test-dir build -R '^(bsp_|render_level|runtime_world|client_map_validation|client_cli_map_validation|world_metadata_validation|collision_validation|scripted_world_session|bsp_playable_world_smoke)' --output-on-failure
```

Active reference search target:

```bash
git grep -n -i 'gltf\|SceneAsset\|STELLAR_ENABLE_GLTF\|cgltf' -- . ':!Plans/Archived/**' ':!build*/**'
```

Expected: no active retired-importer functionality. Compatibility aliases may remain only if they are
documented as temporary and do not preserve old importer behavior.

---

## 8. Stop conditions

Stop and report to `@director` if:

- Implementing PVS requires changing gameplay/collision authority.
- BSP visibility culling would drop visible geometry without a deterministic fallback.
- Lightmap/material work requires full PBR or backend-specific renderer APIs.
- Entity conventions conflict with existing Lua command validation or object-collider semantics.
- Default tests begin requiring GPU/display.
- A phase requires Source/VBSP support.
- A specialist cannot resolve a failure after two serious attempts.
