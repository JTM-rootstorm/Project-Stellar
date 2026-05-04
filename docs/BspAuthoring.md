# BSP Authoring Guide

BSP maps are Stellar's canonical playable level source. Entity keys are imported as backend-neutral world metadata for the authoritative runtime; import never executes scripts and never creates renderer, audio, ECS, or gameplay state directly.

For the current TrenchBroom BSP30 editor workflow, package setup, FGD policy, compile wrappers, and
validation commands, see [`docs/TrenchBroom.md`](TrenchBroom.md). This page remains the lower-level BSP
entity and runtime metadata reference.

Gameplay authoring uses inch-scale Z-up coordinates: 1 Stellar gameplay world unit equals 1 inch, and
BSP30 coordinates are imported 1:1 without hidden scale conversion. The default player capsule center
should be authored at `origin = "0 0 36"` on a floor at `z = 0`.

## Minimal workflow

1. Create a TrenchBroom map with the Stellar game profile and BSP30 map format.
2. Build the first sealed room with an X/Y floor footprint, floor at `z = 0`, and ceiling at `z = 96`.
3. Place `info_player_start` at `origin = "0 0 36"` for the default 72 inch player capsule.
4. Use developer materials such as `dev/grid_32`, `dev/grid_64`, and `dev/wall_96` while blocking out.
5. Add compile-time `light`, `light_spot`, or `light_environment` entities when the map should bake
   static BSP lightmaps.
6. Optionally add `.stellar_material` sidecars for presentation-only normal/specular enrichment.
7. Author gameplay markers with ordinary entity key/value pairs or the Stellar FGD aliases.
8. Compile/export the `.bsp` as BSP30.
9. Run the display-free validation commands below before using the map in runtime tests.

VHLT compile success is not sufficient by itself. Always run Stellar client/server validation after
compile because the runtime importer can still reject unsupported BSP structure, missing gameplay
requirements, or sandbox-unsafe script ids.

The conventions do not require one editor. Use custom key/value fields, smart-edit modes, FGD
definitions, or equivalent editor mechanisms that preserve keys exactly. Dotted Stellar keys such as
`stellar.script` must reach the compiled BSP entity text as dotted keys or as importer-supported aliases
such as `_stellar_script`. The TrenchBroom package at `tools/trenchbroom/Stellar/` uses those
underscore aliases directly for reliability. It is the authoritative FGD; `tools/bsp/stellar_entities.fgd`
is a compatibility copy with the same concrete class/key contract, verified by display-free FGD lint.

For gameplay-scale fixtures, prefer authoring dimensions directly in inches instead of relying on
importer scale conversion. A practical first room is 192x192x96 authored units: a 16 ft by 16 ft floor
plan with an 8 ft ceiling, a player spawn at `0 0 36`, and developer grid/wall materials listed below.

## Procedural developer textures

BSP materials may reference these deterministic developer textures without embedding miptex pixels or
shipping external WAD files:

<!-- STELLAR_DEVELOPER_MATERIAL_TABLE_BEGIN -->
| Runtime material | Source alias | Compiler alias | Scale cue |
| --- | --- | --- | --- |
| `stellar_dev_grid_12` | `dev/grid_12` | `dev_grid_12` | 12 inch / 1 foot grid tile. |
| `stellar_dev_grid_16` | `dev/grid_16` | `dev_grid_16` | 16 inch tile/checker. |
| `stellar_dev_grid_32` | `dev/grid_32` | `dev_grid_32` | 32 inch tile/checker. |
| `stellar_dev_grid_64` | `dev/grid_64` | `dev_grid_64` | 64 inch tile/checker. |
| `stellar_dev_player_72` | `dev/player_72` | `dev_player_72` | 72 inch player-height reference marker. |
| `stellar_dev_wall_96` | `dev/wall_96` | `dev_wall_96` | 96 inch wall-height reference with overflow band. |
<!-- STELLAR_DEVELOPER_MATERIAL_TABLE_END -->

The importer generates `ImageAsset`/`TextureAsset` data for these names during BSP material fallback,
uses nearest filtering so markings stay crisp, and uses repeat wrapping so authored texture axes can
tile across room surfaces. With standard BSP texture axes, one texel/texture unit corresponds to one
world inch; changing editor texture scale changes the visible inch marks accordingly.

When compiling through VHLT, keep source `.map` files as clean authoring references. The VHLT wrapper
copies each map into a build/work directory, creates a temporary developer WAD, injects the copied map's
`wad` key there, rewrites compiler-facing aliases when required, and converts TrenchBroom/editor-facing
spotlight and environment-light pitch to VHLT/GoldSrc pitch on the copied work map before `hlrad`. Do not
commit local absolute WAD paths or generated compiler edits back into `maps/src/` or
`tests/fixtures/trenchbroom/src/`; authored source maps are untouched by the wrapper.

External WAD3 texture pixels are now resolved at BSP import time when the compiled `worldspawn` `wad`
key references safe relative WAD paths. The importer searches the BSP/map directory, roots from
`STELLAR_WAD_PATH` and `STELLAR_TEXTURE_PATH`, then the packaged developer materials directory. It
rejects `..` escapes and ignores absolute WAD paths unless `STELLAR_ALLOW_ABSOLUTE_WAD_PATHS` is
explicitly set for a trusted local workflow. Missing WAD diagnostics list attempted safe paths so editor
fixes map directly to either a corrected `wad` key or a configured search root.

## Material sidecars

BSP files remain the source for geometry, face texture names, lightmaps, visibility, entities, and
collision. Optional `.stellar_material` sidecars only enrich presentation materials; server-authoritative
gameplay, networking, collision, and scripting never depend on them.

Sidecars are looked up by normalized BSP texture name under configured material roots:

```text
materials/<normalized-bsp-texture-name>.stellar_material
materials/dev/wall_96.stellar_material
```

For a BSP face material named `dev/wall_96`, place the sidecar at `materials/dev/wall_96.stellar_material`
relative to a material search root. Texture paths inside the sidecar are relative to the sidecar file
directory. Absolute paths, drive-letter paths, empty path segments, and `..` escapes are rejected.
Missing sidecars preserve existing BSP texture/lightmap behavior without diagnostics; malformed
sidecars or missing referenced images produce deterministic diagnostics. Strict material-sidecar mode
turns invalid sidecars into import failures for fixture and tooling validation.

Normal maps require valid tangents. BSP import generates tangents from texinfo vectors where possible,
and surfaces with degenerate texture axes fall back to non-normal-mapped shading. Specular lighting is
lightweight presentation shading, not full PBR. Full PBR remains deferred until image-based lighting,
environment maps, tone mapping, color management, BRDF parity, and validation strategy are explicitly
scoped.

Sidecar reference:

```ini
version = 1
name = "dev/wall_96"
base_color = "wall_96_albedo.png"
normal = "wall_96_normal.png"
specular = "wall_96_spec.png"
normal_scale = 1.0
normal_light_strength = 0.25
specular_factor = 0.35
specular_power = 48.0
roughness_factor = 0.75
alpha_mode = opaque
double_sided = true
unlit = false
```

Run source preflight before compilers for line/column feedback on editor mistakes:

```bash
python3 tools/bsp/validate_trenchbroom_map_source.py maps/src/test_room.map
```

The compile wrappers run this automatically unless `--skip-source-preflight` is passed.

## Runtime brush entities and target routing

`func_wall` imports as a visible, solid static brush entity. `func_illusionary` imports as visible brush
geometry without authoritative collision. `func_door` and `func_button` preserve BSP brush model ownership
so the server can move their collision overlays and replicate presentation transforms without mutating the
immutable `LevelAsset`.

Entity targeting uses the classic BSP contract: `classname` selects the entity type, `targetname` is the
entity's targetable Name, and `target` points to another entity's `targetname`. Multiple entities may
share a `targetname` for group firing.

Supported door keys are `targetname`, `angle`, `angles`, `speed`, `wait`, `lip`, `dmg`, `spawnflags`, and
`stellar.collision`. Doors open when their `targetname` is fired by a trigger/button. Supported button
output keys are `target` and `delay`; buttons press along their movement direction and may fire `target`.
`angle = -1` moves up, `angle = -2` moves down, and other angles move in the X/Y plane. Missing targets
are server diagnostics, not fatal import/runtime errors.

Trigger entities (`trigger_stellar`, `trigger_multiple`, and `trigger_once`) may set `target` and `delay`
to fire named brush movers through the server-owned router. Clients receive resulting transforms through
authoritative snapshots only; client rendering remains presentation-only with no prediction or gameplay
authority.

## Entity key reference

| Purpose | Class/key | Required keys | Optional keys | Runtime semantics |
| --- | --- | --- | --- | --- |
| World setup | `worldspawn` | none | `message`, `wad`, `_light` | Defines world entity data for BSP compilers/import. No runtime marker is emitted. |
| Player spawn | `info_player_start` | `origin` | `targetname`, `angle`, `angles` | Creates a player spawn marker. |
| Deathmatch/player spawn | `info_player_deathmatch` | `origin` | `targetname`, `angle`, `angles` | Creates the same player-spawn marker type as `info_player_start`; use for multiplayer/deathmatch spawn pads or maps that only provide DM starts. |
| Generic spawn | `info_stellar_spawn` | `targetname`, `archetype`, `origin` | `stellar.script`, `stellar.table` | Creates metadata for server-side spawn logic; no entity is spawned during import. |
| Compile-time lights | `light`, `light_spot`, `light_environment` | `targetname`, `origin`, light color/intensity keys for the compiler profile | `_light`, `light`, `style`, `pattern`, `spawnflags`, `angle`, `angles`, `pitch`, `_cone`, `_cone2`, `target` on `light_spot` only | Light entities are for BSP compile/lightmap generation. Import safely ignores them as non-runtime metadata and does not create dynamic runtime lights. |
| Brush trigger volume | `trigger_stellar`, `trigger_multiple`, `trigger_once` | `targetname`, `model="*N"` | `target`, `delay`, `stellar.script`, `stellar.table`, `stellar.once` | Creates a trigger marker from brush model bounds. Script ids are import-time metadata until authoritative runtime invokes them. |
| Point trigger volume | `trigger_stellar_point`, `trigger_multiple_point`, `trigger_once_point` | `targetname`, `origin`, `stellar.extents` | `target`, `delay`, `stellar.script`, `stellar.table`, `stellar.once` | Creates the same trigger marker type as brush triggers, using authored origin plus half-extents. |
| Sprite billboard | `stellar_sprite`, `env_sprite`, or any entity with `stellar.sprite` | `targetname`, `origin`, `stellar.sprite` | `stellar.texture`, `stellar.size`, `stellar.alpha` | Creates a sprite marker for presentation. `stellar.script` is unsupported and ignored with a diagnostic. |
| Brush object-collider sensor | `stellar_object_collider` or `stellar.collider=object` | `targetname`, `model="*N"`, `stellar.collider=object` | `archetype`, `stellar.script`, `stellar.table`, `stellar.enabled` | Creates a server-side sensor marker from brush model bounds. It is not a rigid body and does not block movement. Pickup/item archetypes collect once. |
| Point object-collider sensor | `stellar_object_collider_point` | `targetname`, `origin`, `stellar.extents`, `stellar.collider=object` | `archetype`, `stellar.script`, `stellar.table`, `stellar.enabled` | Creates the same object-collider marker type as brush sensors, using authored origin plus half-extents. |
| Static brush classes | `func_wall`, `func_illusionary`, `func_detail` | `model="*N"` | `targetname`, `archetype`, `stellar.collision=static|sensor|none` | Imported as brush/static metadata; `func_detail` is primarily compile-time detail geometry. Runtime moving/physics behavior is not implied. |
| Door brush entity | `func_door` | `model="*N"`, `targetname` where fired | `archetype`, `angle`, `angles`, `speed`, `wait`, `lip`, `dmg`, `spawnflags`, `stellar.script`, `stellar.table`, `stellar.collision` | Imported with brush model ownership so the authoritative server can move collision overlays and replicate presentation transforms. Door output fields are not exposed until implemented. |
| Button brush entity | `func_button` | `model="*N"` | `targetname`, `target`, `delay`, `archetype`, `angle`, `angles`, `speed`, `wait`, `lip`, `spawnflags`, `stellar.script`, `stellar.table`, `stellar.collision` | Pressable brush mover that may fire another entity by `targetname`. |
| Target helpers | `target_stellar_relay`, `info_null` | `targetname` where referenced | `origin`, `target`, `delay` on relay; `origin` on `info_null` | Minimal target-routing metadata. `info_null` is compile-time helper metadata and is ignored by runtime import. |

Raw BSP entity key/value pairs are preserved in `WorldMarker::properties` when raw entity preservation
is enabled, including unsupported keys.

## Value formats

- `origin` and `stellar.extents` are three floats: `"x y z"`.
- `stellar.size` is two floats: `"width height"`.
- Light `_light` commonly uses compiler-compatible `"r g b brightness"` text; plain `light` is
  retained as a compiler compatibility key when a compile path expects it.
- Boolean-like values accept only `1`, `0`, `true`, `false`, `yes`, and `no`.
- Script ids must be asset-relative identifiers or paths. Absolute paths, drive-letter paths, and `..` parent escapes are rejected.
- Dotted key aliases currently supported by the importer include `_stellar_script`, `_stellar_table`,
  `_stellar_extents`, `_stellar_once`, `_stellar_sprite`, `_stellar_texture`, `_stellar_size`,
  `_stellar_alpha`, `_stellar_collider`, `_stellar_enabled`, and `_stellar_collision`. Plain
  underscore FGD names such as `stellar_script` are not importer aliases unless remapped by the
  editor/toolchain before BSP export.

Malformed vectors and booleans produce import diagnostics. Import does not run Lua.

## Runtime script and presentation notes

Authored trigger and object-collider `stellar.script` ids are loaded by the authoritative runtime when a
live client/session starts a script-bound BSP map. Script ids remain asset-relative; absolute paths,
drive-letter paths, and `..` parent escapes remain invalid. The default script source root for live
runtime preparation is the map directory unless an explicit configured script root is provided by the
client/runtime configuration.

Missing script source for a referenced trigger or object-collider script is a runtime preparation
error for script-bound maps. Import still only records metadata and never executes scripts.

Pickup and door/gate presentation is driven by server-owned snapshots and server-approved events.
Collected pickups become inactive in authoritative gameplay state before the client hides them;
scripted doors/gates mirror validated collision-command results into server gameplay metadata before
presentation observes open/closed state. Client scripts, renderer state, audio state, and HUD state do
not own gameplay truth.

The current HUD feedback layer is a client-side presentation cache only. It can count unique
server-approved pickup events and retain bounded recent event messages for future text rendering, but
gameplay systems never read HUD state and reset it for each new map/session.
The current audio feedback layer routes server-approved pickup and door/gate events, plus optional
script-error diagnostics, to presentation one-shot sound ids (`pickup`, `door_open`, `door_close`,
`script_error`) through an abstract sink. Production has a `NoOpAudioRequestSink`; fake sinks and
missing-sound diagnostics are test/sink-contract behavior, not a production miniaudio or local asset
implementation.

## Examples

### Inch-scale gameplay room fixture

The generated `gameplay_room` test fixture is the current Phase 6 smoke-map reference. It preserves
the tiny fixtures used by existing importer and scripting tests, but gives runtime/client integration
a room at gameplay scale:

- 192x192 inch footprint, with `x/y` spanning roughly `-96..96`.
- Floor at `z = 0`, ceiling at `z = 96`, and static triangle collision for floor, walls, and ceiling.
- `info_player_start` at `origin = "0 0 36"` for the default 72 inch capsule.
- Floor/ceiling/walls use Phase 2 procedural developer material aliases such as `dev/grid_32`,
  `dev/grid_64`, and `dev/wall_96`.
- Includes one sprite marker, one object-collider marker reserved for pickup work, and one trigger
  marker reserved for door/gate work.

Display-free validation imports this room, builds `RuntimeWorld`, advances the in-process
single-player authoritative runtime with movement input, verifies room-wall containment, and checks
deterministic snapshots.

### First TrenchBroom room checklist

Use this geometry as the manual editor smoke test before adding scripted interactions:

- X/Y footprint: `-96..96` on both axes.
- Floor plane: `z = 0`.
- Ceiling plane: `z = 96`.
- Player start: `info_player_start` with `origin = "0 0 36"` and an `angle` facing into the room.
- Materials: `dev/grid_32` or `stellar_dev_grid_32` on the floor, `dev/grid_64` or
  `stellar_dev_grid_64` on the ceiling, and `dev/wall_96` or `stellar_dev_wall_96` on walls.
- Compile target: BSP30.
- Required validation: wrapper validation plus `stellar-client --validate-map`.

### Player spawn

```text
classname = info_player_start
targetname = PlayerStart
origin = "0 0 36"
angle = "90"
```

For the default 72 inch player capsule, use an origin such as `"0 0 36"` when the floor is at
`z = 0`. The importer preserves the authored origin; it does not lift player spawns automatically.
`info_player_deathmatch` follows the same Z-up origin policy and maps to the same player-spawn marker
type; use it for deathmatch/multiplayer spawn pads or compatibility with BSP compile/editor workflows
that prefer the traditional DM classname.

### Compile-time lights

```text
classname = light
targetname = KeyLight
origin = "0 0 80"
_light = "255 240 220 300"
style = "0"
```

`light`, `light_spot`, and `light_environment` are compile-time entities for BSP lighting tools. VHLT
`hlrad` writes the BSP lighting lump; Stellar imports those bytes into `LevelLightmap` records, generates
secondary UVs, uploads lightmap textures as linear clamp-to-edge images, and multiplies static surface
base color/texture by the lightmap in OpenGL. Faces with missing or invalid light offsets fall
back to unlit/fullbright material behavior with a warning. Classic nonzero light styles are preserved on
the imported lightmap and currently render with a deterministic static multiplier of `1.0` until a later
server-authoritative gameplay phase explicitly animates styles.

Author `light_spot` and `light_environment` orientation in TrenchBroom degrees: `pitch` `90` points down,
`pitch` `270` or `-90` points up, `pitch` `0` is horizontal, and yaw/`angle` controls horizontal
direction. Classic `light_spot` and `light_environment` ignore roll in `angles "pitch yaw roll"`. If a
`light_spot` has `target`, VHLT may aim it at the target entity and ignore `angle`/`pitch`/`angles`.

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
classname = trigger_stellar_point
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
model = "*3"
stellar.collider = "object"
archetype = "pickup"
stellar.script = "scripts/pickup.lua"
stellar.table = "PickupGem"
stellar.enabled = "yes"
```

Point-authored object collider:

```text
classname = stellar_object_collider_point
targetname = PickupGemPoint
origin = "96 64 32"
stellar.extents = "8 8 8"
stellar.collider = "object"
archetype = "pickup"
stellar.script = "scripts/pickup.lua"
stellar.table = "PickupGem"
stellar.enabled = "yes"
```

Object-collider markers with pickup/item archetypes, such as `archetype = "pickup"` or
`archetype = "item_health"`, are server-authoritative pickups. On enter, the authoritative runtime
emits a native `gameplay.collect_pickup` command, validates that the pickup is still active, disables
the object collider, and marks the gameplay entity inactive for future presentation snapshots. Repeated
overlaps do not collect the same pickup again.

### Static collision blocker

```text
classname = func_wall
targetname = DoorBlocker
model = "*2"
stellar.collision = "static"
```

Scripts may later request validated enable/disable changes by collision mesh name. The BSP import itself does not animate, move, or simulate the brush.
When a sandboxed Lua trigger emits `collision.set_mesh_enabled` for a named gate/door mesh, native
server code validates the mesh name, applies the collision-state change, and mirrors the door/gate
`open` state into server-owned gameplay metadata. Disabling a gate mesh means the blocker is open;
enabling it means closed.

### Door/button runtime brush metadata

```text
classname = func_door
targetname = MainDoor
model = "*4"
target = "DoorOpenedRelay"
angle = "90"
speed = "100"
wait = "4"
lip = "8"
stellar.collision = "static"
stellar.script = "scripts/door.lua"
stellar.table = "MainDoor"
```

`func_door` and `func_button` smart properties map to the implemented server-authoritative door/button
path. Buttons fire their `target`, doors move according to angle/speed/wait/lip metadata, collision is
updated on the server, and clients observe only snapshot-owned presentation transforms.

## Non-Goals Outside The Stellar BSP30 Profile

- Moving brush classes for plats, trains, or rotating entities beyond the implemented `func_door` /
  `func_button` path.
- Client-side gameplay scripting or presentation script execution.
- Sprite script callbacks.
- Renderer-owned or audio-owned gameplay state.
- Dynamic rigid bodies and third-party physics integration.
- Source/VBSP-specific entities and lump formats.

## Validation commands

Map validation is display-free and does not create a window, graphics context, or renderer resources. It
may load safe external WAD3 texture pixels referenced by `worldspawn` `wad`; missing WADs are reported as
actionable diagnostics and fallback material behavior remains deterministic. Use either client validation form:

```bash
stellar-client --validate-config --map path/to/map.bsp
stellar-client --validate-map path/to/map.bsp
```

Both commands import the BSP, build the source-neutral runtime world, and fail for fatal validation errors such as unsupported BSP versions, malformed lump bounds, invalid model/face references that prevent import, or rejected script path escapes. Warning diagnostics do not fail validation by default.

Common diagnostics:

- `kLumpOutOfBounds` / `kMalformedLumpSize`: BSP binary structure is corrupt or uses unexpected lump sizes.
- `kInvalidFaceReference` / `kDegenerateFacePolygon`: a face cannot resolve enough valid edges/vertices to form a polygon; affected faces are skipped.
- `kInvalidVisibilityData`: PVS offsets, decompression rows, or marksurface references are invalid; visibility falls back to all surfaces.
- `kMissingTexture` / `kMaterialFallbackUsed`: texture data is external, missing, or blocked by safe-path
  rules; validation uses deterministic fallback materials when WAD pixels cannot be resolved.
- `kMaterialSidecarLoaded`: a `.stellar_material` sidecar was resolved for a BSP texture name.
- `kMaterialSidecarInvalid` / `kMaterialSidecarUnknownKey`: a sidecar contains malformed values or
  unrecognized keys; strict sidecar mode upgrades unknown keys to failures.
- `kMaterialSidecarUnsafePath`: a sidecar texture reference used an absolute path, drive-letter path,
  empty segment, or `..` escape.
- `kMaterialSidecarTextureMissing`: a sidecar texture reference could not be decoded or found.
- `kInvalidLightingData`: a face light offset or inferred lightmap byte range is outside the lighting lump; the face falls back to unlit material behavior.
- `kLightmapStats` / `kAllBlackLightmap`: imported lighting diagnostics report raw lighting byte count,
  lightmap count, per-lightmap dimensions/RGB stats, all-black data, and material/lightmap bindings.
- `kMissingPlayerSpawn`: no `info_player_start` or `info_player_deathmatch` marker exists; import succeeds but gameplay startup may need an explicit spawn policy.
- `kUnsupportedEntityKey`: malformed authoring values such as `origin`, `stellar.extents`, `stellar.size`, `stellar.once`, or `stellar.enabled` were ignored.
- `kScriptPathEscape`: script ids are absolute, drive-letter based, or contain `..`; validation fails to preserve Lua sandbox boundaries.
- `kNoCollisionTriangles`: imported collision extraction produced no triangles; this is currently a warning for maps or tests with an explicit no-collision policy.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_validation|bsp_importer|client_map_validation_smoke|client_cli_map_validation|bsp_authoring_smoke)$' --output-on-failure
tools/bsp/validate_trenchbroom_bsp30.sh build/tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp
tools/bsp/validate_trenchbroom_bsp30.sh build/tests/fixtures/trenchbroom/compiled/entity_matrix_zup.bsp
tools/bsp/validate_trenchbroom_bsp30.sh build/tests/fixtures/trenchbroom/compiled/scripted_interaction_zup.bsp
```

Run the VHLT fixture matrix when VHLT tools are installed locally or provided through
`STELLAR_VHLT_DIR`:

```bash
tools/bsp/run_vhlt_fixture_matrix.sh --source-root . --build-root build --profile full
```

The matrix compiles positive TrenchBroom fixtures through VHLT, validates the produced BSP30 files with
Stellar client/server validators, and verifies that the invalid script escape fixture compiles but fails
Stellar validation for the expected sandbox diagnostic.

For final fixture coverage and CI grouping, use:

```bash
ctest --test-dir build -R 'trenchbroom|bsp_lightmaps|brush_mover|bsp_authoring|client_cli|server_cli' --output-on-failure
```

This includes source preflight, generated BSP fixtures for lightmaps/materials/point volumes/brush
movers, copied-package path smoke, optional VHLT matrix coverage with skip code `77`, and client/server
CLI validation. The full manual editor checklist is in `docs/TrenchBroomManualQA.md`.

For broad confidence, also run:

```bash
ctest --test-dir build --output-on-failure
```
