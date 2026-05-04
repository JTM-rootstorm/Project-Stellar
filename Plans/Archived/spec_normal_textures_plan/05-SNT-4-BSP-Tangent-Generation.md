# SNT-4 — BSP Tangent Generation

**Depends on:** SNT-0  
**Can run in parallel with:** SNT-2/SNT-3 parser work if contracts are stable  
**Must not commit:** yes  
**Purpose:** Generate valid tangent-space data for BSP faces so normal maps can render correctly.

---

## 1. Current State

`StaticVertex` already has:

```cpp
std::array<float, 3> normal;
std::array<float, 2> uv0;
std::array<float, 4> tangent;
std::array<float, 2> uv1;
bool MeshPrimitive::has_tangents;
```

BSP import currently computes face normal and UVs from BSP texinfo, but does not set valid tangent data or `has_tangents=true`.

---

## 2. Tangent Source

Classic BSP texinfo provides face texture axes:

```text
s_vector = (sx, sy, sz, soffset)
t_vector = (tx, ty, tz, toffset)
```

Use these to compute tangent-space basis.

Recommended algorithm per face:

```cpp
Vec3 n = face_normal;
Vec3 s_axis = {info.s[0], info.s[1], info.s[2]};
Vec3 t_axis = {info.t[0], info.t[1], info.t[2]};

Vec3 tangent = normalize(s_axis - n * dot(s_axis, n));
Vec3 bitangent_source = normalize(t_axis - n * dot(t_axis, n));

float handedness = dot(cross(n, tangent), bitangent_source) < 0.0f ? -1.0f : 1.0f;
```

Set every vertex:

```cpp
vertex.tangent = {tangent.x, tangent.y, tangent.z, handedness};
primitive.has_tangents = true;
```

Reject/skip tangents when:

- `face.texinfo` is invalid.
- S axis length is near zero.
- Projected tangent length is near zero.
- face normal is degenerate.

Fallback:

- Keep `primitive.has_tangents = false`.
- Keep default tangent.
- Add a stable warning only if useful. Do not spam warnings for intentionally missing/degenerate faces already reported elsewhere.

---

## 3. Important Orientation Notes

- Preserve BSP-authored coordinates 1:1.
- Do not perform hidden axis conversion.
- Z-up is already the central Stellar world-axis contract.
- Tangent handedness must match the shader's bitangent reconstruction:

```glsl
vec3 bitangent = normalize(cross(normal, tangent) * tangent.w);
```

If the shader uses a different convention, update this phase's algorithm to match and test both together.

---

## 4. Files to Modify

```text
src/import/bsp/BspLevelBuilder.cpp
src/import/bsp/BspGeometryBuilder.hpp
src/import/bsp/BspGeometryBuilder.cpp
tests/import/bsp/BspMaterials.cpp
tests/import/bsp/BspImporter.cpp
```

Optional helper functions:

```cpp
float dot(Vec3 a, Vec3 b) noexcept;
float length(Vec3 v) noexcept;
Vec3 scale(Vec3 v, float s) noexcept;
Vec3 subtract_scaled(Vec3 v, Vec3 axis, float scale) noexcept;
std::optional<std::array<float,4>> tangent_for_texinfo(Vec3 normal, const Texinfo& info) noexcept;
```

---

## 5. Tests

Add display-free tests.

Minimum cases:

1. Axis-aligned floor/ceiling/wall face with valid texinfo produces finite normalized tangent.
2. `primitive.has_tangents == true` when texinfo is valid.
3. Tangent is orthogonal to normal within epsilon.
4. Handedness is either `+1` or `-1`.
5. Degenerate texinfo leaves `has_tangents == false`.
6. Existing UV0/UV1/lightmap tests still pass.

Suggested test names:

```text
bsp_materials_generates_tangents_from_texinfo
bsp_materials_skips_degenerate_tangents
bsp_lightmaps_preserve_uv1_after_tangent_generation
```

---

## 6. Validation Commands

```bash
cmake --build build --target stellar_bsp_materials_test stellar_bsp_importer_test stellar_bsp_lightmaps_test -j$(nproc)
ctest --test-dir build -R '^(bsp_materials|bsp_importer|bsp_lightmaps)$' --output-on-failure
```

Phase-close:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh
```

---

## 7. Acceptance Criteria

- BSP faces with valid texinfo generate valid tangent data.
- Normal maps can be enabled by renderers only when `has_tangents` is true.
- Existing UV/lightmap/collision geometry behavior is unchanged.
- No display/GPU dependency is introduced.
