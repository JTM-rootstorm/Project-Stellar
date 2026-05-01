# Phase 1 — Source-Neutral LevelAsset Contract

Branch target: `bsp-integration`  
Primary agent: `@carmack`  
Coordinator: `@director`  
Dependencies: Phase 0 complete  
Parallelizable: no for shared headers; this phase creates the contract other agents use.

---

## Goal

Replace the glTF-shaped `SceneAsset` runtime contract with a source-neutral `LevelAsset` contract suitable for BSP maps.

This phase should compile and pass tests using synthetic in-memory `LevelAsset` data before a BSP binary parser exists.

---

## Design target

Create a canonical level asset model that can represent:

- static BSP world geometry,
- static collision/query data,
- BSP entity/key-value metadata,
- player spawns,
- triggers,
- sprite markers,
- object-collider sensor markers,
- script bindings,
- surface/material names,
- optional lightmap/visibility data placeholders.

Do not keep the old glTF `SceneAsset` shape as the canonical runtime input.

---

## New/changed public APIs

### Add `include/stellar/assets/LevelAsset.hpp`

Required structs:

```cpp
namespace stellar::assets {

struct LevelSurfaceMaterial {
    std::string name;
    std::optional<std::size_t> texture_index;
    std::optional<std::size_t> lightmap_index;
    std::string source_name;
};

struct LevelSurface {
    std::string name;
    std::size_t mesh_index = 0;
    std::size_t primitive_index = 0;
    std::optional<std::size_t> material_index;
    std::array<float, 3> bounds_min{};
    std::array<float, 3> bounds_max{};
    std::uint32_t source_flags = 0;
};

struct LevelGeometryAsset {
    std::vector<MeshAsset> meshes;
    std::vector<LevelSurface> surfaces;
    std::vector<LevelSurfaceMaterial> materials;
    std::vector<ImageAsset> images;
    std::vector<TextureAsset> textures;
    std::vector<SamplerAsset> samplers;
};

struct LevelVisibilityAsset {
    bool available = false;
    // Keep initial fields minimal. BSP importer may fill leaves/clusters/PVS later.
};

struct LevelAsset {
    std::string source_uri;
    LevelGeometryAsset geometry;
    std::optional<LevelCollisionAsset> level_collision;
    WorldMetadataAsset world_metadata;
    LevelVisibilityAsset visibility;
};

} // namespace stellar::assets
```

Adjust names if needed, but keep the concepts above.

### Update `include/stellar/assets/WorldMetadataAsset.hpp`

Keep the existing marker model but remove glTF-specific wording.

Required changes:

- “authored world marker” stays.
- Script binding docs no longer mention glTF `extras`.
- Add `std::vector<std::pair<std::string, std::string>>` or equivalent key/value preservation only if needed for BSP entity metadata. Prefer a compact `std::vector<WorldEntityProperty>` if public API clarity matters.

Recommended addition:

```cpp
struct WorldEntityProperty {
    std::string key;
    std::string value;
};

struct WorldMarker {
    ...
    std::vector<WorldEntityProperty> properties;
};
```

### Update `include/stellar/assets/CollisionAsset.hpp`

Keep current triangle collision data. Extend only if needed for BSP clip hulls.

Required minimum:

- Preserve `CollisionTriangle`, `CollisionMesh`, and `LevelCollisionAsset` so existing movement tests continue to pass.
- Remove comments tying collision extraction to glTF.
- Allow collision meshes to be sourced from BSP models/entities by name.

Optional but recommended now:

```cpp
struct CollisionPlane {
    std::array<float, 3> normal{};
    float distance = 0.0F;
};

struct CollisionHull {
    std::string name;
    std::vector<CollisionPlane> planes;
    std::array<float, 3> bounds_min{};
    std::array<float, 3> bounds_max{};
    std::uint32_t source_contents = 0;
};
```

Do not force the character controller to use hulls in this phase. This phase defines asset capacity.

### Replace `SceneAsset` usage at API boundaries

Transition public runtime APIs from `SceneAsset` to `LevelAsset`:

- `RuntimeWorld::scene_asset` should become `RuntimeWorld::level_asset` or equivalent.
- `build_runtime_world(const SceneAsset&)` should become `build_runtime_world(const LevelAsset&)`.
- Client validation should prepare for `LevelAsset` output, but BSP loading can remain stubbed until Phase 2.

Do not delete the glTF importer yet in this phase if deletion breaks unrelated code. Deletion is Phase 5.

---

## Handling current MeshAsset / Scene / Animation types

The current `MeshAsset` can remain as the static geometry upload payload, but remove glTF-specific comments when touched.

Recommended cleanup rules:

- Keep `StaticVertex`, `MeshPrimitive`, and `MeshAsset` if they are still useful for BSP surface rendering.
- Remove or defer `SkinAsset`, `AnimationAsset`, and `scene::SceneGraph` only after BSP rendering no longer depends on them.
- Do not add a new model/skinning path.
- Do not retain glTF wording in public API comments.

---

## Tests

Add/adjust display-free tests:

- `tests/assets/LevelAsset.cpp` or similar, if an assets test target exists.
- `tests/world/RuntimeWorld.cpp` should build a synthetic `LevelAsset` with:
  - empty level,
  - level with collision triangles,
  - player spawn marker,
  - trigger marker,
  - sprite marker,
  - object collider marker.
- Existing collision, trigger, object-collider, scripting, and server tests should compile after replacing `SceneAsset` construction with `LevelAsset` construction.

---

## Acceptance criteria

- `LevelAsset` is the canonical runtime world input.
- `RuntimeWorld` no longer needs a glTF-shaped `SceneAsset` to assemble collision and metadata.
- Existing display-free world/server/physics/scripting tests pass with synthetic `LevelAsset` data.
- No BSP parser is required yet.
- No glTF deletion is required yet, but new code must not add glTF assumptions.

---

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(runtime_world|collision_world|character_controller|trigger_system|object_collider|server_world_session|scripted_world_session)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

If glTF tests still exist during this phase, they may fail until later phases. Prefer updating CMake so default tests stay meaningful rather than allowing known failures.

---

## Completion note template

Add to `docs/ImplementationStatus.md`:

```markdown
Phase BSP-1 is complete as of YYYY-MM-DD:

- Added source-neutral `LevelAsset` as the canonical runtime level input.
- Moved runtime world assembly off glTF-shaped `SceneAsset` assumptions.
- Preserved existing backend-neutral collision, trigger, object-collider, scripting, and server-authority seams.
- Updated synthetic display-free tests to construct runtime worlds from `LevelAsset`.

Validation run:

<commands and result>
```
