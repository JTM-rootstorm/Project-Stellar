# Phase 5 — Remove glTF Functionality and Cleanup Active References

Branch target: `bsp-integration`  
Primary agent: `@carmack` for build/core cleanup  
Renderer cleanup: `@miyamoto` through `@director`  
Coordinator: `@director`  
Dependencies: Phases 1–4 complete enough that BSP-backed runtime/render/tests compile  
Parallelizable: inventory can be parallel; deletion and CMake edits should be serial.

---

## Goal

Remove glTF functionality from active code, build configuration, tests, active docs, and active agent-routing guidance.

This is not an optional-disable phase. glTF should not remain as a feature gate, dependency, importer, startup path, or canonical authoring path.

---

## Delete or retire active glTF code

Remove active glTF importer code:

- `src/import/gltf/`
- `include/stellar/import/gltf/` if present
- glTF loader/importer tests under `tests/import/gltf/`
- glTF fixtures under `tests/fixtures/gltf/`
- `thirdparty/cgltf/`
- `.kilo/plans` or active plan references to glTF only if still active; historical archived plans may remain.

Remove build configuration:

- `STELLAR_ENABLE_GLTF`
- `add_subdirectory(thirdparty/cgltf)`
- `stellar_import_gltf`
- glTF-gated test helpers/targets
- glTF compile definitions on client/config/tests
- glTF-specific validation command docs

Remove runtime/startup paths:

- `ApplicationConfig::asset_path` as glTF asset path
- animation-specific startup config unless used by a non-glTF system
- glTF-only error messages
- glTF scripted smoke tests

Remove or refactor unused asset systems if they were only for glTF:

- `SkinAsset`
- `AnimationAsset`
- `scene::SceneGraph`
- `scene::AnimationRuntime`
- skin palette policy and GPU skinning paths, if no BSP/sprite system uses them

If keeping a generic mesh/material/texture asset type, scrub glTF-specific comments and field names where practical.

---

## Active docs cleanup

Update active docs to remove glTF as an active feature:

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `AGENTS.md`
- `Plans/NEXT.md`
- active plan files under `Plans/`
- README or build docs if present

Allowed historical exception:

- `Plans/Archived/**` can retain old glTF references if they are not linked as active work and are clearly historical.

---

## Search targets

Run these searches and clean active hits:

```bash
rg -n -i "gltf|cgltf|STELLAR_ENABLE_GLTF" \
  --glob '!Plans/Archived/**' \
  --glob '!build*/**'

rg -n "SceneAsset|SkinAsset|AnimationAsset|AnimationRuntime|skin_joint|JOINTS_0|WEIGHTS_0" \
  include src tests CMakeLists.txt docs AGENTS.md Plans/NEXT.md
```

Expected after cleanup:

- No active glTF/cgltf/`STELLAR_ENABLE_GLTF` references.
- No active scene/skin/animation types unless intentionally retained for a non-glTF purpose and renamed/documented source-neutrally.

---

## Replace test names and fixtures

Replace glTF-specific integration tests with BSP-backed ones:

| Old | New |
| --- | --- |
| `gltf_importer_regression` | `bsp_importer_regression` |
| `playable_world_smoke` using generated glTF | `bsp_playable_world_smoke` |
| `scripted_playable_world_smoke` using generated glTF | `bsp_scripted_playable_world_smoke` |
| `scripted_collision_smoke` using generated glTF | `bsp_scripted_collision_smoke` |
| `scripted_object_collider_smoke` using generated glTF | `bsp_scripted_object_collider_smoke` |
| `client_cli_asset_validation` | `client_cli_map_validation` |

Names may vary, but the active test output should make it clear that BSP is the source.

---

## CMake target shape after cleanup

Expected high-level default targets:

- `stellar_assets`
- `stellar_physics`
- `stellar_world`
- `stellar_server_core`
- `stellar_scripting`
- `stellar_import_bsp`
- `stellar_platform`
- `stellar_client_runtime`
- `stellar_graphics`
- `stellar_client_config`
- `stellar-client`
- display-free tests
- optional OpenGL/Vulkan context tests

No `stellar_import_gltf`. No `cgltf`. No `STELLAR_ENABLE_GLTF`.

---

## Acceptance criteria

- Full default configure/build/test succeeds without glTF options.
- Active code has no glTF importer, dependency, feature gate, startup path, or tests.
- Active docs say BSP is canonical and glTF is removed.
- Historical archived plans are not treated as active.
- OpenGL/Vulkan backend selection still works.
- Lua scripting remains mandatory.

---

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

rg -n -i "gltf|cgltf|STELLAR_ENABLE_GLTF" \
  --glob '!Plans/Archived/**' \
  --glob '!build*/**'
```

The `rg` command should produce no active hits. If it produces historical hits outside `Plans/Archived`, either remove them or explicitly mark them as historical and move them under archive if appropriate.

---

## Completion note template

Add to `docs/ImplementationStatus.md`:

```markdown
Phase BSP-5 is complete as of YYYY-MM-DD:

- Removed active glTF importer functionality, cgltf dependency, glTF CMake option/gates, glTF fixtures, and glTF integration tests.
- Replaced active startup validation and integration smokes with BSP-backed paths.
- Cleaned active docs and agent guidance so BSP is the canonical level format and glTF is no longer an active feature.

Validation run:

<commands and result>

Active glTF/cgltf reference search:

<command and result>
```
