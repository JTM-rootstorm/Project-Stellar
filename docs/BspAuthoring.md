# BSP Authoring Guide

BSP maps are Stellar's canonical playable level source. Entity keys are imported as backend-neutral world metadata for the authoritative runtime; import never executes scripts and never creates renderer, audio, ECS, or gameplay state directly.

## Minimal workflow

1. Build a classic BSP29/BSP30-style map in a Quake/GoldSrc-compatible editor or toolchain.
2. Author gameplay markers with ordinary entity key/value pairs.
3. Compile/export the `.bsp`.
4. Run the display-free validation commands below before using the map in runtime tests.

The conventions do not require one editor. Use custom key/value fields, smart-edit modes, FGD definitions, or equivalent editor mechanisms that preserve keys exactly.

## Entity key reference

| Purpose | Class/key | Required keys | Optional keys | Runtime semantics |
| --- | --- | --- | --- | --- |
| Player spawn | `info_player_start` | `origin` | `targetname`, `angle` | Creates a player spawn marker. |
| Generic spawn | `info_stellar_spawn` | `targetname`, `archetype`, `origin` | `stellar.script`, `stellar.table` | Creates metadata for server-side spawn logic; no entity is spawned during import. |
| Trigger volume | `trigger_stellar`, `trigger_multiple`, `trigger_once` | `targetname`, `model="*N"` or `origin` + `stellar.extents` | `stellar.script`, `stellar.table`, `stellar.once` | Creates a trigger marker. Script ids are metadata only until authoritative runtime invokes them. |
| Sprite billboard | `stellar_sprite`, `env_sprite`, or any entity with `stellar.sprite` | `targetname`, `origin`, `stellar.sprite` | `stellar.texture`, `stellar.size`, `stellar.alpha` | Creates a sprite marker for presentation. `stellar.script` is unsupported and ignored with a diagnostic. |
| Object-collider sensor | `stellar_object_collider` or `stellar.collider=object` | `targetname`, `model="*N"` or `origin` + `stellar.extents`, `stellar.collider=object` | `stellar.script`, `stellar.table`, `stellar.enabled` | Creates a server-side sensor marker. It is not a rigid body and does not block movement. |
| Static collision brush | `func_wall`, `func_door`, `func_button`, `trigger_*` | `model="*N"` | `targetname`, `stellar.collision=static|sensor|none` | Contributes named static collision where collision extraction supports it. Moving brush simulation is deferred. |

Raw BSP entity key/value pairs are preserved in `WorldMarker::properties` when raw entity preservation is enabled, including unsupported keys.

## Value formats

- `origin` and `stellar.extents` are three floats: `"x y z"`.
- `stellar.size` is two floats: `"width height"`.
- Boolean-like values accept only `1`, `0`, `true`, `false`, `yes`, and `no`.
- Script ids must be asset-relative identifiers or paths. Absolute paths, drive-letter paths, and `..` parent escapes are rejected.

Malformed vectors and booleans produce import diagnostics. Import does not run Lua.

## Examples

### Player spawn

```text
classname = info_player_start
targetname = PlayerStart
origin = "0 0 32"
angle = "90"
```

### Trigger script

Brush-backed trigger:

```text
classname = trigger_multiple
targetname = DoorTrigger
model = "*1"
stellar.script = "scripts/door.lua"
stellar.table = "Door"
stellar.once = "false"
```

Point-authored fallback trigger:

```text
classname = trigger_stellar
targetname = PickupArea
origin = "128 64 32"
stellar.extents = "16 16 24"
stellar.script = "scripts/pickup_area.lua"
stellar.table = "PickupArea"
```

### Sprite billboard

```text
classname = stellar_sprite
targetname = TorchSprite
origin = "64 64 48"
stellar.sprite = "torch"
stellar.texture = "sprites/torch.png"
stellar.size = "1.0 2.0"
stellar.alpha = "blend"
```

Do not attach `stellar.script` to sprite markers; sprite script bindings are unsupported and diagnosed.

### Object-collider sensor

```text
classname = stellar_object_collider
targetname = PickupGem
origin = "96 64 32"
stellar.extents = "8 8 8"
stellar.collider = "object"
stellar.script = "scripts/pickup.lua"
stellar.table = "PickupGem"
stellar.enabled = "yes"
```

### Static collision blocker

```text
classname = func_wall
targetname = DoorBlocker
model = "*2"
stellar.collision = "static"
```

Scripts may later request validated enable/disable changes by collision mesh name. The BSP import itself does not animate, move, or simulate the brush.

## Unsupported or deferred

- Moving brush simulation for `func_door`, `func_button`, plats, trains, or rotating entities.
- Client-side gameplay scripting or presentation script execution.
- Sprite script callbacks.
- Renderer-owned or audio-owned gameplay state.
- Dynamic rigid bodies and third-party physics integration.
- Source/VBSP-specific entities and lump formats.

## Validation commands

Map validation is display-free and does not create a window, graphics context, renderer resources, or load external WAD files by default. Use either client validation form:

```bash
stellar-client --validate-config --map path/to/map.bsp
stellar-client --validate-map path/to/map.bsp
```

Both commands import the BSP, build the source-neutral runtime world, and fail for fatal validation errors such as unsupported BSP versions, malformed lump bounds, invalid model/face references that prevent import, or rejected script path escapes. Warning diagnostics do not fail validation by default.

Common diagnostics:

- `kLumpOutOfBounds` / `kMalformedLumpSize`: BSP binary structure is corrupt or uses unexpected lump sizes.
- `kInvalidFaceReference` / `kDegenerateFacePolygon`: a face cannot resolve enough valid edges/vertices to form a polygon; affected faces are skipped.
- `kInvalidVisibilityData`: PVS offsets, decompression rows, or marksurface references are invalid; visibility falls back to all surfaces.
- `kMissingTexture` / `kMaterialFallbackUsed`: texture data is external or missing; validation uses deterministic fallback materials and does not require WAD files.
- `kInvalidLightingData`: a face light offset or inferred lightmap byte range is outside the lighting lump; the face falls back to unlit material behavior.
- `kMissingPlayerSpawn`: no `info_player_start` or `info_player_deathmatch` marker exists; import succeeds but gameplay startup may need an explicit spawn policy.
- `kUnsupportedEntityKey`: malformed authoring values such as `origin`, `stellar.extents`, `stellar.size`, `stellar.once`, or `stellar.enabled` were ignored.
- `kScriptPathEscape`: script ids are absolute, drive-letter based, or contain `..`; validation fails to preserve Lua sandbox boundaries.
- `kNoCollisionTriangles`: imported collision extraction produced no triangles; this is currently a warning for maps or tests with an explicit no-collision policy.

```bash
cmake -S . -B build-phase5-carmack -DCMAKE_BUILD_TYPE=Debug
cmake --build build-phase5-carmack -j$(nproc)
ctest --test-dir build-phase5-carmack -R '^(bsp_validation|bsp_importer|client_map_validation_smoke|client_cli_map_validation|bsp_authoring_smoke)$' --output-on-failure
```

For broad confidence, also run:

```bash
ctest --test-dir build-phase5-carmack --output-on-failure
```
