# Phase 5 — Remove retired model importer Functionality and Cleanup Active References

Branch target: `bsp-integration`  
Primary agent: `@carmack` for build/core cleanup  
Renderer cleanup: `@miyamoto` through `@director`  
Coordinator: `@director`  
Dependencies: Phases 1–4 complete enough that BSP-backed runtime/render/tests compile  
Parallelizable: inventory can be parallel; deletion and CMake edits should be serial.

---

## Goal

Remove retired model importer functionality from active code, build configuration, tests, active docs, and active agent-routing guidance.

This is not an optional-disable phase. retired model importer should not remain as a feature gate, dependency, importer, startup path, or canonical authoring path.

---

## Delete or retire active retired model importer code

Remove active retired model importer importer code:

- `src/import/retired model importer/`
- `include/stellar/import/retired model importer/` if present
- retired model importer loader/importer tests under `tests/import/retired model importer/`
- retired model importer fixtures under `tests/fixtures/retired model importer/`
- `thirdparty/retired parser dependency/`
- `.kilo/plans` or active plan references to retired model importer only if still active; historical archived plans may remain.

Remove build configuration:

- `retired importer feature gate`
- `add_subdirectory(thirdparty/retired parser dependency)`
- `stellar_import_retired model importer`
- retired model importer-gated test helpers/targets
- retired model importer compile definitions on client/config/tests
- retired model importer-specific validation command docs

Remove runtime/startup paths:

- `ApplicationConfig::asset_path` as retired model importer asset path
- animation-specific startup config unless used by a non-retired model importer system
- retired model importer-only error messages
- retired model importer scripted smoke tests

Remove or refactor unused asset systems if they were only for retired model importer:

- `retired skin asset`
- `retired animation asset`
- `scene::SceneGraph`
- `scene::retired animation runtime`
- skin palette policy and GPU skinning paths, if no BSP/sprite system uses them

If keeping a generic mesh/material/texture asset type, scrub retired model importer-specific comments and field names where practical.

---

## Active docs cleanup

Update active docs to remove retired model importer as an active feature:

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `AGENTS.md`
- `Plans/NEXT.md`
- active plan files under `Plans/`
- README or build docs if present

Allowed historical exception:

- `Plans/Archived/**` can retain old retired model importer references if they are not linked as active work and are clearly historical.

---

## Search targets

Run these searches and clean active hits:

```bash
rg -n -i "retired model importer|retired parser dependency|retired importer feature gate" \
  --glob '!Plans/Archived/**' \
  --glob '!build*/**'

rg -n "retired scene asset|retired skin asset|retired animation asset|retired animation runtime|retired_joint|retired joint attribute|retired weight attribute" \
  include src tests CMakeLists.txt docs AGENTS.md Plans/NEXT.md
```

Expected after cleanup:

- No active retired model importer/retired parser dependency/`retired importer feature gate` references.
- No active scene/skin/animation types unless intentionally retained for a non-retired model importer purpose and renamed/documented source-neutrally.

---

## Replace test names and fixtures

Replace retired model importer-specific integration tests with BSP-backed ones:

| Old | New |
| --- | --- |
| `retired model importer_importer_regression` | `bsp_importer_regression` |
| `playable_world_smoke` using generated retired model importer | `bsp_playable_world_smoke` |
| `scripted_playable_world_smoke` using generated retired model importer | `bsp_scripted_playable_world_smoke` |
| `scripted_collision_smoke` using generated retired model importer | `bsp_scripted_collision_smoke` |
| `scripted_object_collider_smoke` using generated retired model importer | `bsp_scripted_object_collider_smoke` |
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

No `stellar_import_retired model importer`. No `retired parser dependency`. No `retired importer feature gate`.

---

## Acceptance criteria

- Full default configure/build/test succeeds without retired model importer options.
- Active code has no retired model importer importer, dependency, feature gate, startup path, or tests.
- Active docs say BSP is canonical and retired model importer is removed.
- Historical archived plans are not treated as active.
- OpenGL/Vulkan backend selection still works.
- Lua scripting remains mandatory.

---

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

rg -n -i "retired model importer|retired parser dependency|retired importer feature gate" \
  --glob '!Plans/Archived/**' \
  --glob '!build*/**'
```

The `rg` command should produce no active hits. If it produces historical hits outside `Plans/Archived`, either remove them or explicitly mark them as historical and move them under archive if appropriate.

---

## Completion note template

Add to `docs/ImplementationStatus.md`:

```markdown
Phase BSP-5 is complete as of YYYY-MM-DD:

- Removed active retired model importer importer functionality, retired parser dependency dependency, retired model importer CMake option/gates, retired model importer fixtures, and retired model importer integration tests.
- Replaced active startup validation and integration smokes with BSP-backed paths.
- Cleaned active docs and agent guidance so BSP is the canonical level format and retired model importer is no longer an active feature.

Validation run:

<commands and result>

Active retired model importer/retired parser dependency reference search:

<command and result>
```
