#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: tools/bsp/compile_vhlt_bsp30.sh --map path/to/source.map --out path/to/output.bsp --profile fast|full|validate-only [options]
       tools/bsp/compile_vhlt_bsp30.sh --bsp path/to/output.bsp --profile validate-only [options]

Options:
  --allow-skip              Exit 77 when required external tools are unavailable.
  --no-stellar-validation   Skip tools/bsp/validate_trenchbroom_bsp30.sh after BSP30 header validation.

Environment overrides:
  STELLAR_VHLT_DIR          Directory containing hlcsg, hlbsp, hlvis, hlrad, and optional ripent.
  HLCSG, HLBSP, HLVIS,
  HLRAD, RIPENT             Explicit per-tool paths.
  STELLAR_CLIENT            Path to stellar-client for post-compile validation.
  STELLAR_SERVER            Path to stellar-server for post-compile validation.
  STELLAR_VHLT_KEEP_WORK=1  Preserve the transient work directory.
  STELLAR_VHLT_LOG_DIR      Directory for copied VHLT logs and tool outputs.
USAGE
}

fail() {
    printf 'compile_vhlt_bsp30.sh: %s\n' "$1" >&2
    exit 1
}

skip_or_fail() {
    if [[ "$allow_skip" == "1" ]]; then
        printf 'compile_vhlt_bsp30.sh: skipping: %s\n' "$1" >&2
        exit 77
    fi
    fail "$1"
}

repo_root() {
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    cd "$script_dir/../.." && pwd
}

absolute_path() {
    local path="$1"
    local dir
    local base
    dir="$(dirname "$path")"
    base="$(basename "$path")"
    mkdir -p "$dir"
    dir="$(cd "$dir" && pwd)"
    printf '%s/%s\n' "$dir" "$base"
}

find_tool() {
    local tool_name="$1"
    local override_value="$2"
    local required="$3"
    local candidate

    if [[ -n "$override_value" ]]; then
        if [[ -x "$override_value" ]]; then
            absolute_path "$override_value"
            return 0
        fi
        if [[ "$required" == "1" ]]; then
            skip_or_fail "$tool_name override is not executable: $override_value"
        fi
        return 1
    fi

    if [[ -n "${STELLAR_VHLT_DIR:-}" ]]; then
        candidate="$STELLAR_VHLT_DIR/$tool_name"
        if [[ -x "$candidate" ]]; then
            absolute_path "$candidate"
            return 0
        fi
    fi

    local root="$4"
    for candidate in \
        "$root/tools/bsp/$tool_name" \
        "$root/tools/bsp/vhlt/$tool_name" \
        "$root/tools/bsp/bin/$tool_name"; do
        if [[ -x "$candidate" ]]; then
            absolute_path "$candidate"
            return 0
        fi
    done

    if command -v "$tool_name" >/dev/null 2>&1; then
        command -v "$tool_name"
        return 0
    fi

    if [[ "$required" == "1" ]]; then
        skip_or_fail "$tool_name not found or not executable. Set STELLAR_VHLT_DIR or $tool_name override."
    fi
    return 1
}

read_bsp_version() {
    local bsp="$1"
    od -An -td4 -N4 "$bsp" | tr -d '[:space:]'
}

normalize_bsp_entity_lump() {
    local bsp_path="$1"
    python3 - "$bsp_path" <<'PY'
from pathlib import Path
import struct
import sys

bsp_path = Path(sys.argv[1])
data = bytearray(bsp_path.read_bytes())
if len(data) < 12:
    raise SystemExit("BSP is too small")
version = struct.unpack_from("<i", data, 0)[0]
if version != 30:
    raise SystemExit(f"expected BSP30 before entity normalization, found {version}")
entity_offset, entity_length = struct.unpack_from("<ii", data, 4)
if entity_offset < 0 or entity_length < 0 or entity_offset + entity_length > len(data):
    raise SystemExit("invalid BSP entity lump bounds")
end = entity_offset + entity_length
while entity_length > 0 and data[end - 1] == 0:
    entity_length -= 1
    end -= 1
struct.pack_into("<ii", data, 4, entity_offset, entity_length)
bsp_path.write_bytes(data)
PY
}

copy_logs() {
    local work_dir="$1"
    local log_dir="$2"
    mkdir -p "$log_dir"

    local path
    shopt -s nullglob
    for path in \
        "$work_dir"/*.log \
        "$work_dir"/*.err \
        "$work_dir"/*.p0 \
        "$work_dir"/*.p1 \
        "$work_dir"/*.p2 \
        "$work_dir"/*.p3 \
        "$work_dir"/*.prt \
        "$work_dir"/*.lin \
        "$work_dir"/*.ent \
        "$work_dir"/*.bsp \
        "$work_dir"/*.map \
        "$work_dir"/stellar_dev.wad; do
        cp -f "$path" "$log_dir/"
    done
    shopt -u nullglob
}

inject_wad_key() {
    local map_path="$1"
    local wad_path="$2"
    python3 - "$map_path" "$wad_path" <<'PY'
from pathlib import Path
import re
import sys

map_path = Path(sys.argv[1])
wad_path = sys.argv[2]
lines = map_path.read_text(encoding="utf-8").splitlines(keepends=True)

depth = 0
world_start = None
world_end = None
classname_at_depth_one = False
for i, line in enumerate(lines):
    stripped = line.strip()
    if stripped == "{":
        depth += 1
        if depth == 1 and world_start is None:
            world_start = i
        continue
    if stripped == "}":
        if depth == 1 and classname_at_depth_one and world_end is None:
            world_end = i
            break
        depth -= 1
        if depth == 0 and world_start is not None and world_end is None:
            world_start = None
        continue
    if depth == 1 and re.match(r'^"classname"\s+"worldspawn"\s*$', stripped):
        classname_at_depth_one = True

if world_start is None or world_end is None:
    raise SystemExit("worldspawn entity not found")

wad_line = f'"wad" "{wad_path}"\n'
for i in range(world_start + 1, world_end):
    if re.match(r'^\s*"wad"\s+".*"\s*$', lines[i].strip()):
        lines[i] = wad_line
        break
else:
    insert_at = world_start + 1
    for i in range(world_start + 1, world_end):
        if re.match(r'^\s*"classname"\s+"worldspawn"\s*$', lines[i].strip()):
            insert_at = i + 1
            break
    lines.insert(insert_at, wad_line)

map_path.write_text("".join(lines), encoding="utf-8")
PY
}

rewrite_vhlt_texture_aliases() {
    local map_path="$1"
    python3 - "$map_path" <<'PY'
from pathlib import Path
import re
import sys

map_path = Path(sys.argv[1])
lines = map_path.read_text(encoding="utf-8").splitlines(keepends=True)
converted = []
face = re.compile(
    r'^(\s*\([^)]*\)\s*\([^)]*\)\s*\([^)]*\)\s+)'
    r'(\S+)\s+\[[^\]]*\]\s+\[[^\]]*\]\s+'
    r'([-+]?\d+(?:\.\d+)?)\s+([-+]?\d+(?:\.\d+)?)\s+([-+]?\d+(?:\.\d+)?)(\s*(?://.*)?\n?)$'
)
for line in lines:
    for name in ("grid_12", "grid_16", "grid_32", "grid_64", "player_72", "wall_96"):
        line = line.replace(f"dev/{name}", f"dev_{name}")
    match = face.match(line)
    if match:
        prefix, texture, rotation, scale_x, scale_y, suffix = match.groups()
        line = f"{prefix}{texture} 0 0 {rotation} {scale_x} {scale_y}{suffix}"
    converted.append(line)
map_path.write_text("".join(converted), encoding="utf-8")
PY
}

run_tool() {
    local tool_path="$1"
    local log_path="$2"
    shift 2
    printf 'Running %s %s\n' "$tool_path" "$*"
    "$tool_path" "$@" >"$log_path" 2>&1
}

map_path=""
out_path=""
profile=""
allow_skip="0"
stellar_validation="1"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --map)
            [[ $# -ge 2 ]] || fail "--map requires a path"
            map_path="$2"
            shift 2
            ;;
        --out|--bsp)
            [[ $# -ge 2 ]] || fail "$1 requires a path"
            out_path="$2"
            shift 2
            ;;
        --profile)
            [[ $# -ge 2 ]] || fail "--profile requires fast, full, or validate-only"
            profile="$2"
            shift 2
            ;;
        --allow-skip)
            allow_skip="1"
            shift
            ;;
        --no-stellar-validation)
            stellar_validation="0"
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            fail "unknown argument: $1"
            ;;
    esac
done

[[ -n "$profile" ]] || fail "--profile is required"
[[ "$profile" == "fast" || "$profile" == "full" || "$profile" == "validate-only" ]] || fail "unsupported profile: $profile"
[[ -n "$out_path" ]] || fail "--out is required"

root="$(repo_root)"
validate_script="$root/tools/bsp/validate_trenchbroom_bsp30.sh"
[[ -f "$validate_script" ]] || fail "validation wrapper missing: $validate_script"

fixture="$(basename "$out_path")"
fixture="${fixture%.bsp}"
log_dir="${STELLAR_VHLT_LOG_DIR:-$root/build/tests/fixtures/trenchbroom/vhlt/logs/$fixture}"

if [[ "$profile" != "validate-only" ]]; then
    [[ -n "$map_path" ]] || fail "--map is required for compile profiles"
    [[ -f "$map_path" ]] || fail "MAP does not exist: $map_path"
    [[ "$map_path" == *.map ]] || fail "input must be a .map file: $map_path"

    hlcsg="$(find_tool hlcsg "${HLCSG:-}" 1 "$root")"
    hlbsp="$(find_tool hlbsp "${HLBSP:-}" 1 "$root")"
    if [[ "$profile" == "full" ]]; then
        hlvis="$(find_tool hlvis "${HLVIS:-}" 1 "$root")"
        hlrad="$(find_tool hlrad "${HLRAD:-}" 1 "$root")"
    else
        hlvis="$(find_tool hlvis "${HLVIS:-}" 0 "$root" || true)"
        hlrad="$(find_tool hlrad "${HLRAD:-}" 0 "$root" || true)"
    fi
    ripent="$(find_tool ripent "${RIPENT:-}" 0 "$root" || true)"
    if [[ -z "$ripent" ]]; then
        printf 'compile_vhlt_bsp30.sh: optional ripent not found; continuing without entity patching.\n' >&2
    fi

    work_dir="$root/build/tests/fixtures/trenchbroom/vhlt/work/$fixture"
    if [[ "${STELLAR_VHLT_KEEP_WORK:-0}" != "1" ]]; then
        rm -rf "$work_dir"
    fi
    mkdir -p "$work_dir"

    base="$(basename "$map_path" .map)"
    work_map="$work_dir/$base.map"
    cp "$map_path" "$work_map"

    python3 "$root/tools/bsp/create_stellar_dev_wad.py" --out "$work_dir/stellar_dev.wad"
    rewrite_vhlt_texture_aliases "$work_map"
    inject_wad_key "$work_map" "$work_dir/stellar_dev.wad"

    mkdir -p "$(dirname "$out_path")"
    mkdir -p "$log_dir"

    (
        cd "$work_dir"
        run_tool "$hlcsg" "$log_dir/hlcsg.log" "$base.map"
        run_tool "$hlbsp" "$log_dir/hlbsp.log" "$base"
        if [[ "$profile" == "full" ]]; then
            run_tool "$hlvis" "$log_dir/hlvis.log" "$base"
            run_tool "$hlrad" "$log_dir/hlrad.log" "$base"
        fi
    )

    [[ -f "$work_dir/$base.bsp" ]] || fail "VHLT did not create BSP output: $work_dir/$base.bsp"
    [[ -s "$work_dir/$base.bsp" ]] || fail "VHLT created an empty BSP output: $work_dir/$base.bsp"
    cp -f "$work_dir/$base.bsp" "$out_path"
    normalize_bsp_entity_lump "$out_path"
    copy_logs "$work_dir" "$log_dir"

    if [[ "${STELLAR_VHLT_KEEP_WORK:-0}" != "1" ]]; then
        rm -rf "$work_dir"
    fi
else
    [[ -f "$out_path" ]] || fail "validate-only BSP does not exist: $out_path"
fi

[[ -f "$out_path" ]] || fail "BSP output does not exist: $out_path"
[[ -s "$out_path" ]] || fail "BSP output is empty: $out_path"

version="$(read_bsp_version "$out_path")"
[[ "$version" == "30" ]] || fail "expected BSP30 version 30, found '${version:-unreadable}' in $out_path"

if [[ "$stellar_validation" == "1" ]]; then
    bash "$validate_script" --map "$out_path"
fi

printf 'VHLT BSP30 output: %s\n' "$out_path"
printf 'VHLT log directory: %s\n' "$log_dir"
