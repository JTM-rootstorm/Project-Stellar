# TB-FULL-05 — Runtime Brush Entities, Moving Doors, Buttons, and Target Routing

Target branch: `trenchbroom-compat`

## Goal

Make advertised brush entities behave in-game. `func_door` and `func_button` must no longer be metadata-only. TrenchBroom-authored maps should support static blockers, visible nonblocking brushes, moving doors/buttons, target routing, collision updates, and network/presentation snapshots.

## Key files to inspect first

- `src/import/bsp/BspLevelBuilder.cpp`
- `include/stellar/assets/CollisionAsset.hpp`
- `include/stellar/assets/LevelAsset.hpp`
- `include/stellar/assets/WorldMetadataAsset.hpp`
- `src/world/RuntimeWorld.cpp`
- `src/world/RuntimeCollisionState.cpp`
- `src/physics/CollisionWorld.cpp`
- `src/server/WorldSession.cpp`
- `src/server/GameplayWorld.cpp`
- `src/network/Messages.cpp`
- `src/network/SnapshotCodec.cpp`
- `src/client/GameplayPresentation.cpp`
- `src/graphics/RenderLevel.cpp`
- `tests/world/*`
- `tests/server/*`
- `tests/client/*`
- `tests/network/*`
- `tests/fixtures/trenchbroom/src/*.map`

## Runtime behavior target

Final supported brush classes:

- `func_wall`: visible and solid static brush entity.
- `func_illusionary`: visible but nonblocking brush entity.
- `func_detail`: compile-time detail/static world behavior; importer should not create unintended gameplay entity if compiler collapses it into worldspawn.
- `func_door`: sliding brush mover with collision and presentation transform.
- `func_button`: pressable/toggle brush mover that can fire targets.
- `trigger_stellar`, `trigger_multiple`, `trigger_once`: brush or point volumes that fire script/target events.

## Tasks

### TB-FULL-05.1 — Preserve brush model ownership during BSP import

Problem: current level import can flatten model faces into one static mesh/collision stream. Moving brush entities need model ownership, names, transforms, and per-model render/collision instances.

Implement asset/runtime representation for BSP brush models:

- Preserve BSP `model_index` for each face/surface.
- Preserve brush entity classname, targetname, model string, origin, and relevant properties.
- Build separate mesh/collision records for brush models that require independent transforms.
- Keep worldspawn static geometry as model 0.
- For brush entities, record initial transform and local bounds.
- Ensure existing static rendering path still draws worldspawn correctly.

Possible structures:

- Add `LevelBrushEntity` asset structure, or encode via `WorldMarker` + surface/model references.
- Add `bsp_model_index`, `mesh_index`, `collision_mesh_name`, `classname`, `targetname`, and properties.
- Keep source-neutral design; do not leak raw BSP parser structures into runtime APIs.

Acceptance:

- Importer test verifies `func_door` surfaces are identifiable separately from worldspawn.
- Static fixtures still render/collide as before.
- Named collision mesh for `GateDoor` remains stable.

### TB-FULL-05.2 — Add transformed collision mesh support

Problem: existing static collision can enable/disable named meshes, but real moving doors/buttons require transforms.

Implement:

- Runtime collision state overlay supports per-mesh enable flag and transform.
- Collision queries apply enabled mesh transforms deterministically.
- Movement/sweep/ground probes respect moved meshes.
- Empty names are not targetable.
- Duplicate names are diagnosed and either rejected or toggled together consistently.
- Transform updates are server-authoritative.

Implementation options:

1. Query-time transform: keep local triangles and transform rays/capsules into local space.
2. Update-time world triangles: rebuild transformed triangle cache when mover changes.

Choose the simpler deterministic approach with tests. Avoid third-party physics.

Acceptance:

- Character cannot pass through closed door collision.
- Character can pass after door opens.
- Moving collision update is deterministic and does not mutate immutable `LevelAsset` data.

### TB-FULL-05.3 — Add brush mover runtime system

Implement server-authoritative brush mover behavior:

- `BrushMoverState`: closed, opening, open, closing, pressed, locked if needed.
- `func_door` movement:
  - Direction from `angle`, `angles`, or explicit movedir key.
  - Distance from brush bounds minus `lip` unless explicit distance key is added.
  - `speed`, `wait`, `delay`, and spawnflags used where supported.
  - Opens on trigger/button target fire or direct touch if configured.
  - Updates collision transform while moving.
- `func_button` movement:
  - Press movement along direction.
  - Fires `target` after press or according to `wait/delay` policy.
  - Returns after wait unless toggle spawnflag says otherwise.
- Trigger dispatch:
  - `trigger_once` fires only once.
  - `trigger_multiple` repeats with wait/cooldown.
  - `trigger_stellar` uses existing script hooks and can also fire targets.

Acceptance:

- Display-free tests cover door open/close timing.
- Button fires door target.
- Trigger opens door.
- `trigger_once` does not refire.
- `trigger_multiple` refires after wait.
- Existing scripted collision toggles still work or are migrated to the new mover system without breaking script tests.

### TB-FULL-05.4 — Add target routing system

Implement minimal target routing needed by TrenchBroom-authored interactions:

- Entities with `targetname` can receive fire/use events.
- Entities with `target` can fire named targets.
- Support `delay`.
- Support `killtarget` only if safe and needed; otherwise do not expose it in FGD.
- Script output may fire targets through a native validated command if needed.
- Server owns all target dispatch and validates target names.

Acceptance:

- Trigger -> door works.
- Button -> door works.
- Script -> target relay works if exposed.
- Missing target emits diagnostic but does not crash.

### TB-FULL-05.5 — Update gameplay snapshots and client presentation

Moving brush state must be visible to clients.

Implement:

- Server snapshots include brush entity id/name, class, transform, active/enabled/open state, and optional presentation metadata.
- Snapshot codec encodes/decodes brush movers deterministically.
- Client presentation applies transforms to brush models.
- Static worldspawn remains static and efficiently rendered.
- Remote client receives brush transform updates with no local authority.

Possible rendering change:

- `RenderLevel` may need per-brush instance transforms rather than one static world transform.
- If static world and moving brush models are separate meshes, draw worldspawn normally and draw brush instances with per-entity transforms.
- Ensure OpenGL/Vulkan paths receive correct MVP/world/normal matrices.

Acceptance:

- Display-free render inspection verifies moving brush model draws with non-identity world transform.
- Network snapshot tests verify brush state round trip.
- Remote client presentation uses server state only.

### TB-FULL-05.6 — Add brush/entity fixtures

Add source fixtures:

- `moving_door_button_zup.map`
  - Sealed room.
  - Player spawn.
  - `func_door` named `DoorA`.
  - `func_button` or `trigger_multiple` targets `DoorA`.
  - Light entity if TB-FULL-04 is complete.
- `point_volume_zup.map`
  - Point trigger and point object collider using final FGD classes.
- `illusionary_static_zup.map`
  - `func_wall` and `func_illusionary` behavior difference.

Acceptance:

- Source preflight passes.
- Generated BSP fixture writer or VHLT matrix includes equivalent compiled fixtures.
- Runtime tests validate expected behavior.

## Documentation updates

Update:

- `docs/BspAuthoring.md`
- `docs/TrenchBroom.md`
- `docs/ImplementationStatus.md`
- FGD descriptions
- Fixture README

Required content:

- `func_door` supported keys and behavior.
- `func_button` supported keys and behavior.
- Target routing examples.
- Trigger examples.
- Collision/presentation snapshot authority rules.

## Phase done checklist

- [ ] BSP import preserves brush model ownership.
- [ ] Collision overlay supports transforms.
- [ ] Doors move and affect collision.
- [ ] Buttons press and fire targets.
- [ ] Trigger target routing works.
- [ ] Snapshots replicate moving brush state.
- [ ] Client presentation renders moved brush models.
- [ ] New fixtures and tests pass.
