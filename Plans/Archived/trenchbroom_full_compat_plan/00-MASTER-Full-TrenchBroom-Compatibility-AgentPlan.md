# Project Stellar — Full TrenchBroom Compatibility Master Agent Plan

Target branch: `trenchbroom-compat`

Authoring target: TrenchBroom using the project-owned **Stellar** game profile, classic/GoldSrc BSP30 output, Z-up coordinates, and 1 Stellar unit = 1 inch.

## Mission

Move the current TrenchBroom BSP30 workflow from "usable compatibility fixtures" to **real full Stellar/TrenchBroom compatibility**:

- A user can install or copy the Stellar TrenchBroom package, create a new map, browse materials, place all supported entities through smart properties, compile to BSP30, validate, launch, and iterate without hand-editing generated files.
- TrenchBroom-authored lights produce visible lit BSP output in the engine.
- Editor-exposed entities have runtime/import support. Nothing advertised in the Stellar FGD is metadata-only unless that is the actual final intended behavior and is documented as such.
- Important editor-facing features are implemented, not postponed: package installation, material/WAD workflow, lighting, point and brush volumes, moving doors/buttons, source-map preflight, compile/validate integration, fixtures, and manual QA gates.

This plan intentionally scopes "full compatibility" to the **Stellar TrenchBroom game package and BSP30 pipeline**. It does not require supporting Source/VBSP, every Quake/Half-Life entity, or arbitrary third-party game configs. It does require all classes and workflows exposed by the Stellar package to be complete.

## Current branch facts to preserve

- Branch already has `tools/trenchbroom/Stellar/` with `GameConfig.cfg`, `CompilationProfiles.cfg`, materials, WAD, and `stellar_entities.fgd`.
- Branch already has BSP30 compile/validate wrappers in `tools/bsp/` and an optional VHLT fixture matrix.
- Branch already uses Z-up and imports BSP coordinates 1:1.
- Branch already has source fixtures under `tests/fixtures/trenchbroom/src/` and generated BSP fixture support.
- Branch already has BSP importer support for texture names, visibility, lighting bytes, lightmap metadata, world metadata, script aliases, and deterministic developer texture fallbacks.
- Branch already has server-authoritative scripting and static collision enable/disable overlays.

## Gaps this plan closes

1. Package can be repo-local but is fragile when copied into TrenchBroom's games directory.
2. `GameConfig.cfg` references `Icon.png`, but the file is absent.
3. Compile profile parameter paths are not robust to spaces or copied package installs.
4. FGD and importer are not perfectly aligned: point-authored volume fallbacks exist but FGD exposes only solid trigger/collider classes.
5. The package does not expose important entity classes and properties needed for real authored maps, especially lights and common brush/entity behavior.
6. Runtime docs say editor-visible WAD generation is deferred even though a WAD generator and WAD file now exist.
7. Developer texture aliases are not fully normalized across source maps, VHLT work maps, WAD texture names, and runtime fallbacks.
8. External WAD texture loading is missing, so real TrenchBroom maps can compile but render fallback/missing materials unless texture pixels are embedded or developer fallbacks are used.
9. BSP lightmap import exists but backend rendering must be completed and validated so TrenchBroom-authored lights are visible in OpenGL/Vulkan.
10. `func_door` and `func_button` are currently advertised as static metadata; this plan makes them real runtime/presentation/collision entities.
11. Source `.map` preflight is too shallow; malformed brushes can reach VHLT and produce late compiler errors.
12. End-to-end fixture coverage lacks lit room, material/WAD room, moving brush room, point-volume room, copied-package compile, and manual TrenchBroom smoke checklists.

## Phase order

Run phases in this order unless parallelization notes explicitly permit overlap:

1. **TB-FULL-01 — Package install and compile profile hardening**
2. **TB-FULL-02 — FGD/entity authoring contract alignment**
3. **TB-FULL-03 — Material, WAD, texture, and source-map preflight pipeline**
4. **TB-FULL-04 — BSP lighting and renderer lightmap completion**
5. **TB-FULL-05 — Runtime brush/entity behavior for real authored maps**
6. **TB-FULL-06 — End-to-end fixtures, CI gates, and manual TrenchBroom QA**
7. **TB-FULL-07 — Documentation, handoff, and final compatibility audit**

## Parallelization map

- TB-FULL-01 and TB-FULL-02 can start in parallel after both agents read the current package and FGD.
- TB-FULL-03 can start in parallel with TB-FULL-02 for WAD/runtime alias work, but source-map validation should wait until the updated FGD class list is stable.
- TB-FULL-04 can run in parallel with TB-FULL-05 if both agents agree on any `LevelAsset`/render material structure changes before editing shared headers.
- TB-FULL-06 must wait for TB-FULL-01 through TB-FULL-05.
- TB-FULL-07 must wait for all implementation phases and final validation logs.

## Global constraints for every agent

- Do not commit unless the user explicitly asks.
- Keep default CTest display-free and independent of external BSP compilers.
- Optional external compiler/VHLT coverage must skip with return code `77` when tools are unavailable.
- Preserve server authority: editor/import/runtime/presentation code must not create client-side gameplay authority.
- Preserve Lua sandbox policy: import never executes scripts; runtime loads only asset-relative scripts; absolute paths and `..` escapes fail deterministically.
- Preserve Z-up/1-inch scale policy.
- Keep branch docs current as implementation lands.
- Tests must be deterministic and safe on Linux CI.
- If public APIs are added or changed, add Doxygen `@brief` comments consistent with `docs/Design.md`.

## Final definition of done

A clean checkout on `trenchbroom-compat` passes all of the following:

### Automated default validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

### Focused TrenchBroom validation

```bash
ctest --test-dir build -R '^(bsp_|client_map_validation|client_cli|server_cli|render_level|graphics_backend_selection|trenchbroom|world_axes|character_controller|collision_world|runtime_world|server_world_session|scripted_world_session|networked_client_runtime|dedicated_server)' --output-on-failure
```

### Optional VHLT validation when tools are present

```bash
tools/bsp/run_vhlt_fixture_matrix.sh --source-root . --build-root build --profile full --keep-going
```

Expected: all positive fixtures compile and validate; negative fixtures fail for expected diagnostics only.

### Manual TrenchBroom validation performed by user

The final docs must provide a checklist for the user to manually verify:

1. Install package repo-local.
2. Copy package into TrenchBroom games directory and point it at `STELLAR_REPO_ROOT`.
3. Create a new map from the Stellar profile/template.
4. Confirm materials and WAD/PNG thumbnails are visible.
5. Place all supported FGD classes with smart properties.
6. Build a lit room with `light` and `light_spot`.
7. Build a moving door opened by trigger/button.
8. Compile through TrenchBroom profile.
9. Validate with Stellar client/server.
10. Launch local client and confirm geometry, lighting, materials, sprites, triggers, pickups, doors/buttons, and collision behave correctly.

## Deliverables

- Updated package under `tools/trenchbroom/Stellar/`.
- Updated compile/validate wrappers and optional install helper scripts.
- Updated FGD(s) and importer alias/entity handling.
- Runtime WAD/texture loading and alias normalization.
- Complete BSP lightmap rendering in OpenGL and Vulkan.
- Runtime moving brush/entity system for advertised moving brush classes.
- Expanded source `.map` and compiled/generated fixtures.
- Display-free tests and optional VHLT matrix coverage.
- Updated docs: `docs/TrenchBroom.md`, `docs/BspAuthoring.md`, `docs/ImplementationStatus.md`, package README, fixture README.
- Final audit report in `Plans/NEXT.md` or an equivalent active handoff file.
