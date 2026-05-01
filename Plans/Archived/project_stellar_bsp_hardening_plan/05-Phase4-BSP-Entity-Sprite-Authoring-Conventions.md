Prepared: 2026-05-01  
Branch target: `bsp-integration`  
Audience: Kilo Code agents / Codex-style implementation agents  
Objective: implement the next required BSP support hardening after the canonical BSP migration.

# Phase 4 — BSP Entity, Sprite, Trigger, and Object Authoring Conventions

## Purpose

Define and validate the BSP entity-key conventions authors should use for Stellar gameplay markers:
player starts, generic spawns, triggers, sprites, object-collider sensors, script bindings, and static
collision controls.

## Primary agents

- `@kojima`: gameplay/authoring semantics.
- `@carmack`: parser/validation/runtime marker plumbing.
- `@director`: resolves cross-subsystem naming and docs decisions.

## Parallelism

Can run in parallel with Phase 2/3 after Phase 1. Coordinate shared metadata changes.

## Files likely touched

- `include/stellar/assets/WorldMetadataAsset.hpp`
- `src/import/bsp/BspLevelBuilder.cpp`
- `src/world/WorldMetadataValidation.cpp`
- `include/stellar/world/WorldMetadataValidation.hpp`
- `tests/import/bsp/BspImporter.cpp`
- new `tests/import/bsp/BspEntityAuthoring.cpp`
- `tests/world/WorldMetadataValidation.cpp`
- `tests/integration/BspPlayableWorldSmoke.cpp`
- new `docs/BspAuthoring.md`
- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `CMakeLists.txt`

## Required authoring conventions

Document and test these conventions.

### Player spawn

```text
classname = info_player_start
targetname = optional stable marker name
origin = "x y z"
angle = optional yaw
```

### Generic entity spawn

```text
classname = info_stellar_spawn
targetname = stable marker name
archetype = gameplay archetype id
origin = "x y z"
stellar.script = optional script id
stellar.table = optional Lua table
```

### Trigger volume

Brush-backed:

```text
classname = trigger_stellar | trigger_multiple | trigger_once
targetname = stable trigger name
model = "*N"
stellar.script = optional script id
stellar.table = optional Lua table
stellar.once = optional bool-like value
```

Point/AABB fallback if brush model is absent:

```text
origin = "x y z"
stellar.extents = "x y z"
```

### Sprite billboard

```text
classname = stellar_sprite | env_sprite
targetname = stable sprite name
origin = "x y z"
stellar.sprite = sprite archetype or atlas id
stellar.texture = optional texture/material id
stellar.size = "width height"
stellar.alpha = opaque | mask | blend
stellar.script = unsupported unless explicitly scoped later
```

### Object-collider sensor

```text
classname = stellar_object_collider
targetname = stable collider name
model = "*N" OR origin + stellar.extents
stellar.collider = object
stellar.script = optional script id
stellar.table = optional Lua table
stellar.enabled = optional bool-like value
```

### Static collision brush entity

```text
classname = func_wall | func_door | func_button | trigger_*
targetname = stable collision mesh name
model = "*N"
stellar.collision = static | sensor | none
```

Moving brush simulation remains deferred. A `func_door` can provide a named static collision mesh that
scripts may enable/disable, but it must not imply interpolation, networking, or physics motion yet.

## Parser/runtime requirements

1. Preserve raw key/value pairs in `WorldMarker::properties`.
2. Validate script paths using the existing no-absolute-path/no-parent-escape policy.
3. Parse bool-like fields deterministically (`1/0`, `true/false`, `yes/no`) or reject with warning.
4. Parse vector fields deterministically; malformed fields produce validation diagnostics.
5. Derive brush model bounds for triggers/object colliders when `model="*N"` exists.
6. Use `origin + stellar.extents` fallback for point-authored volumes.
7. Do not execute scripts during import.
8. Do not create renderer/audio/gameplay state from entity import directly.

## Required docs

Create `docs/BspAuthoring.md` with:

- minimal map-authoring guide;
- entity key reference table;
- examples for player spawn, trigger script, sprite, object collider, collision blocker;
- unsupported/deferred features;
- validation commands;
- notes for Quake/GoldSrc editor workflows without hard-coding a single editor dependency.

## Required tests

- Entity parser maps each documented convention to `WorldMarker`.
- Invalid vectors/booleans produce diagnostics.
- Script path escape is rejected.
- Brush model trigger/object-collider bounds are stable.
- Origin/extents fallback works.
- Unsupported script binding on sprite remains rejected unless intentionally changed.
- Existing scripted BSP smoke still proves trigger and object-collider Lua commands.

## Acceptance criteria

- Authors have a concrete BSP entity-key guide.
- Importer behavior matches the guide.
- Validation reports authoring mistakes clearly.
- Runtime semantics remain server-authoritative and display-free.

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_entity_authoring|bsp_importer|world_metadata_validation|bsp_playable_world_smoke|scripted_world_session)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

## Stop conditions

Stop if a proposed entity convention implies moving brush simulation, client-side scripting, or
renderer-owned gameplay state. Document it as deferred instead.
