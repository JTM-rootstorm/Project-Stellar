# SNT-8 — Fixtures, Documentation, Final Validation, and Handoff

**Depends on:** SNT-0 through SNT-7  
**Must not commit:** yes  
**Purpose:** Finish the branch with clear fixtures, documentation, final validation, and a handoff summary.

---

## 1. Fixture Requirements

Add deterministic fixtures that prove the feature works without relying on GPU display tests.

Recommended files:

```text
tests/fixtures/materials/dev/wall_96.stellar_material
tests/fixtures/materials/dev/wall_96_normal.png
tests/fixtures/materials/dev/wall_96_spec.png
tests/fixtures/materials/dev/grid_64.stellar_material
tests/fixtures/materials/dev/grid_64_normal.png
tests/fixtures/materials/dev/grid_64_spec.png
```

If binary PNG fixtures are undesirable, generate them during tests using the same decoder-supported format or add a tiny checked-in file with clear provenance.

Example sidecar:

```ini
version = 1
name = "dev/wall_96"
normal = "wall_96_normal.png"
normal_scale = 1.0
normal_light_strength = 0.25
specular = "wall_96_spec.png"
specular_factor = 0.35
specular_power = 48.0
roughness_factor = 0.75
double_sided = true
unlit = false
```

Add negative fixtures:

```text
tests/fixtures/materials/invalid/unsafe_path.stellar_material
tests/fixtures/materials/invalid/malformed_float.stellar_material
tests/fixtures/materials/invalid/missing_texture.stellar_material
tests/fixtures/materials/invalid/unknown_key_strict.stellar_material
```

---

## 2. Documentation Updates

Update active docs only. Do not edit archived plans unless the user later asks for archival.

Recommended docs:

```text
docs/BspAuthoring.md
docs/Design.md
docs/ImplementationStatus.md
tools/trenchbroom/Stellar/README.md
```

Documentation must state:

- BSP files still provide geometry, face texture names, lightmaps, visibility, entities, and collision.
- Sidecars enrich material rendering using BSP texture names as keys.
- Sidecars are optional.
- Missing sidecars preserve existing BSP texture/lightmap behavior.
- Normal maps require valid tangents; BSP import generates tangents from texinfo where possible.
- Specular lighting is lightweight presentation shading, not full PBR.
- Full PBR remains deferred.
- Server-authoritative gameplay and networking do not depend on material sidecars.

Include a short sidecar reference:

```ini
version = 1
name = "dev/wall_96"
base_color = "wall_96_albedo.png"
normal = "wall_96_normal.png"
specular = "wall_96_spec.png"
normal_scale = 1.0
normal_light_strength = 0.25
specular_factor = 0.35
specular_power = 48.0
roughness_factor = 0.75
alpha_mode = opaque
double_sided = true
unlit = false
```

---

## 3. Final Audit Commands

Run these before handoff:

```bash
git status --short

git grep -n 'full PBR\|physically based\|IBL\|environment map\|reflection probe' -- docs include src tests tools ':!Plans/Archived/**'

git grep -n 'specular_texture\|normal_light_strength\|specular_factor\|specular_power\|normal_scale' -- include src tests docs

git grep -n 'MaterialSidecar\|stellar_material\|material sidecar' -- include src tests docs tools

git grep -n 'stellar/server/\|stellar/scripting/' -- include/stellar/graphics src/graphics include/stellar/client src/client ':!src/client/SinglePlayerRuntime.cpp' ':!src/client/ListenHostRuntime.cpp'
```

Interpretation:

- `full PBR` should appear only as a non-goal/deferred wording.
- New material terms should appear only in assets/import/graphics/tests/docs.
- Graphics must not include server/scripting dependencies.
- Client networking/protocol must not include material sidecar concerns.

---

## 4. Final Validation Runbook

Run the complete default validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh
```

Run focused material validation:

```bash
ctest --test-dir build -R '^(bsp_materials|bsp_lightmaps|bsp_importer|render_level_upload|render_level_inspection|graphics_backend_selection|target_boundary)$' --output-on-failure
```

Run TrenchBroom/BSP regression filters:

```bash
ctest --test-dir build -R '^(trenchbroom|bsp_|client_map_validation|client_cli|server_cli|render_level|world_axes|collision_world|runtime_world|server_world_session|scripted_world_session)' --output-on-failure
```

Optional OpenGL smoke:

```bash
cmake -S . -B build-opengl -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_OPENGL_CONTEXT_TESTS=ON
cmake --build build-opengl -j$(nproc)
ctest --test-dir build-opengl -R 'opengl_context_smoke|render_level' --output-on-failure
```

Optional Vulkan smoke:

```bash
cmake -S . -B build-vulkan -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON
cmake --build build-vulkan -j$(nproc)
ctest --test-dir build-vulkan -R 'vulkan_context_smoke|render_level' --output-on-failure
```

If optional GPU smoke skips due to missing display/driver, document the skip and keep default validation as the pass/fail gate.

---

## 5. Handoff Summary Template

At branch completion, produce a local handoff note like this:

```markdown
# Spec/Normal Texture Branch Handoff

Branch: spec-normal-textures
Commit: <local HEAD SHA>

## Implemented

- Material sidecar parser/discovery for `.stellar_material`.
- BSP texture-name material key lookup.
- Sidecar image loading into `LevelAsset`.
- BSP tangent generation from texinfo.
- RenderLevel sidecar material upload.
- OpenGL normal/specular lighting.
- Vulkan normal/specular parity.
- Tests and docs.

## Validation

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`: pass/fail
- `cmake --build build -j$(nproc)`: pass/fail
- `ctest --test-dir build --output-on-failure`: pass/fail
- `tools/dev/check_target_boundaries.sh`: pass/fail
- Optional OpenGL smoke: pass/fail/skip
- Optional Vulkan smoke: pass/fail/skip

## Notes

- Full PBR remains deferred.
- Material sidecars are presentation-only.
- Missing sidecars preserve previous BSP material behavior.
```

Do not commit this handoff unless the user asks.

---

## 6. Acceptance Criteria

- Positive and negative material sidecar fixtures exist.
- Docs explain authoring and limitations.
- Final default validation passes.
- Target boundaries pass.
- No repository commit is made by the agent.
