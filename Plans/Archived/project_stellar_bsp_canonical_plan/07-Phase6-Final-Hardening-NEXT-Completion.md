# Phase 6 — Final Hardening, Documentation, and NEXT.md Completion

Branch target: `bsp-integration`  
Primary agent: `@director`  
Implementation support: `@carmack`, `@miyamoto`, `@kojima` as needed  
Dependencies: Phases 0–5 complete  
Parallelizable: no for final docs; targeted fixes can be assigned by `@director`.

---

## Goal

Harden the BSP migration, verify active retired model importer removal, archive completed plan files, and rewrite `Plans/NEXT.md` so it points to the next active scope rather than completed BSP migration work.

---

## Final hardening checklist

### Build and test

Run full default validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Run focused suites:

```bash
ctest --test-dir build -R '^(bsp_|runtime_world|collision_world|character_controller|server_world_session|scripted_world_session|bsp_playable_world_smoke|bsp_scripted_collision_smoke|bsp_scripted_object_collider_smoke|billboard|render_level)' --output-on-failure
```

Run active-reference cleanup:

```bash
rg -n -i "retired model importer|retired parser dependency|retired importer feature gate" \
  --glob '!Plans/Archived/**' \
  --glob '!build*/**'
```

Expected: no active hits.

### Source review

Verify:

- BSP is built by default.
- There is no retired model importer build option.
- Client uses map/BSP terminology.
- Runtime world consumes `LevelAsset`.
- Server movement/collision/triggers/object colliders/scripts still operate display-free.
- Renderer no longer depends on retired model importer scene graph/animation/skinning.
- Billboard sprites still render through source-neutral draw data.
- Docs do not call retired model importer a future or active requirement.

---

## Documentation updates

### `docs/Design.md`

Ensure final design says:

- BSP maps are canonical playable level format.
- Levels use 3D BSP geometry and 2D billboard entities/objects/props.
- BSP import produces static geometry, collision, metadata, triggers, object-collider sensors, and script bindings.
- Lua scripting is server-authoritative.
- retired model importer has been removed from active engine scope.
- Source/VBSP, moving brush simulation, external WAD/tooling polish, PVS hardening, dynamic bodies, navmesh, and full PBR are deferred unless later requested.

### `docs/ImplementationStatus.md`

Add final migration completion summary at the top.

Required summary:

- BSP canonical migration complete.
- Phase-by-phase bullets with validation result.
- Current source of truth after migration.
- Known deferred items.

Suggested deferred items:

- BSP PVS/leaf culling hardening.
- BSP lightmap presentation if not complete.
- External texture/WAD/material resolver if not complete.
- BSP editor/toolchain documentation.
- Sprite/entity authoring conventions.
- Gameplay loop expansion.

### `AGENTS.md`

Verify no active retired model importer routing references remain. Keep no-new-agents and director-only-delegation rules unchanged.

---

## Plan archival

If implementation agents commit this plan into the repo:

1. Keep active detailed plan at `Plans/ProjectStellar-BSPCanonical-AgentPlan.md` during work.
2. At completion, move it to:

```text
Plans/Archived/ProjectStellar-BSPCanonical-AgentPlan.md
```

3. Update `docs/ImplementationStatus.md` to say the plan is archived and historical.

---

## NEXT.md final rewrite

Do not leave `Plans/NEXT.md` pointing to completed BSP migration.

Rewrite it to a short post-BSP handoff. Pick exactly one next active scope. Recommended default:

```markdown
# Stellar Engine — Current Branch Handoff

Branch target: `bsp-integration`

## Current entry point

Use `docs/ImplementationStatus.md` as the current branch-facing source of truth.

## Post-BSP state

BSP maps are now the canonical playable level format. retired model importer functionality has been removed from active
code, build configuration, tests, and active docs. Runtime world assembly, server-authoritative
movement/collision, triggers, object-collider sensors, Lua hooks, client map validation, and BSP level
rendering operate from BSP-backed `LevelAsset` data.

## Next recommended active scope

Recommended next scope: BSP authoring and presentation hardening.

Focus areas:

- BSP PVS/leaf visibility culling if not fully active.
- BSP lightmap/material/texture fallback hardening.
- Sprite/entity authoring conventions for BSP entity keys.
- Map validation diagnostics for malformed or unsupported BSP content.

## Invariants

- Server authority remains mandatory.
- Lua scripting remains mandatory and sandboxed.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable.
- No retired model importer, Source/VBSP, third-party physics, dynamic rigid bodies, full PBR, or client-side gameplay scripting unless explicitly requested.
```

If another next scope is more appropriate after implementation findings, `@director` may choose it, but `NEXT.md` must point to future work, not completed migration.

---

## Acceptance criteria

- Full default build and CTest pass.
- Focused BSP/runtime/render/script tests pass.
- Active retired model importer/retired parser dependency reference search is clean.
- `docs/Design.md` and `docs/ImplementationStatus.md` reflect final BSP canonical state.
- Plan is archived if it was committed.
- `Plans/NEXT.md` points to the next active scope after BSP migration.

---

## Completion note template

Add to `docs/ImplementationStatus.md`:

```markdown
BSP canonical migration is complete as of YYYY-MM-DD:

- BSP maps are now the canonical playable level format.
- retired model importer functionality, retired parser dependency dependency, retired model importer feature gates, retired model importer startup paths, retired model importer tests, and active retired model importer docs have been removed.
- Runtime world assembly, static collision, server-authoritative movement, triggers, object-collider sensors, Lua hooks, client map validation, and level rendering operate from BSP-backed `LevelAsset` data.
- `Plans/NEXT.md` has been rewritten to the next active post-BSP scope, and the BSP migration plan has been archived.

Validation run:

<commands and result>

Active retired model importer/retired parser dependency reference search:

<command and result>
```
