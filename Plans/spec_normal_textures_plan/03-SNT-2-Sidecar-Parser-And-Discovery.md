# SNT-2 — Material Sidecar Parser and Discovery

**Depends on:** SNT-1 contract decisions  
**Can run partly in parallel with:** SNT-4 tangent design  
**Must not commit:** yes  
**Purpose:** Add a deterministic, safe, display-free parser and discovery layer for BSP material sidecars.

---

## 1. Sidecar Discovery Rules

Input key: BSP texture name from `LevelSurfaceMaterial::source_name`, for example:

```text
dev/wall_96
stellar_dev_grid_64
__missing_texture
```

Normalize the BSP texture name for lookup:

- Convert backslashes to `/`.
- Trim leading/trailing whitespace.
- Lowercase for filesystem lookup unless the agent intentionally preserves case and tests it.
- Reject empty names.
- Reject path traversal: absolute paths, drive prefixes, `..`, empty path segments after normalization.
- Preserve subdirectories: `dev/wall_96` maps to `materials/dev/wall_96.stellar_material`.

Search order:

1. Explicit `LoadOptions::material_search_roots`, in order.
2. If loading from disk through `load_level(path)`, add `<parent-of-bsp>/materials` as the default search root.
3. For `load_level_from_memory*`, do not implicitly search the filesystem unless an explicit root is provided.

Recommended `LoadOptions` additions in `include/stellar/import/bsp/Loader.hpp`:

```cpp
bool parse_material_sidecars = true;
bool strict_material_sidecars = false;
std::vector<std::filesystem::path> material_search_roots;
```

If adding `std::filesystem` to the public header is undesirable, use `std::vector<std::string>`.

---

## 2. Parser Format

Use `.stellar_material` files.

Supported keys:

```text
version
name

base_color
normal
normal_scale
normal_light_strength

specular
specular_factor
specular_power

metallic_factor
roughness_factor
metallic_roughness
occlusion
occlusion_strength

emissive
emissive_factor

alpha_mode
alpha_cutoff
double_sided
unlit
```

Supported value types:

- String: quoted or unquoted single token.
- Bool: `true/false`, `1/0`, `yes/no`, `on/off`.
- Float: finite decimal.
- Vec3: three finite floats separated by spaces or commas.
- Enum:
  - `alpha_mode = opaque|mask|blend`

Comments:

- `# comment`
- `// comment`

Whitespace around `=` is optional.

Unknown keys:

- Warning by default.
- Error when `strict_material_sidecars = true`.

Malformed known values:

- Error for that sidecar; do not partially apply it.
- Import should continue if strict mode is false, preserving fallback material behavior and adding diagnostics.

---

## 3. New Internal Types

Recommended new files:

```text
src/import/bsp/MaterialSidecar.hpp
src/import/bsp/MaterialSidecar.cpp
src/import/bsp/MaterialSidecarResolver.hpp
src/import/bsp/MaterialSidecarResolver.cpp
```

Recommended internal representation:

```cpp
struct MaterialSidecarTextureRef {
    std::string relative_path;
};

struct MaterialSidecar {
    int version = 1;
    std::string name;

    std::optional<MaterialSidecarTextureRef> base_color;
    std::optional<MaterialSidecarTextureRef> normal;
    std::optional<MaterialSidecarTextureRef> specular;
    std::optional<MaterialSidecarTextureRef> metallic_roughness;
    std::optional<MaterialSidecarTextureRef> occlusion;
    std::optional<MaterialSidecarTextureRef> emissive;

    float normal_scale = 1.0F;
    float normal_light_strength = 0.0F;
    float specular_factor = 0.0F;
    float specular_power = 32.0F;
    float metallic_factor = 0.0F;
    float roughness_factor = 1.0F;
    float occlusion_strength = 1.0F;
    std::array<float, 3> emissive_factor{0.0F, 0.0F, 0.0F};

    AlphaMode alpha_mode = AlphaMode::kOpaque;
    float alpha_cutoff = 0.5F;
    std::optional<bool> double_sided;
    std::optional<bool> unlit;
};
```

Keep this internal to the BSP import layer unless a later branch generalizes material authoring.

---

## 4. Diagnostics

Extend import diagnostics if necessary.

Recommended diagnostic categories:

```text
kMaterialSidecarLoaded
kMaterialSidecarMissing       # info/debug only; do not spam normal imports
kMaterialSidecarInvalid
kMaterialSidecarUnknownKey
kMaterialSidecarUnsafePath
kMaterialSidecarTextureMissing
```

If adding enum values is too invasive, reuse existing warning/error channels but make messages stable and testable.

Messages should include:

- source BSP URI
- BSP texture name
- sidecar path when available
- line number and key for parser diagnostics
- whether the sidecar was ignored or applied

---

## 5. Parser Tests

Add display-free tests, preferably in a new test executable or in `tests/import/bsp/BspMaterials.cpp`.

Test cases:

1. Parses a complete valid sidecar.
2. Parses quoted and unquoted paths.
3. Rejects missing/unsupported version.
4. Rejects unsafe texture paths:
   - `/abs/path.png`
   - `../escape.png`
   - `C:\escape.png`
5. Unknown key is warning in non-strict mode.
6. Unknown key is error in strict mode.
7. Malformed float rejects the sidecar.
8. `dev/wall_96` resolves to `materials/dev/wall_96.stellar_material`.

---

## 6. Validation Commands

```bash
cmake --build build --target stellar_bsp_materials_test -j$(nproc)
ctest --test-dir build -R '^(bsp_materials)$' --output-on-failure
tools/dev/check_target_boundaries.sh
```

Phase-close:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

---

## 7. Acceptance Criteria

- Parser is deterministic and display-free.
- Discovery is path-safe.
- Missing sidecars preserve old behavior.
- Invalid sidecars report stable diagnostics and preserve fallback behavior unless strict mode chooses failure.
- No new parser dependency is required.
