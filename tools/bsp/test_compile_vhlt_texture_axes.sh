#!/usr/bin/env bash
set -euo pipefail

source_root="${1:-}"
build_root="${2:-}"

if [[ -z "$source_root" ]]; then
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    source_root="$(cd "$script_dir/../.." && pwd)"
fi
if [[ -z "$build_root" ]]; then
    build_root="$source_root/build"
fi

tmp_dir="$build_root/tests/tools/vhlt_texture_axes"
fake_vhlt_dir="$tmp_dir/vhlt"
out_dir="$tmp_dir/out"
log_dir="$tmp_dir/logs"
work_root="$tmp_dir/work"
rm -rf "$tmp_dir"
mkdir -p "$fake_vhlt_dir" "$out_dir" "$log_dir" "$work_root"

cat >"$fake_vhlt_dir/hlcsg" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
test -f "$1"
SH

cat >"$fake_vhlt_dir/hlbsp" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
python3 - "$1.bsp" <<'PY'
from pathlib import Path
import struct
import sys

Path(sys.argv[1]).write_bytes(struct.pack("<iii", 30, 12, 0))
PY
SH

cat >"$fake_vhlt_dir/hlvis" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
test -n "$1"
SH

cat >"$fake_vhlt_dir/hlrad" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
test -n "$1"
SH

chmod +x "$fake_vhlt_dir/hlcsg" "$fake_vhlt_dir/hlbsp" \
    "$fake_vhlt_dir/hlvis" "$fake_vhlt_dir/hlrad"

map_path="$source_root/tests/fixtures/trenchbroom/src/texture_axes_zup.map"
compile_script="$source_root/tools/bsp/compile_vhlt_bsp30.sh"

light_map_path="$tmp_dir/vhlt_light_angles.map"
cat >"$light_map_path" <<'MAP'
{
"classname" "worldspawn"
}
{
"classname" "info_player_start"
"origin" "0 0 36"
}
{
"classname" "info_null"
"targetname" "spot_target"
}
{
"classname" "light_spot"
"angles" "270 90 15"
"target" "spot_target"
}
{
"classname" "light_environment"
"pitch" "45"
"angle" "180"
}
MAP

STELLAR_VHLT_DIR="$fake_vhlt_dir" \
STELLAR_VHLT_KEEP_WORK=1 \
STELLAR_VHLT_WORK_ROOT="$work_root" \
STELLAR_VHLT_LOG_DIR="$log_dir/preserve" \
bash "$compile_script" \
    --map "$map_path" \
    --out "$out_dir/texture_axes_preserve.bsp" \
    --profile full \
    --no-stellar-validation

preserved_work_map="$work_root/texture_axes_preserve/texture_axes_zup.map"
test -f "$preserved_work_map"
grep -q '"mapversion" "220"' "$preserved_work_map"
grep -q 'dev_wall_96 \[ 0 1 0 12 \] \[ 0 0 1 24 \] 15 0.5 2' \
    "$preserved_work_map"
grep -q 'dev_grid_32 \[ 1 0 0 8 \] \[ 0 1 0 16 \] 0 0.5 2' \
    "$preserved_work_map"
if grep -Eq 'dev_(grid_32|wall_96) 0 0 [-+0-9.]+' "$preserved_work_map"; then
    printf 'default VHLT rewrite stripped Valve 220 axes in %s\n' "$preserved_work_map" >&2
    exit 1
fi
if grep -q 'dev/wall_96' "$preserved_work_map"; then
    printf 'default VHLT rewrite did not convert slash texture aliases in %s\n' \
        "$preserved_work_map" >&2
    exit 1
fi

STELLAR_VHLT_DIR="$fake_vhlt_dir" \
STELLAR_VHLT_KEEP_WORK=1 \
STELLAR_VHLT_WORK_ROOT="$work_root" \
STELLAR_VHLT_LOG_DIR="$log_dir/lights" \
bash "$compile_script" \
    --map "$light_map_path" \
    --out "$out_dir/vhlt_light_angles.bsp" \
    --profile full \
    --no-stellar-validation

lights_work_map="$work_root/vhlt_light_angles/vhlt_light_angles.map"
test -f "$lights_work_map"
grep -q '"angles" "90 90 0"' "$lights_work_map"
grep -q '"angles" "-45 180 0"' "$lights_work_map"
grep -q '"target" "spot_target"' "$lights_work_map"
grep -q '"_stellar_vhlt_angles_normalized" "1"' "$lights_work_map"

STELLAR_VHLT_DIR="$fake_vhlt_dir" \
STELLAR_VHLT_KEEP_WORK=1 \
STELLAR_VHLT_WORK_ROOT="$work_root" \
STELLAR_VHLT_LOG_DIR="$log_dir/classic" \
bash "$compile_script" \
    --map "$map_path" \
    --out "$out_dir/texture_axes_classic.bsp" \
    --profile full \
    --classic-texture-axes \
    --no-stellar-validation

classic_work_map="$work_root/texture_axes_classic/texture_axes_zup.map"
test -f "$classic_work_map"
grep -q 'dev_wall_96 0 0 15 0.5 2' "$classic_work_map"
grep -q 'dev_grid_32 0 0 0 0.5 2' "$classic_work_map"

printf 'VHLT texture-axis rewrite regression passed: %s\n' "$tmp_dir"
