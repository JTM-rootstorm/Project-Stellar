Prepared: 2026-05-01  
Branch target: `bsp-integration`  
Audience: Kilo Code agents / Codex-style implementation agents  
Objective: implement the next required BSP support hardening after the canonical BSP migration.

# Phase 1 — BSP Diagnostics and LevelAsset Contract Foundation

## Purpose

Add the data contracts required for PVS/leaf visibility, lightmaps/materials, and actionable map
validation without changing gameplay behavior or requiring renderer work.

This phase is the required foundation for parallel Phase 2/3/4 work.

## Primary agent

`@carmack`, coordinated by `@director`

## Parallelism

Do not parallelize this phase with Phase 2/3/4 because it defines shared data contracts.

## Files likely touched

- `include/stellar/assets/LevelAsset.hpp`
- `include/stellar/import/bsp/Loader.hpp`
- `src/import/bsp/BspBinary.hpp`
- `src/import/bsp/BspParser.cpp`
- `src/import/bsp/BspLevelBuilder.cpp`
- `tests/import/bsp/BspImporter.cpp`
- new `include/stellar/import/bsp/Diagnostics.hpp` or equivalent
- new `tests/import/bsp/BspDiagnostics.cpp`
- `CMakeLists.txt`
- `docs/ImplementationStatus.md`

## Required contracts

### 1. Structured BSP diagnostics

Add a small diagnostics model that can be returned or attached to `LevelAsset` import results without
breaking existing `std::expected<LevelAsset, Error>` load APIs.

Suggested public/internal shape:

```cpp
namespace stellar::import::bsp {

enum class DiagnosticSeverity {
    kInfo,
    kWarning,
    kError,
};

enum class DiagnosticCode {
    kUnsupportedVersion,
    kLumpOutOfBounds,
    kMalformedEntityText,
    kMissingWorldspawn,
    kMissingPlayerSpawn,
    kMissingTexture,
    kUnsupportedTextureFormat,
    kInvalidFaceReference,
    kInvalidModelReference,
    kInvalidVisibilityData,
    kInvalidLightingData,
    kUnsupportedEntityKey,
};

struct Diagnostic {
    DiagnosticSeverity severity;
    DiagnosticCode code;
    std::string message;
    std::string source_uri;
    std::optional<std::size_t> lump_index;
    std::optional<std::size_t> entity_index;
    std::optional<std::size_t> face_index;
};

struct ImportReport {
    std::vector<Diagnostic> diagnostics;
    bool has_errors = false;
};

}
```

Keep fatal parse errors as `std::unexpected(Error)` for now. Use the report for non-fatal warnings and
later validation tooling.

### 2. Expanded `LevelVisibilityAsset`

Current visibility is a boolean placeholder. Expand it enough for classic BSP visibility and future
render culling.

Suggested source-neutral shape:

```cpp
struct LevelLeaf {
    std::int32_t contents = 0;
    std::array<float, 3> bounds_min{};
    std::array<float, 3> bounds_max{};
    std::vector<std::size_t> surface_indices;
    std::optional<std::size_t> compressed_pvs_offset;
};

struct LevelVisibilityAsset {
    bool available = false;
    std::vector<LevelLeaf> leaves;
    std::vector<std::byte> compressed_pvs;
    std::size_t cluster_count = 0;
};
```

Do not put renderer handles or gameplay authority in this structure.

### 3. Lightmap/material placeholders

Keep this phase minimal: ensure `LevelSurfaceMaterial::lightmap_index` and `StaticVertex::uv1` can be
used later without changing APIs again. If a small `LevelLightmapInfo` type is needed, add it now.

Suggested minimal shape:

```cpp
struct LevelLightmap {
    std::size_t image_index = 0;
    std::array<std::uint32_t, 2> size{};
    std::uint8_t style = 0;
    std::string source_name;
};
```

A `std::vector<LevelLightmap>` can live in `LevelGeometryAsset` if useful. Otherwise defer exact
storage to Phase 3, but document the decision in `ImplementationStatus.md`.

### 4. Parser struct additions

Add version-aware internal structs for classic BSP nodes, leaves, marksurfaces, visibility bytes, and
lighting bytes. Current `BspMap` has lump identifiers but not enough parsed leaf data.

Suggested internal additions:

- `Node`
- `Leaf`
- `Marksurface`
- raw `visibility_bytes`
- raw `lighting_bytes`

Keep parsed structs internal unless source-neutral assets need them.

## Required tests

Add display-free tests for:

- Import report collects non-fatal missing-player-spawn warning.
- Fatal errors still return `std::unexpected`.
- Visibility placeholder contracts survive minimal fixture with no vis data.
- Raw lighting and visibility lumps can be present without requiring renderer use.
- Existing `bsp_importer`, runtime, collision, and playable smoke tests still pass.

## Acceptance criteria

- Current behavior remains valid for existing generated BSP fixtures.
- `LevelAsset` has stable structures for later PVS/lightmap/material implementation.
- Default build and tests pass.
- No renderer or server behavior changes are required in this phase.
- `docs/ImplementationStatus.md` records Phase 1 completion notes.

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_importer|runtime_world|collision_world|world_metadata_validation|collision_validation)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

## Stop conditions

Stop if the diagnostic or visibility contracts require changing authoritative collision/movement.
Visibility and materials are presentation/import concerns only.
