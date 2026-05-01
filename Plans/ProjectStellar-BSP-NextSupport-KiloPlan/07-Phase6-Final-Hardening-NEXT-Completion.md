Prepared: 2026-05-01  
Branch target: `bsp-integration`  
Audience: Kilo Code agents / Codex-style implementation agents  
Objective: implement the next required BSP support hardening after the canonical BSP migration.

# Phase 6 — Final Hardening, Documentation, Archive, and NEXT.md Completion

## Purpose

Finish the BSP hardening implementation with a full validation pass, status updates, archived plan
files, and a new `NEXT.md` pointing at the next post-hardening active scope.

## Primary agent

`@director`, with targeted support from specialists for final fixes.

## Files likely touched

- `docs/ImplementationStatus.md`
- `docs/Design.md`
- `docs/BspAuthoring.md`
- `Plans/NEXT.md`
- `Plans/Archived/...`
- possibly `AGENTS.md` only if active routing still contains outdated assumptions

## Tasks

1. Run full default validation.
2. Run focused BSP validation.
3. Search for active stale references.
4. Review docs for consistency:
   - BSP remains canonical.
   - Migration is complete.
   - Hardening features are described accurately.
   - Deferred items are explicit.
5. Move the active detailed hardening plan into:
   - `Plans/Archived/project_stellar_bsp_hardening_plan/`
6. Rewrite `Plans/NEXT.md` to the next active scope.

## Suggested next scopes after this plan

Choose one as the next active handoff. Recommended order:

1. **Gameplay loop expansion over BSP maps**
   - ECS/entity spawn from BSP metadata.
   - Player presentation from authoritative snapshots.
   - Sprite/animation/interaction loop.
   - Item pickup and scripted doors/gates using existing Lua command path.

2. **BSP toolchain/editor workflow polish**
   - map authoring templates;
   - sample entity definition files for editors;
   - map validation UX;
   - optional fixture map repository policy.

3. **Networking/snapshot expansion**
   - server snapshot serialization;
   - local loopback-to-network bridge;
   - client interpolation.

Do not leave `NEXT.md` pointing at completed hardening work.

## Final validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(bsp_|render_level|runtime_world|client_map_validation|client_cli_map_validation|world_metadata_validation|collision_validation|scripted_world_session|bsp_playable_world_smoke)' --output-on-failure
git grep -n -i 'STELLAR_ENABLE_GLTF\|cgltf\|SceneAsset\|gltf' -- . ':!Plans/Archived/**' ':!build*/**'
```

Expected:

- Full CTest passes.
- Focused BSP suite passes.
- Search has no active retired-importer functionality.
- Any temporary compatibility names are documented and do not preserve old importer behavior.

## ImplementationStatus completion note template

Add a top section like:

```markdown
## BSP Authoring and Presentation Hardening — Complete

Complete as of YYYY-MM-DD:

- Added structured BSP import/authoring diagnostics.
- Added BSP leaf/PVS visibility data and optional presentation culling.
- Added BSP lightmap/material/texture fallback support.
- Documented and validated BSP entity authoring conventions.
- Added generated fixtures and headless validation coverage.
- Archived the implementation plan at `Plans/Archived/project_stellar_bsp_hardening_plan/`.
- `Plans/NEXT.md` now points to <next active scope>.

Validation:
<commands and results>

Known deferred:
- Source/VBSP
- moving brush simulation
- dynamic rigid bodies
- full PBR
- client-side gameplay scripting
- <any remaining explicit BSP limitations>
```

## Stop conditions

Stop if final validation cannot pass after two focused fixes by the owning specialist. Report the
failure to `@director` with exact failing targets, likely cause, and recommended next action.
