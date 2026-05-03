# Project Stellar — TrenchBroom/VHLT Spotlight Angle Compatibility Plan

Target branch: `fixes`

## Purpose

Fix the remaining spotlight orientation mismatch discovered during manual testing:

- In TrenchBroom, a `light_spot` pitch of `270` points directly upward.
- The current VHLT/GoldSrc compile path effectively treats that direction as downward.
- Yaw appears to do nothing in some cases.
- Roll appears to do nothing.

This plan is optimized for AI implementation agents. Do not commit unless explicitly instructed by the user.

---

## Executive summary

Fix this in the **VHLT map-prep stage**, not in the runtime renderer.

Reason:
- Stellar runtime renders baked lightmaps.
- `light_spot`, `light_environment`, yaw, pitch, cone angles, and target direction are consumed by `hlrad` during compile.
- Runtime has no opportunity to reinterpret spotlight pitch after lightmaps are baked.
- Therefore, the source `.map` copied into the VHLT work directory must be normalized from TrenchBroom’s editor-facing angle convention to the convention expected by VHLT/GoldSrc-style tools.

The current VHLT wrapper already preprocesses the copied work map:
- rewrites developer texture names,
- optionally rewrites Valve 220 axes,
- injects `mapversion`,
- injects `wad`,
- injects `_stellar_lighting_mode`,
- then runs `hlcsg`, `hlbsp`, `hlvis`, and `hlrad` for Full profiles.

Add spotlight/environment-light orientation normalization to this same preprocessing pipeline before `hlcsg`.

---

## Current repo evidence

Relevant files:

- `tools/bsp/compile_vhlt_bsp30.sh`
- `tools/trenchbroom/Stellar/stellar_entities.fgd`
- `tools/bsp/stellar_entities.fgd`
- `tools/bsp/validate_trenchbroom_map_source.py`
- `tests/import/bsp/FgdContract.cpp`
- `docs/TrenchBroom.md`
- `docs/BspAuthoring.md`

Observed FGD state:

```fgd
@BaseClass = Angles
[
    angle(string) : "Yaw angle around +Z" : "0"
    angles(string) : "Pitch yaw roll, degrees" : "0 0 0"
]

@PointClass base(LightKeys, Angles) ... = light_spot
[
    pitch(string) : "Spot pitch in degrees" : "-90"
    _cone(string) : "Inner cone angle" : "30"
    _cone2(string) : "Outer cone angle" : "45"
]

@PointClass base(LightKeys, Angles) ... = light_environment
[
    pitch(string) : "Sun pitch in degrees" : "-45"
]
```

Problem:
- The FGD exposes both `angles` and separate `angle`/`pitch`.
- TrenchBroom commonly edits direction as `angles "pitch yaw roll"`.
- VHLT/GoldSrc-style tools commonly expect spotlight direction from `angle` yaw and `pitch`, or a target direction.
- Current wrapper does not normalize these fields before VHLT compile.

---

## Desired behavior

### Pitch

Use TrenchBroom-facing pitch as the source of truth and convert it for the compiler.

Observed/manual target:

| TrenchBroom pitch | TrenchBroom meaning | Compiler/VHLT pitch to write |
|---:|---|---:|
| `270` | directly upward | `90` |
| `-90` | directly upward | `90` |
| `90` | directly downward | `-90` |
| `0` | horizontal | `0` |
| `45` | downward-ish | `-45` |
| `315` / `-45` | upward-ish | `45` |

Algorithm:

```python
def normalize_signed(degrees: float) -> float:
    value = degrees % 360.0
    if value > 180.0:
        value -= 360.0
    return value

compiler_pitch = -normalize_signed(editor_pitch)
```

### Yaw

Yaw should generally be preserved, normalized, and written to `angle`.

Recommended:
- Read editor yaw from `angles "pitch yaw roll"` if present.
- Otherwise read from `angle`.
- Write compiler yaw to:
  - `"angle" "<yaw>"`
  - normalized `"angles" "<compiler_pitch> <yaw> 0"`

Do not flip yaw unless a manual horizontal fixture proves yaw is mirrored.

### Roll

Roll should be treated as ignored for classic `light_spot` / `light_environment`.

Reason:
- Classic spotlights have a circular cone.
- A circular cone has no roll-dependent visual orientation.
- GoldSrc/VHLT-style spotlight compile behavior is expected to use yaw + pitch, or a target vector.
- Roll only becomes meaningful later if Stellar adds projected textures/gobos or non-circular spotlight patterns.

Recommended:
- Preserve nothing from roll during VHLT normalization.
- Normalize compiler `angles` roll to `0`.
- Add docs: roll is ignored for compile-time classic spotlights.

### Targeted spotlights

If `light_spot` has `target`, the compiler may aim the spotlight at the target entity and ignore `angle`/`pitch`/`angles`.

Recommended:
- Do not rewrite `target`.
- Still normalize fields for consistency.
- Preflight should warn:
  - `light_spot has target; compiler may ignore angle/pitch/angles and aim at target`
- Manual target fixtures should verify actual VHLT behavior.

---

## Implementation phase 1 — Add a standalone normalizer script

Create:

```text
tools/bsp/normalize_vhlt_light_angles.py
```

Usage:

```bash
python3 tools/bsp/normalize_vhlt_light_angles.py path/to/work.map
```

Optional flags:

```bash
--dry-run
--json
--no-marker
```

### Responsibilities

The script should:
- parse top-level entity blocks;
- operate only on `classname`:
  - `light_spot`
  - `light_environment`
- leave `light` unchanged;
- leave non-light entities unchanged;
- convert TrenchBroom/editor pitch to compiler pitch;
- translate `angles` into compiler-compatible `angle` + `pitch`;
- preserve `target` and warn when target may override angles;
- be idempotent.

### Property precedence

For each `light_spot` / `light_environment` entity:

1. If `angles` exists and parses as three floats:
   - editor pitch = first value
   - editor yaw = second value
   - editor roll = third value, ignored
2. Else:
   - editor pitch = `pitch` if present, else class default
   - editor yaw = `angle` if present, else `0`
3. Defaults after FGD update:
   - `light_spot` editor pitch default: `90` (downward)
   - `light_environment` editor pitch default: `45` if intended downward sun
4. Normalize:
   - compiler pitch = `-normalize_signed(editor_pitch)`
   - compiler yaw = normalize to `[0, 360)` or preserve equivalent
   - roll = `0`
5. Write/update:
   - `"pitch" "<compiler_pitch>"`
   - `"angle" "<compiler_yaw>"`
   - `"angles" "<compiler_pitch> <compiler_yaw> 0"`

### Stable numeric formatting

```python
def fmt(value: float) -> str:
    if abs(value - round(value)) < 1e-6:
        return str(int(round(value)))
    return f"{value:.6f}".rstrip("0").rstrip(".")
```

### Idempotency

Add an internal marker after normalization:

```map
"_stellar_vhlt_angles_normalized" "1"
```

If marker exists, skip that entity.

Notes:
- Marker is inserted after source preflight, so it will not affect authored map validation.
- It may remain in the entity lump. That is acceptable, but can be stripped later if desired.
- If avoiding marker metadata is preferred, make the wrapper call normalizer exactly once and rely on tests. Marker is safer.

---

## Implementation phase 2 — Call normalizer from VHLT wrapper

File:

```text
tools/bsp/compile_vhlt_bsp30.sh
```

Add function:

```bash
rewrite_vhlt_light_orientations() {
    local map_path="$1"
    python3 "$root/tools/bsp/normalize_vhlt_light_angles.py" "$map_path"
}
```

Recommended call order:

```bash
python3 "$root/tools/bsp/create_stellar_dev_wad.py" --out "$work_dir/stellar_dev.wad"
rewrite_vhlt_texture_names_only "$work_map"
rewrite_vhlt_light_orientations "$work_map"

if [[ "$classic_texture_axes" == "1" ]]; then
    rewrite_valve220_to_classic_texture_axes "$work_map"
fi

inject_mapversion_key "$work_map"
inject_wad_key "$work_map" "$work_dir/stellar_dev.wad"
...
```

Rationale:
- Run after source map copy and texture-name rewrite.
- Run before `hlcsg`, because `hlrad` consumes baked direction after compile chain.
- Keep source map untouched; only modify copied VHLT work map.

---

## Implementation phase 3 — Update FGD defaults and descriptions

Files:

```text
tools/trenchbroom/Stellar/stellar_entities.fgd
tools/bsp/stellar_entities.fgd
```

### `light_spot`

Change:

```fgd
pitch(string) : "Spot pitch in degrees" : "-90"
```

To:

```fgd
pitch(string) : "Spot pitch in TrenchBroom degrees; 90 points down, 270/-90 points up" : "90"
```

Reason:
- A ceiling spotlight should default downward.
- Manual testing says TrenchBroom pitch `270` points up.

### `light_environment`

Change:

```fgd
pitch(string) : "Sun pitch in degrees" : "-45"
```

To:

```fgd
pitch(string) : "Sun pitch in TrenchBroom degrees; 45 points downward, 315/-45 upward" : "45"
```

Only do this if the intended default is a downward sun. If intended default was upward, adjust accordingly.

### `Angles` base docs

Change descriptions to editor-facing wording:

```fgd
angle(string) : "Yaw angle around +Z, TrenchBroom/editor convention" : "0"
angles(string) : "Pitch yaw roll in TrenchBroom/editor degrees; roll ignored by classic lights" : "0 0 0"
```

Keep FGD syntax TrenchBroom-compatible:
- `spawnflags(flags) =`
- no description before `=` on flags fields.

---

## Implementation phase 4 — Update source preflight

File:

```text
tools/bsp/validate_trenchbroom_map_source.py
```

Add `stellar_global_light` to `KNOWN_CLASSES` only if not already present from recent lighting fixes.

Add warnings/errors for light orientation:

### Errors

- `light_spot` / `light_environment` `angles` cannot parse three finite floats.
- `angle` cannot parse finite float.
- `pitch` cannot parse finite float.

### Warnings

- Both `angles` and separate `pitch`/`angle` exist:
  - warn that `angles` takes precedence during VHLT normalization.
- Nonzero roll in `angles`:
  - warn that roll is ignored by classic `light_spot` / `light_environment`.
- `light_spot` has `target` and also angle/pitch/angles:
  - warn that VHLT may aim at `target` and ignore orientation fields.

Do not reject valid TrenchBroom pitch values like:
- `pitch "270"`
- `pitch "-90"`
- `angles "270 45 0"`

---

## Implementation phase 5 — Add tests

### New script tests

Create:

```text
tools/bsp/test_normalize_vhlt_light_angles.sh
```

or add Python unit tests if the project has a Python test convention.

#### Case 1: `angles "270 45 0"`

Input:

```map
{
"classname" "light_spot"
"origin" "0 0 64"
"angles" "270 45 0"
"_light" "255 255 255 200"
}
```

Expected:

```map
"pitch" "90"
"angle" "45"
"angles" "90 45 0"
```

#### Case 2: `angles "90 45 0"`

Expected:

```map
"pitch" "-90"
"angle" "45"
"angles" "-90 45 0"
```

#### Case 3: separate keys

Input:

```map
"pitch" "-90"
"angle" "180"
```

Expected:

```map
"pitch" "90"
"angle" "180"
"angles" "90 180 0"
```

#### Case 4: `light_environment`

Input:

```map
"classname" "light_environment"
"pitch" "45"
"angle" "270"
```

Expected:

```map
"pitch" "-45"
"angle" "270"
"angles" "-45 270 0"
```

#### Case 5: non-light unchanged

`info_player_start`, `light`, `func_door` unchanged.

#### Case 6: idempotency

Run normalizer twice. Second output is identical.

#### Case 7: target warning

`light_spot` with `target` exits 0 and emits warning diagnostic.

### VHLT wrapper test

Extend or add a shell smoke test:
- run wrapper with `STELLAR_VHLT_KEEP_WORK=1` using fake VHLT tools or skip mode;
- confirm preserved work map includes normalized light fields before compile.

---

## Implementation phase 6 — Add manual fixture maps

Add under:

```text
tests/fixtures/trenchbroom/src/
```

Recommended fixtures:

1. `spotlight_pitch_down_zup.map`
   - sealed room;
   - ceiling `light_spot`;
   - TrenchBroom pitch `90`;
   - expected: floor lit.

2. `spotlight_pitch_up_zup.map`
   - sealed room;
   - floor `light_spot`;
   - TrenchBroom pitch `270` or `-90`;
   - expected: ceiling lit.

3. `spotlight_yaw_walls_zup.map`
   - horizontal spotlights:
     - yaw `0`
     - yaw `90`
     - yaw `180`
     - yaw `270`
   - expected: corresponding wall lit.
   - This fixture decides whether yaw needs flipping. Do not flip yaw before this evidence.

4. `spotlight_targeted_zup.map`
   - `light_spot` with `target`;
   - target above/below/side;
   - expected: compiler aims toward target or documented VHLT-specific behavior.

5. `light_environment_pitch_zup.map`
   - sun/environment pitch fixture;
   - verifies `45` downward and `315/-45` upward behavior.

---

## Implementation phase 7 — Documentation

Update:

```text
docs/TrenchBroom.md
docs/BspAuthoring.md
tools/trenchbroom/Stellar/README.md
```

Add:

```text
Spotlight orientation:
- pitch 90 points down
- pitch 270 or -90 points up
- pitch 0 is horizontal
- yaw controls horizontal direction
- roll is ignored by classic light_spot/light_environment
```

Add compile note:

```text
The VHLT compile wrapper converts TrenchBroom/editor-facing pitch to VHLT/GoldSrc pitch on the copied work map before hlrad. Authored source maps are not modified.
```

Add target note:

```text
If a light_spot has target, VHLT may aim the spotlight at the target and ignore angle/pitch/angles.
```

---

## Manual validation commands

Compile with preserved work map:

```bash
STELLAR_VHLT_KEEP_WORK=1 tools/bsp/compile_trenchbroom_bsp30.sh   --map tests/fixtures/trenchbroom/src/spotlight_pitch_down_zup.map   --out maps/compiled/spotlight_pitch_down_zup.bsp   --profile full   --toolchain vhlt
```

Inspect work map:

```bash
grep -nE '"classname" "light_spot"|"pitch"|"angle"|"angles"|"_stellar_vhlt_angles_normalized"'   build/tests/fixtures/trenchbroom/vhlt/work/spotlight_pitch_down_zup/*.map
```

Run:

```bash
build/stellar-client --renderer opengl   --map maps/compiled/spotlight_pitch_down_zup.bsp
```

Repeat for:
- `spotlight_pitch_up_zup`
- `spotlight_yaw_walls_zup`
- `spotlight_targeted_zup`
- `light_environment_pitch_zup`

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
ctest --test-dir build -R 'trenchbroom|vhlt|fgd|bsp_light|map_source' --output-on-failure
```

Required:
- angle normalizer tests pass;
- texture-axis preservation test still passes;
- FGD contract passes;
- source preflight tests pass;
- VHLT wrapper syntax test passes.

### Manual

- `light_spot` pitch `90` in TrenchBroom lights downward.
- `light_spot` pitch `270` / `-90` lights upward.
- Yaw values rotate horizontal spotlights to expected walls.
- Roll has no visible effect and is documented.
- Targeted spotlights behave as documented based on actual VHLT output.
- Existing lighting behavior remains fixed.

---

## Do not do

- Do not alter runtime baked-light rendering for this issue.
- Do not flip yaw unless fixture evidence proves yaw is mirrored.
- Do not implement roll behavior for classic circular spotlights.
- Do not change `_cone` / `_cone2` semantics.
- Do not rewrite non-light entities.
- Do not modify user-authored source maps in place.
- Do not remove `angles`; normalize and preserve compiler-compatible `angle`/`pitch` keys.
