---
project: Project Stellar
branch: audio-impl
plan_family: audio_footsteps
agent_optimized_for: Codex-style implementation agents
status: proposed_handoff
generated: 2026-05-04
---

# Phase AF-1 — Collision Surface Metadata and Footstep Surface Resolver

## Goal

Attach stable floor surface identity to collision triangles so an authoritative ground hit can be mapped back to a BSP texture/source material.

This phase does **not** emit footsteps yet.

## Primary owner

`@carmack` / systems Codex agent.

## Consult

`@miyamoto` only for BSP import/material naming details.

## Depends on

AF-0.

## Can run in parallel with

- AF-1 docs/tests after API field names are agreed.
- AF-3 sound generation after the surface id list is agreed.

## Must finish before

AF-2.

## Core design

Add backend-neutral surface metadata to collision triangles.

Preferred minimal shape:

```cpp
namespace stellar::assets {

inline constexpr std::uint32_t kInvalidLevelSurfaceIndex = 0xFFFF'FFFFu;
inline constexpr std::uint32_t kInvalidLevelMaterialIndex = 0xFFFF'FFFFu;

/** @brief Source material and audio-surface metadata for one collision triangle. */
struct CollisionSurfaceMetadata {
    /** @brief LevelGeometryAsset::surfaces index, or invalid when unknown. */
    std::uint32_t surface_index = kInvalidLevelSurfaceIndex;

    /** @brief LevelGeometryAsset::materials index, or invalid when unknown. */
    std::uint32_t material_index = kInvalidLevelMaterialIndex;

    /** @brief Original source texture/material name preserved for diagnostics. */
    std::string source_material_name;

    /** @brief Server-safe resolved footstep surface id such as generic, wood, or metal. */
    std::string footstep_surface_id = "generic";
};

struct CollisionTriangle {
    std::array<float, 3> a{};
    std::array<float, 3> b{};
    std::array<float, 3> c{};
    std::array<float, 3> normal{};
    CollisionSurfaceMetadata surface{};
};

}
```

A parallel vector is also acceptable if it is clearly safer for ABI churn, but keep lookup simple:
`asset.level_collision->meshes[mesh].triangles[tri].surface.footstep_surface_id`.

## Surface resolver

Add a tiny deterministic resolver. Suggested location:

```text
include/stellar/world/FootstepSurface.hpp
src/world/FootstepSurface.cpp
tests/world/FootstepSurface.cpp
```

or, if this is considered importer-only:

```text
include/stellar/assets/SurfaceAudio.hpp
src/world/SurfaceAudio.cpp
```

Preferred API:

```cpp
namespace stellar::world {

/** @brief Return a stable footstep surface id from a source material or texture name. */
[[nodiscard]] std::string resolve_footstep_surface_id(std::string_view source_material_name);

}
```

Initial mapping:

```text
metal, grate, pipe, vent                -> metal
wood, plank, crate, board               -> wood
stone, rock, brick, tile                -> stone
dirt, mud, soil, sand                   -> dirt
grass, moss, foliage                    -> grass
water, slime, liquid                    -> water
concrete, cement, asphalt, dev/grid     -> concrete
empty/unknown                           -> generic
```

Rules:

- Case-insensitive.
- Normalize `\` to `/`.
- Strip extension if obvious.
- Do not parse `.stellar_material` sidecars for authoritative surface ids in this phase.
- Keep mapping deterministic and small. Retro scope beats completeness.

## BSP importer tasks

In `src/import/bsp/BspLevelBuilder.cpp` or the current face-to-collision path:

1. Locate where each BSP face material is resolved into `LevelSurfaceMaterial`.
2. When emitting collision triangles from that same face:
   - write `surface_index`
   - write `material_index`
   - write `source_material_name`
   - write `footstep_surface_id = resolve_footstep_surface_id(source_material_name)`
3. Preserve existing collision bounds and mesh naming.
4. For collision triangles not associated with a BSP face:
   - leave surface/material invalid
   - set `source_material_name = ""`
   - set `footstep_surface_id = "generic"`

## CollisionWorld query support

No new query API is required if callers can read:

```cpp
const auto& hit_tri =
  collision_world.asset().meshes[hit.mesh_index].triangles[hit.triangle_index];
```

If runtime collision translation/filtering makes direct asset lookup unsafe in a local code path, add a helper:

```cpp
[[nodiscard]] const stellar::assets::CollisionTriangle*
collision_triangle_for_hit(const stellar::physics::CollisionWorld& world,
                           const stellar::physics::RaycastHit& hit) noexcept;
```

Do not make `stellar_physics` depend on audio.

## Tests

Add/extend:

```text
tests/world/FootstepSurface.cpp
tests/import/bsp/BspMaterials.cpp
tests/import/bsp/BspImporter.cpp
tests/physics/CollisionWorld.cpp
```

Required coverage:

1. Resolver maps case-insensitive material names:
   - `METAL/GRATE01` -> `metal`
   - `wood/plank_01` -> `wood`
   - `dev/grid_32` or `stellar_dev_grid_32` -> `concrete`
   - `unknown_weird` -> `generic`
2. BSP import preserves source texture/material name on collision triangles.
3. Imported floor triangles get non-empty `footstep_surface_id`.
4. Existing synthetic collision-only tests still default to `generic`.
5. `CollisionWorld::probe_ground` still returns mesh/triangle indices that can index metadata.

## Acceptance criteria

- Collision triangles can carry floor material/source texture identity.
- Ground probe hits can be mapped to a stable footstep surface id.
- No gameplay events or sound routing are added in this phase.
- No renderer/audio authority is introduced.
- Default tests remain display-free.

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build --target \
  stellar_collision_world_test \
  stellar_bsp_materials_test \
  stellar_bsp_importer_test \
  -j$(nproc)
ctest --test-dir build -R '^(collision_world|bsp_materials|bsp_importer|footstep_surface)$' --output-on-failure
tools/dev/check_target_boundaries.sh .
```

If target names differ, use the repo's current test target names.

## COMMIT CHECKPOINT

Suggested commit message:

```bash
git add include/stellar/assets/CollisionAsset.hpp include/stellar/world/FootstepSurface.hpp \
        src/world/FootstepSurface.cpp src/import/bsp/BspLevelBuilder.cpp \
        tests/world/FootstepSurface.cpp tests/import/bsp tests/physics \
        CMakeLists.txt docs/ImplementationStatus.md
git commit -m "Add collision footstep surface metadata"
```

Do not push.
## Global invariants for every phase

- Target branch: `audio-impl`.
- Do not push. Local commits are allowed only at the explicit checkpoints in each phase.
- Keep server authority intact. Footsteps are server-approved presentation events, not client guesses.
- Keep renderer, HUD, and audio presentation-only. They must not become sources of gameplay truth.
- Keep collision/runtime/audio contracts backend-neutral unless the phase explicitly works on a presentation sink.
- Keep default tests display-free. Real audio device checks must be opt-in/manual.
- BSP remains the canonical playable level format.
- Scope stays retro and practical: implement footsteps only, with generated one-shot sounds for now.
- Do not add full material gameplay, dynamic rigid bodies, animation systems, prediction/reconciliation, or a broad ECS rewrite.
- Public APIs need Doxygen `@brief` comments.
- Preserve deterministic ordering and deterministic event selection.
- Update `docs/ImplementationStatus.md` after each implemented phase.
- Update `docs/Design.md` or `docs/BspAuthoring.md` only when a broad architecture or authoring convention changes.
- Do not modify `AGENTS.md` or `.kilo/agents/*.md` without explicit user approval.

## Standard safe local commit checkpoint

At the end of any phase or subphase that says `COMMIT CHECKPOINT`, run:

```bash
git status --short
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh .
git diff --check
git status --short
```

If all relevant validation passes, make a local commit only:

```bash
git add <phase files>
git commit -m "<phase-specific message>"
```

Do **not** run `git push`.
