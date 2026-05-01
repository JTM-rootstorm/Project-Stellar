# Phase 2 — BSP Importer, Entity Metadata, and Collision Extraction

Branch target: `collision-movement`  
Primary agent: `@carmack`  
Gameplay metadata support: `@kojima` through `@director`  
Dependencies: Phase 1 complete  
Parallelizable: partially. Parser and entity mapping can be developed in parallel after public data shapes are agreed.

---

## Goal

Add a mandatory BSP importer that loads classic Quake/GoldSrc BSP data into `assets::LevelAsset`.

This phase must produce display-free tests using generated or tiny fixture BSP data. Rendering integration comes later.

---

## New module

Add:

- `include/stellar/import/bsp/Loader.hpp`
- `src/import/bsp/Loader.cpp`
- `src/import/bsp/BspBinary.hpp`
- `src/import/bsp/BspParser.cpp`
- `src/import/bsp/BspEntities.cpp`
- `src/import/bsp/BspLevelBuilder.cpp`
- Tests under `tests/import/bsp/`

CMake target:

- `stellar_import_bsp`
- Built by default. Do not gate BSP behind an option; it is canonical.

Public API:

```cpp
namespace stellar::import::bsp {

struct LoadOptions {
    bool preserve_raw_entities = true;
    bool build_triangle_collision_from_faces = true;
    bool parse_visibility = true;
    bool parse_lightmaps = true;
};

[[nodiscard]] std::expected<stellar::assets::LevelAsset, stellar::platform::Error>
load_level(std::string_view path, const LoadOptions& options = {});

[[nodiscard]] std::expected<stellar::assets::LevelAsset, stellar::platform::Error>
load_level_from_memory(std::span<const std::byte> bytes,
                       std::string source_uri,
                       const LoadOptions& options = {});

} // namespace stellar::import::bsp
```

Use `std::uint8_t` instead of `std::byte` if that is more consistent with existing code.

---

## BSP parser requirements

### Versions

- Accept Quake BSP29 and GoldSrc/Half-Life BSP30 style versions.
- Reject unknown versions with a deterministic error containing the version number.
- Use version-aware structs where lump layouts differ.
- Do not attempt Source/VBSP.

### Binary safety

The parser must:

- Treat file data as little-endian.
- Bounds-check every lump offset and size.
- Reject negative or overflowing offsets/sizes.
- Reject records whose size is not a multiple of the expected struct size.
- Avoid unaligned pointer casts over raw file bytes.
- Avoid exceptions.
- Return `std::expected<T, platform::Error>`.

### Required lumps to parse initially

Parse enough to build a playable static level:

- header/version/lump table,
- planes,
- vertices,
- edges,
- surfedges,
- faces,
- texinfo,
- textures or texture-name references,
- nodes/leaves enough for bounds/visibility placeholders,
- models/submodels,
- clipnodes/hulls where available,
- entities key/value string,
- lighting data if present,
- visibility data if present.

It is acceptable for rendering to initially draw all surfaces, but visibility data should be parsed/stored or explicitly diagnosed as unavailable.

---

## Level geometry conversion

Build `LevelAsset::geometry` from BSP faces:

- Convert BSP polygon faces to triangle primitives.
- Preserve surface/material names from texinfo/texture records.
- Generate world-space positions, normals, UVs, and bounds.
- Use one or more `MeshAsset` objects; do not require a scene graph.
- Preserve source face/model indices in internal or diagnostic metadata where useful.
- Material handling may initially produce placeholder materials by texture name, but texture names must be preserved.

Recommended MVP:

- One static mesh for worldspawn surfaces.
- Optional separate meshes for submodels/brush entities.
- One material entry per unique BSP texture name.
- Placeholder image/texture if embedded texture resolution is not complete yet.

---

## Collision extraction

Build `LevelCollisionAsset` from BSP data.

Required minimum:

- Create deterministic named collision meshes.
- Worldspawn collision mesh name: `worldspawn`.
- Brush/submodel collision mesh names use, in priority order:
  1. entity `targetname`,
  2. entity `classname` plus model index,
  3. `bsp_model_<index>`.
- Triangulate solid BSP faces into `CollisionTriangle` data so existing `CollisionWorld` can query them.
- Preserve bounds.
- Keep duplicate-name behavior compatible with current `RuntimeCollisionState`: duplicate names toggle together deterministically.

Recommended additional work:

- Parse clipnodes/hulls into optional collision hull data on `LevelCollisionAsset`.
- Keep existing triangle movement path active until hull movement is explicitly implemented.
- Add diagnostics that say whether collision came from face triangles, clip hulls, or both.

Do not implement full rigid body or moving brush simulation in this phase.

---

## Entity lump parsing

Add a deterministic parser for classic BSP entity strings:

Input format shape:

```text
{
"classname" "worldspawn"
"message" "Example"
}
{
"classname" "info_player_start"
"origin" "0 0 64"
"angle" "90"
}
```

Parser requirements:

- Preserve all key/value pairs in order.
- Support quoted strings and common escaped quotes/backslashes if present.
- Reject malformed unterminated blocks/strings with deterministic errors.
- Treat missing `classname` as a warning or generic entity, not a crash.
- Parse `origin` into position when valid.
- Parse `angle` or `angles` into orientation when valid.
- Parse model references like `*1` where useful for brush entity bounds.

### World metadata mapping

Map entities to `WorldMetadataAsset` markers:

| BSP entity | Marker type | Notes |
| --- | --- | --- |
| `info_player_start` | `kPlayerSpawn` | Name from `targetname` or `Player`. |
| `info_player_deathmatch` | `kPlayerSpawn` or `kEntitySpawn` | Keep deterministic order. |
| `info_stellar_spawn` | `kEntitySpawn` | Archetype from `archetype` key. |
| `trigger_multiple`, `trigger_once`, `trigger_stellar` | `kTrigger` | Extents from brush model bounds where possible. |
| `env_sprite`, `stellar_sprite`, entities with `stellar.sprite` key | `kSprite` | Name/archetype from key-values. |
| `stellar_object_collider`, entities with `stellar.collider=object` | `kObjectCollider` | Sensor-only AABB. |
| `func_wall`, `func_door`, `func_button`, other brush models | preserve properties | May contribute named collision meshes; moving behavior deferred. |

### Script binding vocabulary

Since glTF `extras.stellar.*` is gone, use BSP entity keys:

- `stellar.script` or `_stellar_script` → `WorldScriptBinding::script_id`
- `stellar.table` or `_stellar_table` → `WorldScriptBinding::table_name`

Validation rules:

- Empty script id is diagnostic.
- Empty table name is diagnostic if script id is present and table is required.
- Absolute paths and `..` path traversal are rejected as current script validation does.
- Trigger and object-collider script bindings are supported.
- Sprite/spawn script bindings remain unsupported unless a later phase explicitly adds invocation.

---

## Tests

Add display-free tests:

1. `bsp_parser_rejects_invalid_header`
2. `bsp_parser_rejects_unknown_version`
3. `bsp_parser_checks_lump_bounds`
4. `bsp_parser_loads_minimal_worldspawn_geometry`
5. `bsp_parser_builds_collision_triangles`
6. `bsp_entity_parser_preserves_key_values`
7. `bsp_entity_parser_rejects_malformed_entity_text`
8. `bsp_entities_build_player_spawn_trigger_sprite_object_collider_markers`
9. `bsp_script_bindings_import_to_world_metadata`
10. `bsp_loader_is_display_free`

Prefer generated binary fixtures in test code so tests are readable and version-controlled without opaque binary blobs. If a tiny `.bsp` fixture is committed, include a source comment or generator describing every byte/lump.

---

## CMake

Add BSP importer target to default build:

```cmake
add_library(stellar_import_bsp
    src/import/bsp/Loader.cpp
    src/import/bsp/BspParser.cpp
    src/import/bsp/BspEntities.cpp
    src/import/bsp/BspLevelBuilder.cpp
)

target_include_directories(stellar_import_bsp PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_import_bsp PUBLIC
    stellar_assets
)
```

Add importer test target to default tests. Do not gate behind a feature option.

---

## Acceptance criteria

- `stellar_import_bsp` is built by default.
- BSP loader can load minimal BSP29/BSP30 test fixtures into `LevelAsset`.
- Geometry, collision, metadata, and script bindings are represented without glTF.
- No renderer or display is required.
- Existing physics/world/server tests still pass.

---

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_|runtime_world|collision_world|world_metadata_validation|collision_validation)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

---

## Completion note template

Add to `docs/ImplementationStatus.md`:

```markdown
Phase BSP-2 is complete as of YYYY-MM-DD:

- Added mandatory `stellar_import_bsp` loader for the classic BSP29/BSP30 family.
- Added safe lump parsing, entity key/value parsing, and BSP-to-`LevelAsset` conversion.
- Built static geometry, named collision meshes, world metadata markers, and script bindings from BSP data.
- Added display-free BSP parser/importer regression tests.

Validation run:

<commands and result>
```
