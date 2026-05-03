# Project Stellar — Baked Lighting Policy and Global Level Light Implementation Plan

Target branch: `fixes`

## Purpose

Fix the current Full/VHLT lighting regression:

- A Full pipeline map with **no light entities** should render black, not fullbright.
- Maps with dim lights should remain dim.
- Level-wide lighting should remain an intentional feature, controlled by a specific authored global light entity or world metadata.
- Fast/no-bake iteration maps should remain visible/fullbright by design.

This plan is optimized for AI implementation agents. Do not commit unless explicitly instructed by the user.

---

## Current observed behavior

### User report

1. Full pipeline maps with no light present should be black, but currently appear fullbright.
2. Dim lights appear too bright and fill the test room too easily.
3. User suggests level-wide lighting should exist as an intended feature, using a specific global light entity or properties, but absence of that entity should not imply ambient/fullbright lighting.

### Repo evidence

Important files:

- `include/stellar/assets/LevelAsset.hpp`
- `include/stellar/assets/WorldMetadataAsset.hpp`
- `src/import/bsp/BspLevelBuilder.cpp`
- `src/graphics/RenderLevel.cpp`
- `src/graphics/opengl/OpenGLGraphicsDevice.cpp`
- `tests/import/bsp/BspLightmaps.cpp`
- `tests/graphics/RenderSceneInspection.cpp`
- `tools/bsp/compile_vhlt_bsp30.sh`
- `tools/trenchbroom/Stellar/stellar_entities.fgd`
- `tools/trenchbroom/Stellar/templates/lit_zup_room.map`

Current structural facts:

- `LevelSurfaceMaterial` has optional `texture_index` and optional `lightmap_index`, but no lighting policy field.
- `LevelGeometryAsset` stores imported `lightmaps` and `raw_lighting`, but no concept of whether baked lighting is required.
- `LevelAsset` has `geometry`, `level_collision`, `world_metadata`, and `visibility`, but no level-wide lighting settings.
- `WorldMetadataAsset` currently stores generic `WorldMarker` records and properties, but has no dedicated global lighting structure.
- `RenderLevel` creates BSP level materials as `unlit = true` and only attaches `lightmap_texture` when `surface_material.lightmap_index` exists.
- The OpenGL shader renders fullbright when `u_has_lightmap_texture == false && u_unlit == true`.
- `BspLevelBuilder` returns no lightmap for faces when the source BSP has no lighting or `face.light_offset < 0`.
- Existing tests cover lightmap import diagnostics, all-black lightmap warnings, and missing-lighting summaries, but not runtime “baked lighting required” behavior.

---

## Root cause

The engine currently has only an implicit rendering rule:

```text
if a material has a lightmap -> use it
else if material.unlit -> render fullbright
```

That is wrong for Full/VHLT baked-lighting maps.

The engine needs an explicit distinction between:

1. **Fullbright authoring/iteration mode**
   - Intended for Fast compile profile.
   - No lightmaps required.
   - Missing lightmaps render visible/fullbright.

2. **Baked lighting required mode**
   - Intended for Full VHLT/hlrad compile profile.
   - Missing lightmaps should render black.
   - No light entities and no global ambient/light entity should yield black output.
   - Dim lightmaps should remain dim.

3. **Baked lighting plus authored global level light**
   - Intended level-wide illumination.
   - Must be explicitly authored.
   - Uses RGB + intensity properties.
   - Absence means no ambient/global fill.

---

## Target behavior

### Fast compile profile

- Fast profile remains visible/fullbright for geometry iteration.
- Fast output can omit baked lighting and still render textures.
- Fast output should be marked or inferred as `fullbright`.

### Full Lighting compile profile

- Full Lighting profile is `baked-required`.
- If no lights and no global ambient entity exist:
  - surfaces should sample black lightmaps or black fallback lightmaps;
  - map should be black except UI/debug overlays.
- If dim lights exist:
  - lightmaps should be dim;
  - no hidden ambient floor or boost should brighten them.
- If a global ambient/light entity exists:
  - level-wide RGB/intensity is applied intentionally.
  - global lighting should be documented and visible in debug logs.

### Renderer defaults

- Baked lightmap defaults:
  - `lightmap_intensity = 1.0`
  - `lightmap_min = 0.0`
- Any nonzero ambient/minimum-light behavior must be explicit:
  - from a global light entity, or
  - debug-only env flag, or
  - documented material/level property.

---

## Proposed data model

### Option A: Add `LevelLightingAsset`

Recommended.

Add to `include/stellar/assets/LevelAsset.hpp`:

```cpp
enum class LevelLightingMode {
    kFullbright,
    kBakedRequired,
};

struct LevelGlobalLight {
    std::array<float, 3> color{0.0F, 0.0F, 0.0F};
    float intensity = 0.0F;
    bool enabled = false;
};

struct LevelLightingAsset {
    LevelLightingMode mode = LevelLightingMode::kFullbright;

    // Explicit authored ambient/global fill only. Absence means black.
    LevelGlobalLight global_ambient{};

    // Renderer controls for baked lightmaps.
    float lightmap_intensity = 1.0F;
    float lightmap_min = 0.0F;
};
```

Add to `LevelAsset`:

```cpp
LevelLightingAsset lighting;
```

### Option B: Use only `world_metadata`

Not recommended as the main runtime contract. It preserves source properties but requires renderer/importer code to repeatedly parse generic markers.

Use `world_metadata` for preserving authored entities, but import them into `LevelAsset::lighting` for renderer/runtime use.

---

## Authoring contract

### New global lighting entity

Add a specific editor entity:

```fgd
@PointClass color(255 255 200) size(-8 -8 -8, 8 8 8) = stellar_global_light : "Stellar global ambient light"
[
    origin(string) : "Origin; ignored for ambient lighting" : "0 0 0"
    _stellar_color(string) : "Global ambient RGB, 0-255" : "255 255 255"
    _stellar_intensity(string) : "Global ambient intensity, 0.0-1.0 recommended" : "0.0"
    _stellar_enabled(choices) : "Enabled" : 1 =
    [
        0 : "false"
        1 : "true"
    ]
]
```

Possible aliases:
- `light_global`
- `light_ambient`
- `stellar_global_light`

Recommended primary classname: `stellar_global_light`.

### Worldspawn optional keys

Also allow worldspawn keys for convenience/automation:

```map
"_stellar_lighting_mode" "baked"
"_stellar_global_light" "0 0 0 0"
```

Possible values:

```text
_stellar_lighting_mode:
  fullbright
  baked
  baked_required

_stellar_global_light:
  r g b intensity
  r g b
```

If both worldspawn keys and `stellar_global_light` exist:
- entity overrides worldspawn or vice versa, but choose one deterministic rule.
- Recommended: explicit `stellar_global_light` entity wins; worldspawn is fallback.

### Important semantic rule

No `stellar_global_light` entity and no worldspawn global-light key means:

```text
global ambient = 0 0 0, intensity = 0
```

No hidden ambient fill.

---

## Compile/tooling contract

### Full Lighting profile

The Full/VHLT wrapper should mark output as baked-required.

In `tools/bsp/compile_vhlt_bsp30.sh`, when `--profile full`:

1. Inject source map worldspawn key before VHLT compile:

```map
"_stellar_lighting_mode" "baked"
```

2. Do not inject any global light.
3. Preserve authored `stellar_global_light` entity if present.

For `--profile fast`:

```map
"_stellar_lighting_mode" "fullbright"
```

or omit and let importer default non-baked maps to fullbright.

Recommended explicit behavior:
- Fast: inject `_stellar_lighting_mode "fullbright"`.
- Full: inject `_stellar_lighting_mode "baked"`.

This removes ambiguity after compile.

### Generic compiler behavior

If `compile_trenchbroom_bsp30.sh` uses a non-VHLT compiler:
- `--profile fast` -> fullbright.
- `--profile full` without VHLT should either:
  - fail with a clear diagnostic if baked lighting is expected, or
  - mark as fullbright and warn that lighting was not baked.

Recommended:
- Full Lighting profile should force `--toolchain vhlt`.
- Generic full should not silently claim baked lighting.

---

## Importer implementation

### Phase 1 — Parse lighting mode

In `BspLevelBuilder.cpp`:

1. Read worldspawn:
   - `_stellar_lighting_mode`
   - `stellar.lighting_mode`
   - maybe `lighting_mode`
2. Normalize values:
   - `fullbright`, `fast`, `none` -> `LevelLightingMode::kFullbright`
   - `baked`, `baked_required`, `full` -> `LevelLightingMode::kBakedRequired`
3. Default:
   - If explicit key absent and `geometry.raw_lighting` is non-empty or any face has `light_offset >= 0`, choose `kBakedRequired`.
   - If explicit key absent and no lighting lump exists, choose `kFullbright`.
4. Add diagnostics:
   - lighting mode source: explicit/inferred
   - raw lighting bytes
   - face lightmap count
   - material lightmap count

### Phase 2 — Parse global light

Parse all entities for:
- `stellar_global_light`
- optional aliases: `light_global`, `light_ambient`

Properties:
- `_stellar_color` or `color`: `r g b`
- `_stellar_intensity` or `intensity`: float
- `_stellar_enabled`: bool

Also parse worldspawn fallback:
- `_stellar_global_light "r g b intensity"`
- `_stellar_global_color`
- `_stellar_global_intensity`

Normalization:
- RGB 0-255 -> 0.0-1.0.
- Intensity is clamped to a reasonable nonnegative range.
- Recommended clamp:
  - min 0.0
  - max 16.0 for safety, but document 0.0-1.0 for ambient.
- If disabled or intensity <= 0:
  - global ambient disabled.

Diagnostics:
- Found global light entity count.
- Chosen global ambient RGB/intensity.
- Warning if multiple enabled global lights exist; choose first or sum deterministically.
- Recommended: allow only one enabled global light and warn if more exist.

### Phase 3 — Preserve source metadata

Still preserve the global light entity in `world_metadata.markers` if useful, but do not rely on generic markers for renderer policy.

---

## RenderLevel implementation

### Phase 1 — Black fallback lightmap

In `RenderLevel::initialize()`:

1. Create and upload a shared 1x1 black linear lightmap texture when:
   - `level_.lighting.mode == kBakedRequired`
   - lightmaps are not disabled by debug env.
2. Use sampler:
   - linear or nearest both okay for 1x1
   - clamp-to-edge
3. Store handle, e.g. `black_lightmap_texture_`.

Pseudo:

```cpp
if (!disable_lightmaps_ && level_.lighting.mode == LevelLightingMode::kBakedRequired) {
    ImageAsset black;
    black.name = "baked_missing_black_lightmap";
    black.width = 1;
    black.height = 1;
    black.format = ImageFormat::kR8G8B8;
    black.pixels = {0, 0, 0};
    black_lightmap_texture_ = device_->create_texture({black, TextureColorSpace::kLinear});
}
```

### Phase 2 — Material policy

When building `MaterialUpload`:

```cpp
const bool baked_required = level_.lighting.mode == LevelLightingMode::kBakedRequired;

if (!disable_lightmaps_ && surface_material.lightmap_index.has_value()) {
    bind imported lightmap;
} else if (!disable_lightmaps_ && baked_required && black_lightmap_texture_) {
    bind black fallback lightmap;
}
```

Important:
- Do not leave baked-required surfaces as `u_unlit=true` with no lightmap.
- If black fallback is bound, shader will use lightmap path and output black unless global ambient/emissive is applied.

### Phase 3 — Global ambient application

Add `MaterialUpload` fields or extend `MaterialAsset` with:

```cpp
std::array<float, 3> global_light_color{0,0,0};
float global_light_intensity = 0;
```

Simpler first implementation:
- Use existing `emissive_factor` as global ambient contribution for level surfaces only.

But do not permanently abuse emissive if the material model matters. Preferred:
- Add explicit global ambient uniforms:
  - `u_global_light_color`
  - `u_global_light_intensity`

Shader logic:

```glsl
vec3 baked = vec3(0.0);
if (u_has_lightmap_texture) {
    baked = texture(u_lightmap_texture, uv_for_set(u_lightmap_texcoord_set)).rgb
            * u_lightmap_intensity;
}
vec3 ambient = u_global_light_color * u_global_light_intensity;
vec3 lighting = max(baked, vec3(0.0)) + ambient;
lighting = max(lighting, vec3(u_lightmap_min)); // but lightmap_min default must be 0
frag_color = vec4(color.rgb * lighting + emissive, color.a);
```

For unlit/fullbright fast mode:
- no lightmap path;
- `u_unlit=true` can remain.

For baked mode:
- bind imported or black lightmap;
- `u_unlit` can stay true internally, but shader should take `u_has_lightmap_texture` branch first.
- Better: set `u_unlit=false` for baked-required materials to avoid accidental fullbright if binding fails.

### Phase 4 — Remove default light boost

Ensure defaults are:

```cpp
upload.lightmap_multiplier = 1.0F;
upload.lightmap_min = 0.0F;
```

If `MaterialUpload` currently does not expose `lightmap_min`, add it.

In OpenGL:
- `glUniform1f(lightmap_intensity_loc, material->upload.lightmap_multiplier);`
- `glUniform1f(lightmap_min_loc, material->upload.lightmap_min);`

No hardcoded minimum ambient for baked maps.

Debug-only overrides:
- `STELLAR_LIGHTMAP_INTENSITY`
- `STELLAR_LIGHTMAP_MIN`
- `STELLAR_GLOBAL_LIGHT_INTENSITY`
Optional, but defaults must stay physically/predictably dark.

---

## Shader implementation

Current shader does:

```glsl
if (u_has_lightmap_texture) {
    vec3 lightmap = texture(u_lightmap_texture, uv).rgb;
    frag_color = vec4(color.rgb * max(lightmap * u_lightmap_intensity,
        vec3(u_lightmap_min)) + emissive, color.a);
} else if (u_unlit) {
    frag_color = vec4(color.rgb + emissive, color.a);
}
```

Required change:
- Keep `u_lightmap_min = 0` by default.
- Add explicit global ambient uniforms.
- Avoid hidden fill.

Target:

```glsl
if (u_has_lightmap_texture) {
    vec3 lightmap = texture(u_lightmap_texture,
        uv_for_set(u_lightmap_texcoord_set)).rgb;
    vec3 lighting = lightmap * u_lightmap_intensity;
    lighting += u_global_light_color * u_global_light_intensity;
    lighting = max(lighting, vec3(u_lightmap_min));
    frag_color = vec4(color.rgb * lighting + emissive, color.a);
} else if (u_unlit) {
    frag_color = vec4(color.rgb + emissive, color.a);
} else {
    ...
}
```

If global light should be additive independent of baked light:
- use additive ambient as above.

If global light should act as minimum:
- use `max(baked, global)`.
- Recommended for ambient/global: additive with low intensity, but design can adjust.

---

## FGD implementation

Update both FGD copies if present:
- `tools/trenchbroom/Stellar/stellar_entities.fgd`
- `tools/bsp/stellar_entities.fgd`

Add:

```fgd
@BaseClass = StellarGlobalLightKeys
[
    _stellar_color(string) : "Global ambient RGB, 0-255" : "255 255 255"
    _stellar_intensity(string) : "Global ambient intensity, 0.0 disables" : "0"
    _stellar_enabled(choices) : "Enabled" : 1 =
    [
        0 : "false"
        1 : "true"
    ]
]

@PointClass base(Targetname, StellarGlobalLightKeys) color(255 255 200) size(-8 -8 -8, 8 8 8) = stellar_global_light : "Stellar global ambient light"
[
    origin(string) : "Origin; ignored for ambient lighting" : "0 0 0"
]
```

Also add optional worldspawn keys:

```fgd
_stellar_lighting_mode(choices) : "Lighting mode" : "fullbright" =
[
    "fullbright" : "Fast/fullbright fallback"
    "baked" : "Baked lighting required"
]
_stellar_global_light(string) : "Global ambient RGB intensity: r g b intensity" : "0 0 0 0"
```

Be careful with TrenchBroom FGD syntax:
- `spawnflags(flags) =`, not `spawnflags(flags) : "..." =`.

---

## Tests to add

### Importer tests

Add tests to `tests/import/bsp/BspLightmaps.cpp` or new file.

1. `worldspawn_baked_mode_imports_lighting_policy`
   - entity lump has `_stellar_lighting_mode "baked"`.
   - assert `level.lighting.mode == kBakedRequired`.

2. `worldspawn_fullbright_mode_imports_lighting_policy`
   - `_stellar_lighting_mode "fullbright"`.
   - assert `kFullbright`.

3. `no_lighting_key_infers_fullbright_without_lighting_lump`
   - no lighting lump.
   - assert `kFullbright`.

4. `lighting_lump_infers_baked_required_when_no_key`
   - raw lighting bytes present.
   - assert `kBakedRequired`.

5. `global_light_entity_imports_rgb_intensity`
   - entity:
     ```map
     {
       "classname" "stellar_global_light"
       "_stellar_color" "255 128 64"
       "_stellar_intensity" "0.25"
     }
     ```
   - assert:
     - enabled true
     - color approx `{1.0, 0.50196, 0.25098}`
     - intensity `0.25`

6. `absence_of_global_light_defaults_zero`
   - assert color zero/intensity zero/enabled false.

### RenderLevel tests

Add tests to `tests/graphics/RenderSceneInspection.cpp` / upload tests.

1. `baked_required_missing_lightmap_binds_black_fallback`
   - create synthetic level:
     - lighting mode baked
     - one material with texture but no lightmap
   - initialize `RenderLevel` with recording device.
   - assert:
     - a black lightmap texture was uploaded;
     - material upload has `lightmap_texture`;
     - not fullbright-only.

2. `fullbright_mode_missing_lightmap_does_not_bind_black_fallback`
   - same level but lighting mode fullbright.
   - assert:
     - no black fallback lightmap;
     - material remains unlit/fullbright.

3. `baked_required_uses_zero_lightmap_min`
   - assert `MaterialUpload.lightmap_min == 0`.

4. `global_light_upload_sets_ambient_uniform_data`
   - if added to `MaterialUpload`, assert values are propagated.

### OpenGL/shader tests

If opt-in OpenGL context tests exist:
1. Render a textured quad with black lightmap in baked mode.
   - pixel should be black.
2. Render same quad with global ambient `{1,1,1} intensity 0.1`.
   - pixel should be dim, not fullbright.
3. Render with lightmap value `{8,8,8}`.
   - pixel should be dark.

Keep GPU tests opt-in; do not require them in default CI.

### Compile wrapper tests

Add shell test:
- `compile_vhlt_bsp30.sh --profile full` preserved work map contains:
  ```map
  "_stellar_lighting_mode" "baked"
  ```
- `--profile fast` contains:
  ```map
  "_stellar_lighting_mode" "fullbright"
  ```

---

## Manual validation maps

Create or update fixtures:

1. `full_no_light_zup.map`
   - sealed room
   - no `light`
   - no `stellar_global_light`
   - full profile should render black.

2. `full_dim_light_zup.map`
   - sealed room
   - one light with very low brightness.
   - should render dim, not fullbright.

3. `full_global_light_zup.map`
   - sealed room
   - no point light
   - one `stellar_global_light`
   - should render uniformly according to RGB/intensity.

4. `fast_no_light_zup.map`
   - no lights
   - fast profile should remain visible/fullbright.

---

## Manual test commands

Compile:

```bash
tools/bsp/compile_trenchbroom_bsp30.sh \
  --map tests/fixtures/trenchbroom/src/full_no_light_zup.map \
  --out maps/compiled/full_no_light_zup.bsp \
  --profile full \
  --toolchain vhlt
```

Validate:

```bash
STELLAR_DEBUG_LIGHTMAPS=1 \
STELLAR_DEBUG_RENDER=1 \
build/stellar-client --validate-map maps/compiled/full_no_light_zup.bsp
```

Run:

```bash
STELLAR_DEBUG_LIGHTMAPS=1 \
STELLAR_DEBUG_RENDER=1 \
build/stellar-client --renderer opengl \
  --map maps/compiled/full_no_light_zup.bsp
```

Expected:
- baked mode logged.
- global light disabled/zero.
- missing or black lightmaps logged.
- view black.

Dim light:

```bash
tools/bsp/compile_trenchbroom_bsp30.sh \
  --map tests/fixtures/trenchbroom/src/full_dim_light_zup.map \
  --out maps/compiled/full_dim_light_zup.bsp \
  --profile full \
  --toolchain vhlt

STELLAR_FORCE_LIGHTMAP_VISUALIZATION=1 \
build/stellar-client --renderer opengl \
  --map maps/compiled/full_dim_light_zup.bsp
```

Expected:
- visualization dim.
- normal rendering dim.

Global light:

```bash
tools/bsp/compile_trenchbroom_bsp30.sh \
  --map tests/fixtures/trenchbroom/src/full_global_light_zup.map \
  --out maps/compiled/full_global_light_zup.bsp \
  --profile full \
  --toolchain vhlt

build/stellar-client --renderer opengl \
  --map maps/compiled/full_global_light_zup.bsp
```

Expected:
- uniform global ambient per RGB/intensity.
- no hidden fullbright.

---

## Implementation order

1. Add `LevelLightingAsset` to `LevelAsset.hpp`.
2. Add parser helpers in `BspLevelBuilder.cpp` for:
   - lighting mode,
   - global light entity,
   - worldspawn fallback keys.
3. Add compile wrapper injection:
   - full -> `_stellar_lighting_mode "baked"`
   - fast -> `_stellar_lighting_mode "fullbright"`
4. Add FGD support for `stellar_global_light` and worldspawn lighting keys.
5. Add `RenderLevel` black fallback lightmap for `baked` missing lightmaps.
6. Add explicit global ambient propagation into material/shader uniforms.
7. Set default `lightmap_min = 0`, `lightmap_intensity = 1`.
8. Add importer/render tests.
9. Add fixture maps.
10. Run default CTest and manual fixture validation.

---

## Acceptance criteria

### Required automated acceptance

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Focused:

```bash
ctest --test-dir build -R 'bsp_lightmaps|render_level|render_scene|trenchbroom|fgd|client_map_validation' --output-on-failure
```

### Required manual acceptance

1. Full/no-light map renders black.
2. Fast/no-light map renders fullbright/visible.
3. Full/dim-light map renders dim.
4. Full/global-light map renders visible only according to authored global RGB/intensity.
5. Removing `stellar_global_light` from a Full map removes global ambient.
6. `STELLAR_FORCE_FULLBRIGHT=1` remains a debug override only.
7. `STELLAR_DISABLE_LIGHTMAPS=1` remains a debug override only.

---

## Do not do

- Do not reintroduce hidden ambient via `lightmap_min`.
- Do not make missing lightmaps in baked mode fullbright.
- Do not make Fast profile black by default.
- Do not use global lighting unless explicitly authored.
- Do not disable lightmaps permanently.
- Do not rely only on shader constants; make level lighting policy explicit in imported assets.
- Do not change TrenchBroom compilation profiles unless needed for lighting-mode injection tests.
