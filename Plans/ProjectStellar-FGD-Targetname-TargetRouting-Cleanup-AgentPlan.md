# Project Stellar — FGD Cleanup, Canonical Entity Names, and Target Routing Alignment Plan

Target branch: `fixes`

## Purpose

Clean up the TrenchBroom FGD so entity fields are understandable and not redundant, then mirror the cleanup in importer/preflight/runtime code. The main functional goal is to make authored entity targeting reliable: triggers, buttons, relays, and other routing entities must be able to target a specific entity by a canonical **Name** field rather than by `classname`.

This plan is optimized for AI implementation agents. Do not commit unless explicitly instructed by the user.

---

## Executive summary

Use the classic Quake/Half-Life contract:

- **Class** is `classname`.
  - Defines what the entity is: `trigger_multiple`, `func_door`, `light_spot`, etc.
  - Never use `classname` as the targetable identity.

- **Name** is `targetname`.
  - This is the canonical map key for a targetable entity's name.
  - In the FGD, display it as `Name / targetname`.

- **Target** is `target`.
  - Points to another entity's `targetname`.

Optional aliases may be accepted by the importer for convenience:
- `_stellar_name`
- `name`

But the canonical authoring/output key should remain `targetname` for TrenchBroom, VHLT, and GoldSrc-style compiler compatibility.

Current issue:
- The FGD has a `Targetname` base, but some classes that should fire targets do not expose `target`.
- `LightKeys` redundantly declares `targetname` instead of using `Targetname`.
- `TargetRouting` is too broad and exposes fields like `killtarget`, `message`, and `delay` on entities where runtime support is unclear.
- Runtime already has target routing pieces: triggers scan marker properties for `target`, and `WorldSession::fire_target()` opens/presses named brush movers. The editor contract should expose the correct fields for that behavior.

---

## Current branch evidence

Relevant files:

- `tools/trenchbroom/Stellar/stellar_entities.fgd`
- `tools/bsp/stellar_entities.fgd`
- `tools/bsp/validate_trenchbroom_map_source.py`
- `src/import/bsp/BspLevelBuilder.cpp`
- `src/server/WorldSession.cpp`
- `tests/import/bsp/FgdContract.cpp`
- `tests/import/bsp/BspEntityAuthoring.cpp`
- `tests/server/WorldSession.cpp`
- `docs/TrenchBroom.md`
- `docs/BspAuthoring.md`

Observed current FGD issues:

1. `Targetname` exists but its description is too terse:
   ```fgd
   @BaseClass = Targetname
   [
       targetname(string) : "Name"
   ]
   ```

2. `TargetRouting` is too broad:
   ```fgd
   @BaseClass = TargetRouting
   [
       target(string) : "Target entity to fire"
       killtarget(string) : "Target entity to remove/disable when supported"
       message(string) : "Message or presentation text"
       delay(string) : "Delay before firing target, seconds" : "0"
   ]
   ```

3. `LightKeys` declares `targetname` directly, instead of inheriting `Targetname`.

4. Trigger classes have `Targetname` but do not inherit any field that exposes `target`:
   - `trigger_stellar`
   - `trigger_multiple`
   - `trigger_once`
   - `trigger_stellar_point`
   - `trigger_multiple_point`
   - `trigger_once_point`

5. `func_button` and `func_door` both inherit broad `TargetRouting`, but runtime currently uses `func_button.target` and opens doors by targetname. Door output fields may be overexposed if not implemented.

6. `stellar_global_light` does not inherit `Targetname`; it should for consistency, even if not commonly targeted.

7. `validate_trenchbroom_map_source.py` validates `target` only against `targetname`. It should understand canonical name aliases if the importer accepts them.

---

## Desired authoring contract

### Canonical fields

| Concept | Canonical key | FGD display text | Notes |
|---|---|---|---|
| Entity class/type | `classname` | implicit class name | Never targeted directly |
| Entity name | `targetname` | `Name / targetname` | Canonical targetable identity |
| Fire target | `target` | `Target entity name to fire` | Points to a `targetname` |
| Delay before target fire | `delay` | `Delay before firing target, seconds` | Only on firing entities |
| Kill/remove target | `killtarget` | `Targetname to remove/disable when supported` | Only expose if implemented |
| Presentation message | `message` | `Message text` | Only expose where used |

### Alias policy

Importer should accept these aliases as fallback for runtime name resolution:

1. `targetname`
2. `_stellar_name`
3. `name`

Precedence:

```text
targetname > _stellar_name > name > generated fallback
```

Warnings:
- If multiple name fields exist and disagree, warn and use `targetname` if present.
- If only `name` exists, allow it but optionally warn that `targetname` is canonical.

FGD should prefer `targetname`; it may expose `_stellar_name` only if the team wants an explicit Stellar alias. Recommended first pass: do **not** expose `_stellar_name` in normal FGD unless there is a strong reason; support it in importer/preflight for resilience.

---

## Phase 1 — Refactor FGD base classes

Files:
- `tools/trenchbroom/Stellar/stellar_entities.fgd`
- `tools/bsp/stellar_entities.fgd`

Keep the TrenchBroom package FGD authoritative and mirror the BSP helper FGD exactly.

### Replace/clarify base classes

Recommended base layout:

```fgd
@BaseClass = Targetname
[
    targetname(string) : "Name / targetname used by target fields"
]

@BaseClass = FiresTarget
[
    target(string) : "Targetname to fire"
    delay(string) : "Delay before firing target, seconds" : "0"
]

@BaseClass = KillTarget
[
    killtarget(string) : "Targetname to remove/disable when supported"
]

@BaseClass = MessageText
[
    message(string) : "Message or presentation text"
]

@BaseClass = AimTarget
[
    target(string) : "Targetname to aim at; compiler may ignore pitch/yaw when set"
]
```

Do not keep one broad `TargetRouting` base unless every inheriting class fully supports all fields. If keeping it for compatibility, mark it legacy/internal and avoid using it in concrete class inheritance.

### Light keys

Change `LightKeys` so it does **not** declare `targetname` directly.

Current:
```fgd
@BaseClass = LightKeys
[
    targetname(string) : "Name"
    origin(string) : "Origin" : "0 0 64"
    ...
]
```

Recommended:
```fgd
@BaseClass = LightKeys
[
    origin(string) : "Origin" : "0 0 64"
    _light(string) : "Compile-time RGB/intensity: r g b brightness" : "255 255 255 200"
    light(string) : "Compile-time brightness or RGB/intensity alias" : "200"
    style(string) : "Compile-time light style" : "0"
    pattern(string) : "Compile-time custom light pattern"
    spawnflags(flags) =
    [
        1 : "Initially dark" : 0
    ]
]
```

Then define lights as:
```fgd
@PointClass base(Targetname, LightKeys) ... = light
@PointClass base(Targetname, LightKeys, Angles, AimTarget) ... = light_spot
@PointClass base(Targetname, LightKeys, Angles) ... = light_environment
```

### Trigger classes

Add `FiresTarget` to trigger classes.

Change:
```fgd
@SolidClass base(Targetname, StellarScript, TriggerKeys) = trigger_multiple
```

To:
```fgd
@SolidClass base(Targetname, FiresTarget, StellarScript, TriggerKeys) = trigger_multiple
```

Apply to:
- `trigger_stellar`
- `trigger_multiple`
- `trigger_once`
- `trigger_stellar_point`
- `trigger_multiple_point`
- `trigger_once_point`

### Buttons, doors, relays

`func_button`:
```fgd
@SolidClass base(Targetname, FiresTarget, Angles, StellarScript, StellarCollision) = func_button
```

`func_door`:
- The door must be targetable, so keep `Targetname`.
- Only expose `target` on `func_door` if runtime implements “on open / on close / when triggered, fire target.”
- Current runtime opens the door when `fire_target(mover.name)` matches, but does not fire door.target for doors. It does fire `func_button.target`.

Recommended safe FGD:
```fgd
@SolidClass base(Targetname, Angles, StellarScript, StellarCollision) = func_door
```

If door output routing is desired now, implement it first and then use:
```fgd
@SolidClass base(Targetname, FiresTarget, Angles, StellarScript, StellarCollision) = func_door
```

`target_stellar_relay`:
```fgd
@PointClass base(Targetname, FiresTarget, KillTarget, MessageText) = target_stellar_relay
```

Only include `KillTarget` and `MessageText` if runtime supports or intentionally preserves them. Otherwise keep just `FiresTarget`.

### Global light

Change:
```fgd
@PointClass color(...) = stellar_global_light
```

To:
```fgd
@PointClass base(Targetname) color(...) = stellar_global_light
```

### Object colliders, sprites, spawns

Keep `Targetname` where appropriate:
- `info_player_start`
- `info_player_deathmatch`
- `info_stellar_spawn`
- `stellar_sprite`
- `env_sprite`
- `stellar_object_collider`
- `stellar_object_collider_point`
- `func_wall`
- `func_illusionary`
- `func_detail`
- `info_null`

Update descriptions to be concise. Avoid repeated phrases like “alias for stellar.*” in every field if a top-level FGD comment already explains alias policy. Prefer short field descriptions.

---

## Phase 2 — Mirror naming in importer

File:
- `src/import/bsp/BspLevelBuilder.cpp`

### Add canonical name helper

Add:

```cpp
[[nodiscard]] const std::string* entity_name_value(const Entity& entity) noexcept {
    if (const std::string* value = value_for(entity, "targetname");
        value != nullptr && !value->empty()) {
        return value;
    }
    if (const std::string* value = value_for(entity, "_stellar_name");
        value != nullptr && !value->empty()) {
        return value;
    }
    if (const std::string* value = value_for(entity, "name");
        value != nullptr && !value->empty()) {
        return value;
    }
    return nullptr;
}

[[nodiscard]] std::string entity_name_or(const Entity& entity,
                                         std::string fallback = {}) {
    if (const std::string* value = entity_name_value(entity)) {
        return *value;
    }
    return fallback;
}
```

### Add conflict diagnostics

Add a helper:
```cpp
void add_name_alias_diagnostics(const Entity& entity, ImportReport* report, ...);
```

Rules:
- If `targetname`, `_stellar_name`, and/or `name` exist with different nonempty values:
  - warning: `entity has conflicting name aliases; targetname takes precedence`
- If only `name` exists:
  - optional info/warning: `name is accepted as alias; targetname is canonical`

### Replace direct targetname lookups

Update uses of:
```cpp
value_for(entity, "targetname")
string_or(entity, "targetname")
```

for runtime identity to use `entity_name_or`.

Use this for:
- `WorldMarker.name`
- `LevelBrushEntity.name`
- `LevelBrushEntity.targetname`
- brush collision names where appropriate
- trigger names
- object collider names
- sprite names
- generic spawn names

Do not use aliases for:
- `target` itself
- `classname`
- compiler-specific fields

### Preserve properties

Still call `copy_properties()` so original source key/value metadata remains available.

---

## Phase 3 — Mirror naming and target lookup in preflight

File:
- `tools/bsp/validate_trenchbroom_map_source.py`

### Add known Stellar name keys

Add to `KNOWN_STELLAR_KEYS`:
```python
"_stellar_name"
"stellar.name"
```

Only add `stellar.name` if importer also supports it. Recommended importer support:
- `_stellar_name`
- `stellar.name`
- `name`
- `targetname`

### Add canonical name helper

```python
def canonical_entity_name(entity: dict[str, str]) -> str:
    for key in ("targetname", "_stellar_name", "stellar.name", "name"):
        value = entity.get(key, "")
        if value:
            return value
    return ""
```

### Validate name alias conflicts

For every entity:
- collect nonempty name fields.
- if more than one unique value:
  - warning, not error: `conflicting name aliases; targetname takes precedence`.

### Update target resolution

Current target lookup only uses:
```python
targetnames = {entity.get("targetname", "") ...}
```

Change to:
```python
targetnames = {canonical_entity_name(entity) for entity in entities if canonical_entity_name(entity)}
```

Then validate `target` against canonical name set.

### Duplicate name policy

Classic target systems often allow multiple entities with the same `targetname` for group firing. Do not hard-error duplicates.

Recommended:
- allow duplicates.
- optionally warn only for classes where uniqueness is required by current runtime.
- Runtime `fire_target()` already loops movers and can open multiple matching movers, so duplicate names can be useful.

### Target field semantics

If a class has `target` but it is not a firing/aiming class:
- warning or allow, depending on compatibility.
- Do not block classic `light_spot` target aim.

---

## Phase 4 — Runtime target routing audit

File:
- `src/server/WorldSession.cpp`

Current behavior:
- Trigger tick code scans `world_->world_metadata.markers`.
- If `marker.name` equals trigger event name, it looks for marker property `target`.
- It calls `fire_target(prop.value)`.
- `fire_target()` opens/presses brush movers whose `mover.name == targetname`.
- `func_button` fires its own `target`.
- `func_door` opens but does not fire its own `target`.

### Required audit

1. Confirm `WorldMarker.name` is populated with canonical `entity_name_or()`.
2. Confirm `MovementTriggerEvent.trigger_name` uses the same canonical name.
3. Confirm brush mover `mover.name` uses canonical `entity_name_or()`.
4. Confirm `fire_target()` matching uses canonical runtime name fields.

### Optional door output implementation

If `func_door` should fire a target after opening:
- add fields to `BrushMover`:
  - `target`
  - maybe `target_on_open`
  - maybe `target_on_close`
- When a door reaches open state, schedule/fire its target once.
- Update FGD accordingly.

If not implementing door outputs:
- remove `target` from `func_door` FGD for now to avoid misleading authors.

### Relay behavior

`target_stellar_relay` currently appears as metadata, but `WorldSession::fire_target()` only loops brush movers. Decide whether relay should be implemented now.

Recommended:
- Implement minimal relay if it is exposed:
  - When `fire_target(relay.targetname)` matches a relay marker:
    - fire relay’s `target`, respecting `delay`.
    - optionally handle spawnflag “remove after fire” later.
- If not implementing relay now:
  - reduce FGD description to “metadata placeholder” and do not expose unsupported fields.
  - Preferred for usability: implement minimal relay.

---

## Phase 5 — FGD contract and lint updates

Files:
- `tests/import/bsp/FgdContract.cpp`
- any FGD lint tooling

Add assertions:

### Naming fields

- Every targetable class exposes `targetname`.
- `LightKeys` no longer directly declares `targetname`; lights inherit `Targetname`.
- `stellar_global_light` has `targetname`.

### Firing fields

Classes that must expose `target`:
- `trigger_stellar`
- `trigger_multiple`
- `trigger_once`
- `trigger_stellar_point`
- `trigger_multiple_point`
- `trigger_once_point`
- `func_button`
- `target_stellar_relay`

Classes that should expose aim `target`:
- `light_spot`

Classes that should not expose `target` unless implemented:
- `func_door`
- `light`
- `light_environment`
- `info_player_start`
- `stellar_sprite`
- `env_sprite`
- object colliders

### Duplicate/redundant fields

Add checks to fail if:
- a concrete class contains duplicate field names inherited from multiple bases.
- a base like `LightKeys` declares `targetname` directly.
- fields are exposed on classes where runtime does not support them.

---

## Phase 6 — Importer and runtime tests

File:
- `tests/import/bsp/BspEntityAuthoring.cpp`

Add tests:

### Name alias resolution

1. `targetname` wins:
```cpp
{ "targetname", "Canonical" },
{ "_stellar_name", "Alias" },
{ "name", "Plain" }
```
Assert marker/brush name is `Canonical` and warning is emitted.

2. `_stellar_name` fallback:
```cpp
{ "_stellar_name", "StellarAlias" }
```
Assert runtime name is `StellarAlias`.

3. `name` fallback:
```cpp
{ "name", "PlainName" }
```
Assert runtime name is `PlainName`.

### Trigger target preservation

Map:
```text
trigger_multiple targetname TriggerA target MainDoor
func_door targetname MainDoor
```

Assert:
- trigger marker has name `TriggerA`
- trigger properties contain `target=MainDoor`
- door brush entity name is `MainDoor`

### Preflight alias target

Add source preflight test:
```map
{
"classname" "func_door"
"_stellar_name" "MainDoor"
...
}
{
"classname" "trigger_multiple"
"target" "MainDoor"
...
}
```

Expected:
- no unresolved target error.

### Runtime fire target

File:
- `tests/server/WorldSession.cpp`

Add scenario:
- trigger event fires target `MainDoor`
- brush mover named by `targetname`/alias opens.
- button target still fires door.
- duplicate targetnames optionally fire all matching movers.

### Relay tests

If relay implemented:
- `fire_target("RelayA")` fires relay target.
- relay delay schedules downstream fire.

---

## Phase 7 — Docs update

Files:
- `docs/TrenchBroom.md`
- `docs/BspAuthoring.md`
- `tools/trenchbroom/Stellar/README.md`

Add a concise entity-targeting section:

```text
Entity targeting:
- classname selects the entity type.
- targetname is the entity's Name.
- target points to another entity's targetname.
- Multiple entities may share a targetname for group firing.
- Stellar accepts _stellar_name/name aliases for import compatibility, but targetname is canonical.
```

Add examples:

### Trigger opens a door

```map
{
"classname" "trigger_multiple"
"targetname" "DoorTrigger"
"target" "MainDoor"
...
}
{
"classname" "func_door"
"targetname" "MainDoor"
...
}
```

### Button opens a door

```map
{
"classname" "func_button"
"targetname" "DoorButton"
"target" "MainDoor"
...
}
```

### Spotlight aim target

```map
{
"classname" "light_spot"
"target" "SpotTarget"
...
}
{
"classname" "info_null"
"targetname" "SpotTarget"
...
}
```

---

## Implementation order

1. Refactor FGD base classes in `tools/trenchbroom/Stellar/stellar_entities.fgd`.
2. Mirror FGD changes to `tools/bsp/stellar_entities.fgd`.
3. Update FGD contract tests.
4. Add importer canonical name helper and diagnostics.
5. Update all importer name assignment paths to use canonical helper.
6. Update source preflight target resolution and warnings.
7. Update runtime target routing only if importer change exposes issues.
8. Decide whether to implement relay and door output support or hide unsupported fields.
9. Add importer/preflight/runtime tests.
10. Update docs.
11. Run focused and full test suites.

---

## Acceptance criteria

### Automated

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Focused:

```bash
ctest --test-dir build -R 'fgd|bsp_entity|map_source|world_session|trenchbroom' --output-on-failure
```

Required:
- FGD contract passes.
- No duplicate/redundant FGD fields.
- Triggers expose `target`.
- `targetname` is canonical Name across FGD, importer, preflight, and runtime.
- `_stellar_name`/`name` aliases import correctly.
- Target preflight resolves aliases.
- Trigger target opens/activates intended named entity.
- Unsupported fields are either removed from FGD or implemented.

### Manual

In TrenchBroom:
- Select a trigger and see:
  - Name / targetname
  - Target entity name to fire
- Select a door and see:
  - Name / targetname
  - no misleading target field unless door output routing is implemented
- Select a button and see:
  - Name / targetname
  - Target entity name to fire
- Select `light_spot` and see:
  - Name / targetname
  - Aim target
  - clean pitch/yaw docs
- Author a trigger targeting a door by Name.
- Compile and confirm the trigger opens the door.

---

## Do not do

- Do not make `classname` the targetable identity.
- Do not replace classic `targetname` with plain `name` as the primary key.
- Do not expose unsupported FGD fields just because classic FGDs often have them.
- Do not break duplicate `targetname` group firing unless the runtime explicitly requires uniqueness.
- Do not remove source property preservation from importer metadata.
- Do not make TrenchBroom-specific labels diverge from importer/preflight/runtime behavior.
