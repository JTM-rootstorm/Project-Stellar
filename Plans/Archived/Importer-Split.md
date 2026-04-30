# Draft Plan: glTF Importer.cpp Decomposition

## Goal

Refactor `src/import/gltf/Importer.cpp` into smaller, focused translation units while preserving existing importer behavior, public APIs, tests, and build defaults.

The current importer owns too many responsibilities in one file: cgltf entrypoint/lifetime management, image loading, data URI decoding, material/texture/sampler conversion, mesh attribute decoding, tangent generation, skin loading, animation loading, node/scene assembly, and final scene post-processing. The refactor should make these concerns easier to maintain without changing externally visible functionality.

## Non-Goals

- Do not change the public importer API:
  - keep `stellar::import::gltf::Importer`
  - keep `create_importer()`
  - keep `load_scene(std::string_view path)`
- Do not change asset structs or renderer-facing data.
- Do not change glTF feature support in this phase.
- Do not introduce GPU/backend dependencies into the importer.
- Do not require `STELLAR_ENABLE_GLTF` to be enabled by default.
- Do not replace `cgltf` or `stb_image`.
- Do not make large behavioral “cleanup” changes while moving code.
- Do not combine this structural refactor with importer feature work.

## Current Context

The public importer API is intentionally small: `Importer::load_file()` returns a CPU-side `SceneAsset`, and `create_importer()` constructs the implementation.

`load_scene()` is a thin wrapper that creates the importer and delegates to `load_file()`, so the refactor should keep this external call path unchanged.

The build only compiles the glTF importer when `STELLAR_ENABLE_GLTF` is enabled, and currently includes `src/import/gltf/Importer.cpp` and `src/import/gltf/Loader.cpp` in `stellar_import_gltf`.

`Importer.cpp` currently embeds both `STB_IMAGE_IMPLEMENTATION` and `CGLTF_IMPLEMENTATION`, defines `CgltfImporter`, owns cgltf cleanup via `CgltfDataDeleter`, handles data URI/base64/percent decoding, image decode, materials, textures, samplers, mesh attributes, tangent generation, skins, animations, nodes, and scene assembly.

The importer regression test already covers materials, texture slots, sampler state, embedded/external images, data URIs, bounds, tangents, scene graph basics, and import validation. Keep this test green throughout the refactor.

---

# Mandatory Refactor Discipline: One-Move-at-a-Time Rule

Codex must apply this refactor as a sequence of small, independently buildable moves.

After each responsibility group is moved to a new file, Codex must:

1. Build the project with `STELLAR_ENABLE_GLTF=ON`.
2. Run `gltf_importer_regression`.
3. Fix any compile/test failures before moving the next responsibility group.
4. Avoid unrelated edits while resolving failures.

Do not split the entire importer in one large edit. This file is behavior-heavy, and the regression suite is most useful when each move has a small blame surface.

Required checkpoint sequence:

1. Add private declarations/header, no behavior moved yet.
2. Move cgltf/stb implementation ownership.
3. Move pure utilities and data URI decoding.
4. Move image import.
5. Move materials, samplers, and textures.
6. Move mesh import.
7. Move skins, animations, and nodes.
8. Move scene assembly.
9. Final cleanup and full test pass.

Each checkpoint should leave the repository compiling.

---

# Proposed File Layout

Create a private implementation area under:

```text
src/import/gltf/
```

Suggested files:

```text
src/import/gltf/Importer.cpp
src/import/gltf/ImporterPrivate.hpp
src/import/gltf/CgltfUtils.cpp
src/import/gltf/DataUri.cpp
src/import/gltf/ImageImport.cpp
src/import/gltf/MaterialImport.cpp
src/import/gltf/TextureImport.cpp
src/import/gltf/MeshImport.cpp
src/import/gltf/SkinImport.cpp
src/import/gltf/AnimationImport.cpp
src/import/gltf/NodeImport.cpp
src/import/gltf/SceneImport.cpp
```

The exact names can be adjusted, but keep the split by responsibility rather than by arbitrary line count.

## File Responsibilities

### `Importer.cpp`

Keep this file small.

Responsibilities:

- include public `stellar/import/gltf/Importer.hpp`
- define `CgltfImporter final : public Importer`
- implement `CgltfImporter::load_file()`
- implement `create_importer()`
- keep only high-level delegation to `load_scene_from_file()`

Do not keep mesh/material/image helper code here after the split.

### `ImporterPrivate.hpp`

Private shared declarations only.

Responsibilities:

- include `cgltf.h`
- include engine asset/platform headers needed by importer helpers
- declare private helper functions shared across translation units
- define `CgltfDataDeleter`
- define `using CgltfDataPtr = std::unique_ptr<cgltf_data, CgltfDataDeleter>`
- declare common utilities:
  - `duplicate_to_string`
  - `resolve_relative_path`
  - `checked_index`
  - `import_error`
  - `validate_required_extensions`
  - accessor validation helpers
  - accessor read helpers
  - `identity_matrix`

Important:

- Do **not** put `CGLTF_IMPLEMENTATION` in this header.
- Do **not** put `STB_IMAGE_IMPLEMENTATION` in this header.
- Avoid non-inline function definitions in this header unless they are templates or tiny `constexpr` values.
- Keep the header private to `src/import/gltf`; do not install or expose it as a public include.

### `CgltfUtils.cpp`

Responsibilities:

- own `#define CGLTF_IMPLEMENTATION`
- include `<cgltf.h>` exactly once with implementation enabled
- implement cgltf-independent and cgltf-adjacent utility helpers:
  - string duplication
  - path resolution
  - index checking
  - required extension validation
  - accessor validation
  - vector/matrix read helpers
  - identity matrix
  - node matrix helper if shared

This prevents accidental multiple definition issues from `CGLTF_IMPLEMENTATION`.

### `DataUri.cpp`

Responsibilities:

- base64 decode table
- base64 decoder
- percent decoder
- private helper:
  - `decode_data_uri(std::string_view uri)`

Keep this file independent of `cgltf` if practical.

### `ImageImport.cpp`

Responsibilities:

- own `#define STB_IMAGE_IMPLEMENTATION`
- include `<stb/stb_image.h>` exactly once with implementation enabled
- implement:
  - `load_image_from_file`
  - `load_image_from_memory`
  - image import from URI
  - image import from data URI
  - image import from buffer view
- use `decode_data_uri()` from `DataUri.cpp`

This prevents accidental multiple definition issues from `STB_IMAGE_IMPLEMENTATION`.

### `MaterialImport.cpp`

Responsibilities:

- implement material conversion:
  - base color factor
  - metallic/roughness
  - normal texture scale
  - occlusion strength
  - emissive factor
  - alpha mode
  - alpha cutoff
  - double-sided
- implement helper for material texture slot conversion
- avoid image loading here

### `TextureImport.cpp`

Responsibilities:

- implement sampler conversion:
  - `cgltf_filter_type` to `TextureFilter`
  - `cgltf_wrap_mode` to `TextureWrapMode`
- implement texture conversion:
  - texture name
  - image index
  - sampler index
- implement texture color-space classification:
  - base color and emissive textures mark sRGB
  - normal, metallic-roughness, and occlusion remain linear unless reused by color usage

This keeps the current color-space behavior together with texture metadata.

### `MeshImport.cpp`

Responsibilities:

- mesh primitive import
- attribute lookup
- index loading
- POSITION/NORMAL/TEXCOORD/TANGENT/COLOR/JOINTS/WEIGHTS decoding
- bounds extraction/fallback
- tangent generation
- `has_tangents`
- `has_colors`
- `has_skinning`
- material index mapping

Suggested internal helper:

```cpp
struct PrimitiveAccessors {
    const cgltf_accessor* positions = nullptr;
    const cgltf_accessor* normals = nullptr;
    const cgltf_accessor* uv0 = nullptr;
    const cgltf_accessor* uv1 = nullptr;
    const cgltf_accessor* tangents = nullptr;
    const cgltf_accessor* colors = nullptr;
    const cgltf_accessor* joints0 = nullptr;
    const cgltf_accessor* weights0 = nullptr;
    const cgltf_accessor* indices = nullptr;
};
```

This will reduce long local scopes and make validation clearer.

### `SkinImport.cpp`

Responsibilities:

- skin name
- joint index resolution
- inverse bind matrices
- default identity inverse bind matrices
- validation of inverse-bind matrix count

### `AnimationImport.cpp`

Responsibilities:

- animation sampler loading
- input/output accessor decoding
- interpolation mapping
- channel target node and target path mapping
- validation of supported animation paths/types

### `NodeImport.cpp`

Responsibilities:

- node name
- transform matrix/TRS extraction
- mesh instance extraction
- child index extraction
- skin index mapping
- camera/light fields only if already supported

### `SceneImport.cpp`

Responsibilities:

- parse glTF file
- call `cgltf_parse_file`
- call `cgltf_load_buffers`
- call `cgltf_validate`
- validate required extensions
- load all asset arrays in dependency order:
  1. images
  2. samplers
  3. textures
  4. materials
  5. meshes
  6. skins
  7. animations
  8. nodes
  9. scenes
- assign default scene
- call texture color-space classification
- return `SceneAsset`

---

# Refactor Strategy

## Step 1: Create Private Header Without Moving Logic

Create `src/import/gltf/ImporterPrivate.hpp`.

Move only declarations and shared type declarations into it.

Keep all function definitions in `Importer.cpp` temporarily.

Compile after this step.

Expected result:

- no behavior change
- no test changes
- `Importer.cpp` still large but now has a stable private interface for later moves

## Step 2: Move cgltf/stb Implementation Macros Safely

Move:

```cpp
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
```

into `CgltfUtils.cpp` or a dedicated `CgltfImplementation.cpp`.

Move:

```cpp
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
```

into `ImageImport.cpp` or a dedicated `StbImageImplementation.cpp`.

Important:

- `cgltf.h` can still be included normally elsewhere, but `CGLTF_IMPLEMENTATION` must appear in exactly one translation unit.
- `stb_image.h` can still be included normally elsewhere, but `STB_IMAGE_IMPLEMENTATION` must appear in exactly one translation unit.

Compile immediately after this step to catch linkage issues.

## Step 3: Move Pure Utilities First

Move low-risk helpers to `CgltfUtils.cpp` and `DataUri.cpp`:

- `duplicate_to_string`
- `resolve_relative_path`
- `checked_index`
- `import_error`
- accessor validators
- read helpers
- `identity_matrix`
- base64 decode
- percent decode
- `decode_data_uri`

Keep signatures unchanged.

Compile and run importer regression test before continuing.

## Step 4: Move Image Import

Move image-loading helpers into `ImageImport.cpp`:

- file image load
- memory image load
- data URI image load
- buffer-view image load, if currently embedded in scene assembly

Preserve exact error strings where practical, especially for regression-tested failures.

Compile and run importer regression test before continuing.

## Step 5: Move Material, Sampler, Texture Import

Move:

- `load_material`
- `load_sampler`
- `load_texture`
- texture filter/wrap mapping
- texture color-space classification

into `MaterialImport.cpp` and `TextureImport.cpp`.

Do not change color-space classification behavior.

Compile and run importer regression test before continuing.

## Step 6: Move Mesh Import

Move mesh-related code into `MeshImport.cpp`.

Break long primitive import logic into helpers:

```cpp
find_primitive_accessors(...)
validate_primitive_accessors(...)
read_indices(...)
read_vertices(...)
apply_bounds(...)
generate_missing_tangents_if_needed(...)
```

Behavior-preserving requirements:

- Preserve support for missing optional attributes.
- Preserve default normals/UVs/colors/tangents behavior.
- Preserve generated tangent behavior for normal-mapped primitives where possible.
- Preserve JOINTS_0/WEIGHTS_0 normalization and validation.
- Preserve index validation and primitive topology validation.
- Preserve material index mapping.
- Preserve bounds import/fallback logic.

Compile and run importer regression test before continuing.

## Step 7: Move Skin, Animation, Node Import

Move:

- `load_skin` to `SkinImport.cpp`
- `load_animation` to `AnimationImport.cpp`
- `load_node` to `NodeImport.cpp`

Keep helper functions private to each file unless shared by multiple domains.

Compile and run importer regression test before continuing.

## Step 8: Move Scene Assembly Last

Move final `load_scene_from_file()` orchestration into `SceneImport.cpp`.

At this point `Importer.cpp` should only construct and delegate.

Compile and run importer regression test before continuing.

## Step 9: Final Cleanup

After all moves are complete:

- remove unused includes from each new file
- keep include order consistent with nearby files
- ensure no helper remains declared but unused
- ensure no duplicate private helpers exist unless intentionally file-local
- make functions file-local where possible
- keep shared declarations only in `ImporterPrivate.hpp`

Run the full test plan.

---

# CMake Changes

Update the `stellar_import_gltf` target inside the existing `if(STELLAR_ENABLE_GLTF)` block.

Current source list includes only:

```cmake
src/import/gltf/Importer.cpp
src/import/gltf/Loader.cpp
```

Replace with:

```cmake
src/import/gltf/Importer.cpp
src/import/gltf/Loader.cpp
src/import/gltf/CgltfUtils.cpp
src/import/gltf/DataUri.cpp
src/import/gltf/ImageImport.cpp
src/import/gltf/MaterialImport.cpp
src/import/gltf/TextureImport.cpp
src/import/gltf/MeshImport.cpp
src/import/gltf/SkinImport.cpp
src/import/gltf/AnimationImport.cpp
src/import/gltf/NodeImport.cpp
src/import/gltf/SceneImport.cpp
```

Keep the existing include paths and link libraries unchanged unless compilation requires a private include path for `src/import/gltf`.

---

# Testing Plan

## Required Build

```bash
cmake -S . -B build -DSTELLAR_ENABLE_GLTF=ON
cmake --build build -j$(nproc)
```

## Required Per-Move Test

Run after every checkpoint in the one-move-at-a-time sequence:

```bash
ctest --test-dir build -R '^gltf_importer_regression$' --output-on-failure
```

## Required Final Tests

```bash
ctest --test-dir build --output-on-failure
```

At minimum, explicitly run:

```bash
ctest --test-dir build -R '^gltf_importer_regression$' --output-on-failure
ctest --test-dir build -R '^client_asset_validation_smoke$' --output-on-failure
ctest --test-dir build -R '^client_cli_asset_validation$' --output-on-failure
ctest --test-dir build -R '^render_scene_inspection$' --output-on-failure
```

## Optional Safety Check

Build without glTF enabled to ensure default configuration still works:

```bash
cmake -S . -B build-default
cmake --build build-default -j$(nproc)
ctest --test-dir build-default --output-on-failure
```

---

# Acceptance Criteria

- `src/import/gltf/Importer.cpp` becomes a small entrypoint/adapter file.
- No public headers change unless absolutely necessary.
- `Loader.cpp` behavior remains unchanged.
- `create_importer()` behavior remains unchanged.
- `load_scene()` behavior remains unchanged.
- `STB_IMAGE_IMPLEMENTATION` appears in exactly one `.cpp`.
- `CGLTF_IMPLEMENTATION` appears in exactly one `.cpp`.
- The one-move-at-a-time sequence was followed and each checkpoint compiled before continuing.
- The importer regression test passes with `STELLAR_ENABLE_GLTF=ON`.
- Default non-glTF build remains unaffected.
- No feature support is removed:
  - `.gltf`
  - `.glb`
  - external buffers/images
  - data URI buffers/images
  - buffer-view images
  - materials
  - samplers
  - textures
  - color-space classification
  - tangents and tangent generation
  - vertex colors
  - skin data
  - animation data
  - node/scene hierarchy
- Error handling remains `std::expected<..., stellar::platform::Error>` based.
- No exceptions are introduced as normal control flow.

---

# Suggested Completion Notes Template

Completion notes (YYYY-MM-DD):

- Split `src/import/gltf/Importer.cpp` into focused private implementation files for cgltf utilities, data URI decoding, image import, material/texture/sampler import, mesh import, skin import, animation import, node import, and scene assembly.
- Kept the public `Importer` and `load_scene()` APIs unchanged.
- Followed the one-move-at-a-time rule: each responsibility group was moved, compiled, and validated with `gltf_importer_regression` before the next group was moved.
- Moved `CGLTF_IMPLEMENTATION` and `STB_IMAGE_IMPLEMENTATION` into single dedicated translation units to avoid multiple-definition hazards.
- Preserved current importer behavior and error handling while reducing `Importer.cpp` to the public adapter/entrypoint.
- Updated `CMakeLists.txt` so the `stellar_import_gltf` target builds the new implementation files only when `STELLAR_ENABLE_GLTF` is enabled.
- Validated with:
  - `<build command/result>`
  - `<per-move gltf_importer_regression results>`
  - `<final gltf_importer_regression result>`
  - `<full/default ctest result>`
- Deferred non-structural importer improvements:
  - broader glTF extension support
  - behavior changes to tangent generation
  - behavior changes to color-space reuse handling
  - importer performance rewrites

---

Completion notes (2026-04-27):

- Split `src/import/gltf/Importer.cpp` into focused private implementation files for cgltf utilities, data URI decoding, image import, material/texture/sampler import, mesh import, skin import, animation import, node import, and scene assembly.
- Kept the public `Importer`, `create_importer()`, and `load_scene()` APIs unchanged.
- Added private declarations in `src/import/gltf/ImporterPrivate.hpp` and reduced `Importer.cpp` to the public adapter/entrypoint that delegates to `load_scene_from_file()`.
- Moved `CGLTF_IMPLEMENTATION` into `CgltfUtils.cpp` and `STB_IMAGE_IMPLEMENTATION` into `ImageImport.cpp`; each macro now appears in exactly one `.cpp`.
- Preserved current importer behavior and error handling; no asset structs, renderer-facing data, public importer API, cgltf/stb usage, or build default were changed.
- Updated `CMakeLists.txt` so the `stellar_import_gltf` target builds the new implementation files only when `STELLAR_ENABLE_GLTF` is enabled.
- Validated with:
  - `cmake -S . -B build -DSTELLAR_ENABLE_GLTF=ON`: passed.
  - `cmake --build build -j$(nproc)`: passed after resolving split/linkage compile issues.
  - Checkpoint 1 `gltf_importer_regression`: passed.
  - Moved responsibility groups regression validation: `gltf_importer_regression` passed after the split was complete.
  - `ctest --test-dir build --output-on-failure`: passed, 7/7 tests.
  - Explicit final tests passed: `gltf_importer_regression`, `client_asset_validation_smoke`, `client_cli_asset_validation`, and `render_scene_inspection`.
  - Optional default non-glTF check passed: `cmake -S . -B build-default && cmake --build build-default -j$(nproc) && ctest --test-dir build-default --output-on-failure`, 6/6 tests.
- Deferred non-structural importer improvements:
  - broader glTF extension support
  - behavior changes to tangent generation
  - behavior changes to color-space reuse handling
  - importer performance rewrites
