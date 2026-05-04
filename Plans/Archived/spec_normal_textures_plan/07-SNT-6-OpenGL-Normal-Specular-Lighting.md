# SNT-6 — OpenGL Normal and Lightweight Specular Lighting

**Depends on:** SNT-5  
**Must not commit:** yes  
**Purpose:** Implement the OpenGL backend path for BSP normal maps and lightweight specular highlights.

---

## 1. Current State

`src/graphics/opengl/OpenGLGraphicsDevice.cpp` already has:

- vertex attributes for normal, UV0, tangent, UV1, and color
- base color texture sampling
- lightmap texture sampling
- normal texture sampling when `primitive.has_tangents` is true
- metallic-roughness, occlusion, and emissive support in shader logic
- a lightweight non-PBR directional lighting fallback when no lightmap is present

This phase extends that path with sidecar-controlled normal/specular behavior.

---

## 2. Required Shader Changes

### 2.1 Vertex Shader

Add world position output if camera-aware specular is used:

```glsl
out vec3 v_world_position;
```

Set:

```glsl
vec4 world_position = u_model * vec4(a_position, 1.0);
v_world_position = world_position.xyz;
```

Ensure current `gl_Position` behavior is unchanged.

### 2.2 Fragment Shader

Add:

```glsl
uniform sampler2D u_specular_texture;
uniform bool u_has_specular_texture;
uniform int u_specular_texcoord_set;

uniform float u_normal_scale;
uniform float u_normal_light_strength;
uniform float u_specular_factor;
uniform float u_specular_power;

uniform vec3 u_camera_world_position;
uniform bool u_has_camera_world_position;
uniform vec3 u_key_light_direction;
```

Material logic:

1. Compute base color exactly as before.
2. Compute normal:
   - if `u_has_normal_texture`, `primitive.has_tangents`, and tangent basis is valid, sample normal map.
   - scale XY by `u_normal_scale`.
3. Compute baked lighting:
   - preserve current lightmap behavior.
   - preserve global ambient and `lightmap_min`.
4. Add lightweight normal visibility term:
   - only when `u_normal_light_strength > 0`.
   - use fixed normalized `u_key_light_direction`.
   - this is a presentation-only detail term, not a dynamic gameplay light.
5. Add specular:
   - use normal map if present.
   - if camera position is available, compute view vector from world position.
   - otherwise use a stable fallback view vector.
   - use Blinn-Phong or Phong; pick one and document it.
   - multiply by `u_specular_factor`.
   - multiply by sampled specular map channel, use `.r` for grayscale.
   - attenuate with roughness if desired:
     - `effective_power = mix(u_specular_power, 4.0, clamp(roughness, 0.0, 1.0))`
6. Add emissive as before.
7. Preserve alpha mask/blend behavior.

Recommended formula:

```glsl
vec3 light_dir = normalize(u_key_light_direction);
float detail_diffuse = max(dot(normal, light_dir), 0.0) * u_normal_light_strength;

float spec_map = u_has_specular_texture
    ? texture(u_specular_texture, transformed_uv_for_slot(u_specular_texcoord_set, SPEC_SLOT)).r
    : 1.0;

vec3 view_dir = u_has_camera_world_position
    ? normalize(u_camera_world_position - v_world_position)
    : normalize(vec3(0.0, -1.0, 0.25));

vec3 half_dir = normalize(light_dir + view_dir);
float specular = pow(max(dot(normal, half_dir), 0.0), max(u_specular_power, 1.0))
               * u_specular_factor
               * spec_map;
```

Do not call this physically based rendering.

---

## 3. OpenGL C++ Binding Changes

In `OpenGLGraphicsDevice::draw_mesh`:

- Add binding for `material->upload.specular_texture`.
- Assign a new texture unit, for example `GL_TEXTURE6`.
- Update texture transform arrays if slot count changes.
- Set all new uniforms.
- Set `u_has_normal_texture=false` unless:
  - material has normal texture
  - primitive has tangents
  - texture binding succeeds
- Set `u_has_specular_texture` based on texture binding success.
- Set factors from `material->upload.material`.

Important: cache uniform locations if practical. Existing code repeatedly calls `glGetUniformLocation`; do not make that worse if refactoring is easy, but do not turn this phase into a broad OpenGL cleanup.

---

## 4. CPU/Backend Defaults

OpenGL fallback defaults:

```text
normal_scale = 1.0
normal_light_strength = 0.0
specular_factor = 0.0
specular_power = 32.0
key_light_direction = normalize(0.35, 0.8, 0.45)
specular map fallback = white if factor > 0 and no texture, otherwise irrelevant
```

Specular should be invisible by default because `specular_factor` defaults to `0.0`.

---

## 5. Tests

Default tests should remain display-free.

Add tests that inspect material upload and generated shader input decisions through fake devices where possible.

Optional OpenGL context smoke:

```bash
cmake -S . -B build-opengl -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_OPENGL_CONTEXT_TESTS=ON
cmake --build build-opengl --target stellar_opengl_context_smoke_test -j$(nproc)
ctest --test-dir build-opengl -R 'opengl_context_smoke' --output-on-failure
```

If no display is available, the context smoke may skip with the existing skip behavior.

---

## 6. Validation Commands

Default phase validation:

```bash
cmake --build build --target stellar_render_level_upload_test stellar_render_level_inspection_test stellar_graphics_backend_selection_test -j$(nproc)
ctest --test-dir build -R '^(render_level_upload|render_level_inspection|graphics_backend_selection)$' --output-on-failure
tools/dev/check_target_boundaries.sh
```

Phase-close:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Optional display/GPU smoke as above.

---

## 7. Acceptance Criteria

- OpenGL compiles.
- Default CTest passes without requiring OpenGL context tests.
- Normal map sampling remains gated by `primitive.has_tangents`.
- Specular contribution is disabled by default.
- Sidecar values visibly affect OpenGL path when enabled.
- Lightmap path is preserved.
