# Project Stellar: Next Implementation Plans

Branch target: `bsp-integration`

## BSP Authoring and Presentation Hardening — Active

Active implementation plan package:

- `Plans/ProjectStellar-BSP-NextSupport-KiloPlan/00-MASTER-KILO-BSP-HARDENING-PLAN.md`
- Phase files in `Plans/ProjectStellar-BSP-NextSupport-KiloPlan/`

This work hardens BSP maps as the already-canonical playable level format. It must not restart the
BSP migration, reintroduce retired importer functionality, or weaken server authority, mandatory Lua
sandboxing, display-free default tests, or OpenGL/Vulkan abstraction parity.

Planned phase status:

- Phase 0 — Active handoff and `NEXT.md` lock-in: complete as of 2026-05-01.
- Phase 1 — BSP diagnostics and `LevelAsset` contract foundation: complete as of 2026-05-01.
- Phase 2 — BSP PVS, leaf visibility, and render culling: not started.
- Phase 3 — BSP lightmaps, textures, materials, and WAD fallback: not started.
- Phase 4 — BSP entity, sprite, trigger, and object authoring conventions: not started.
- Phase 5 — BSP validation tooling and regression fixtures: not started.
- Phase 6 — Final hardening, documentation, archive, and `NEXT.md` completion: not started.

Phase 1 completion notes:

- Added structured BSP import diagnostics and additive load-with-report APIs while preserving the
  existing `std::expected<LevelAsset, Error>` load path for fatal parse errors.
- Expanded source-neutral `LevelAsset` visibility/lightmap contracts with classic BSP leaf records,
  compressed PVS bytes, raw lighting bytes, and minimal lightmap metadata placeholders.
- Parser internals now retain nodes, leaves, marksurfaces, visibility bytes, and lighting bytes for
  later PVS/lightmap hardening without adding renderer handles or gameplay authority.
- Added display-free importer coverage for non-fatal missing player spawn warnings, fatal unexpected
  errors, no-vis placeholders, and raw visibility/lighting lumps.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_importer|runtime_world|collision_world|world_metadata_validation|collision_validation)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure and build succeeded. Targeted BSP/runtime/collision tests passed 5/5, and full
default CTest passed 31/31.

Phase 0 completion notes:

- Locked `Plans/NEXT.md` to the BSP hardening plan package as the active implementation entry point.
- Recorded this hardening work as the current active branch scope without restarting BSP migration.
- Updated `docs/Design.md` to describe current BSP hardening rather than the completed migration as
  the near-term goal.
- No source or build behavior changes were introduced.

## BSP Canonical Migration — Complete

BSP canonical migration is complete as of 2026-05-01:

- BSP maps are now the canonical playable level format.
- Retired importer functionality, parser dependency, feature gates, startup paths, tests, and active
  docs have been removed.
- Runtime world assembly, static collision, server-authoritative movement, triggers,
  object-collider sensors, Lua hooks, client map validation, and level rendering operate from
  BSP-backed `LevelAsset` data.
- `Plans/NEXT.md` now points to BSP authoring and presentation hardening as the next recommended
  active scope.
- The completed migration plan is archived at
  `Plans/Archived/project_stellar_bsp_canonical_plan/`.

Final validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(bsp_|runtime_world|collision_world|character_controller|server_world_session|scripted_world_session|bsp_playable_world_smoke|bsp_scripted_collision_smoke|bsp_scripted_object_collider_smoke|billboard|render_level)' --output-on-failure
```

Result: configure/build succeeded, full CTest passed 31/31, and focused BSP/runtime/render/script
CTest passed 13/13.

Active retired-importer reference search: no active hits. `git grep` was used because `rg` is
unavailable in this environment.

Known deferred post-BSP items:

- BSP PVS/leaf visibility culling hardening.
- BSP lightmap presentation.
- External texture/WAD/material resolver polish.
- BSP editor/toolchain and map-authoring documentation.
- Sprite/entity authoring conventions.
- Gameplay loop expansion.

## BSP Canonical Migration — Phase Notes

The user selected BSP maps as the canonical playable level format. BSP replaces the retired
pre-BSP model-import path as the active level pipeline rather than being kept beside it.

Archived implementation entry points:

- `Plans/NEXT.md`
- `Plans/Archived/project_stellar_bsp_canonical_plan/00-MASTER-KILO-BSP-CANONICAL-PLAN.md`

Required migration outcome:

- BSP is the default and canonical playable level source.
- Retired importer code, startup validation, CMake options, fixtures, parser dependency, runtime
  assumptions, and active docs are removed during this migration.
- Existing server-authoritative movement, collision, Lua scripting, trigger, object-collider,
  billboard sprite, and display-free validation boundaries are preserved.

Phase BSP-0 is complete as of 2026-05-01:

- Locked active branch direction to BSP maps as canonical playable level format.
- Marked retired importer functionality for removal rather than side-by-side support.
- Replaced `Plans/NEXT.md` with the BSP migration handoff.
- Updated active design/agent guidance so new work no longer follows retired level assumptions.

Validation: documentation review and active-reference search.

Phase BSP-1 is complete as of 2026-05-01:

- Added source-neutral `LevelAsset` as the canonical runtime level input.
- Moved runtime world assembly off the retired scene-asset contract.
- Preserved existing backend-neutral collision, trigger, object-collider, scripting, and
  server-authority seams.
- Updated synthetic display-free tests to construct runtime worlds from `LevelAsset`.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(runtime_world|collision_world|character_controller|trigger_system|object_collider|server_world_session|scripted_world_session)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: default configure and build succeeded. Targeted Phase BSP-1 suite passed 7/7, and full
default CTest passed 26/26.

Phase BSP-2 is complete as of 2026-05-01:

- Added mandatory `stellar_import_bsp` loader for the classic BSP29/BSP30 family.
- Added safe lump parsing, entity key/value parsing, and BSP-to-`LevelAsset` conversion.
- Built static geometry, named collision meshes, world metadata markers, and script bindings from
  BSP data.
- Added display-free BSP parser/importer regression tests using generated binary fixtures.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_|runtime_world|collision_world|world_metadata_validation|collision_validation)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: default configure and build succeeded. The phase-plan targeted command completed
successfully for the matching runtime/collision tests; a corrected CTest regex also ran the new
`bsp_importer` test for 5/5 targeted passes. Full default CTest passed 27/27.

Phase BSP-3 is complete as of 2026-05-01:

- Runtime world assembly consumes source-neutral BSP-backed `LevelAsset` data and exposes object
  collider marker lookup alongside existing spawn, trigger, and sprite marker helpers.
- Server movement, static collision filters, triggers, object-collider sensors, and Lua script hooks
  are covered from generated BSP metadata through authoritative `ScriptedWorldSession` ticks.
- Client startup validation now uses `--map`/`map_path`, loads `.bsp` maps through
  `stellar_import_bsp`, and no longer has a retired importer feature-gate validation failure path.
- Added display-free BSP playable-world/scripted integration smoke coverage plus BSP-backed client
  map validation and CLI validation tests.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_|playable_world_smoke|scripted_playable_world_smoke|scripted_collision_smoke|scripted_object_collider_smoke|server_world_session|scripted_world_session|client_map_validation_smoke|client_cli_map_validation)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: default configure and build succeeded. The phase-plan targeted command passed 9/9 selected
tests in this CTest regex mode, including server/session, scripted session, client map validation,
CLI map validation, and BSP-backed playable/scripted smoke aliases. A corrected CTest regex also ran
`bsp_importer` and `bsp_playable_world_smoke` for 11/11 targeted passes. Full default CTest passed
33/33.

Phase BSP-4 is complete as of 2026-05-01:

- Refactored active graphics presentation to consume BSP/`LevelAsset` static geometry through
  `RenderLevel`/`LevelRenderer` instead of retired scene data.
- Preserved OpenGL/Vulkan backend parity through the shared `GraphicsDevice` abstraction; no
  BSP-specific raw backend API was added.
- Preserved billboard sprite rendering after static level geometry and updated display-free
  upload/submission tests for BSP geometry, default material fallback, surface material indices,
  and billboard ordering.
- Removed active renderer assumptions around scene graph pose, skinning, animation, and PBR-style
  materials; retained temporary compatibility aliases for old `RenderScene`/`SceneRenderer` names
  while active APIs are LevelAsset-focused.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(render_level|render_scene|billboard|graphics_backend_selection)$' --output-on-failure
ctest --test-dir build -R '^(render_level_.*|graphics_backend_selection)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: default configure and build succeeded. The phase-plan targeted regex matched and passed
`graphics_backend_selection`; the corrected graphics regex ran `render_level_upload`,
`render_level_inspection`, and `graphics_backend_selection` for 3/3 passes. Full default CTest passed
33/33.

Phase BSP-5 is complete as of 2026-05-01:

- Removed the retired model-importer code path, parser dependency, feature gate, fixtures, and
  integration tests from active source, build configuration, and default validation.
- Removed retired scene, skin, animation asset/runtime types and active graphics draw plumbing that
  only served the retired model path.
- Kept BSP-backed level import/runtime/render tests and renamed scripted integration smoke
  registrations to BSP names.
- Cleaned active docs and agent guidance so BSP is the canonical level format and the retired model
  importer is no longer an active feature.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Result: configure and build succeeded; full CTest passed 31/31.

Active retired-importer reference search: no active hits. `git grep` was used because `rg` is
unavailable in this environment.

## Historical Context

Earlier collision, movement, billboard, metadata, Lua scripting, trigger, and object-collider work
remains useful historical context, but it is not the active branch plan. The active branch direction
is now BSP-first level import, runtime assembly, server-authoritative gameplay seams, and rendering.

Preserve these invariants in follow-up work:

- Server authority remains mandatory.
- Lua scripting remains mandatory and sandboxed.
- Default tests remain display-free.
- OpenGL and Vulkan remain runtime-selectable through shared abstractions.
- Source/VBSP, moving brush simulation, third-party physics, dynamic rigid bodies, full PBR,
  model/animation systems, and client-side gameplay scripting remain out of scope unless explicitly
  requested.
