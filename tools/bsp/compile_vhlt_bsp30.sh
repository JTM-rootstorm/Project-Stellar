#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: tools/bsp/compile_vhlt_bsp30.sh --map path/to/source.map --out path/to/output.bsp --profile fast|full|validate-only [options]
       tools/bsp/compile_vhlt_bsp30.sh --bsp path/to/output.bsp --profile validate-only [options]

Options:
  --allow-skip              Exit 77 when required external tools are unavailable.
  --classic-texture-axes    Rewrite Valve 220 texture axes to classic shifts.
  --no-stellar-validation   Skip tools/bsp/validate_trenchbroom_bsp30.sh after BSP30 header validation.
  --skip-source-preflight   Do not run tools/bsp/validate_trenchbroom_map_source.py before VHLT.

Environment overrides:
  STELLAR_VHLT_DIR          Directory containing hlcsg, hlbsp, hlvis, hlrad, and optional ripent.
  HLCSG, HLBSP, HLVIS,
  HLRAD, RIPENT             Explicit per-tool paths.
  STELLAR_CLIENT            Path to stellar-client for post-compile validation.
  STELLAR_SERVER            Path to stellar-server for post-compile validation.
  STELLAR_VHLT_KEEP_WORK=1  Preserve the transient work directory.
  STELLAR_VHLT_WORK_ROOT    Override the transient work root directory.
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

host_tool_is_executable() {
    local path="$1"
    local host
    local machine
    local info

    [[ -x "$path" ]] || return 1
    host="$(uname -s 2>/dev/null || printf 'unknown')"
    machine="$(uname -m 2>/dev/null || printf 'unknown')"
    if command -v file >/dev/null 2>&1; then
        info="$(file "$path" 2>/dev/null || true)"
        case "$host:$info" in
            Darwin:*ELF*) return 1 ;;
            Linux:*Mach-O*) return 1 ;;
        esac
        case "$host:$machine:$info" in
            Darwin:arm64:*x86_64*) return 1 ;;
            Darwin:aarch64:*x86_64*) return 1 ;;
            Darwin:x86_64:*arm64*) return 1 ;;
            Linux:x86_64:*aarch64*) return 1 ;;
            Linux:x86_64:*ARM\ aarch64*) return 1 ;;
            Linux:amd64:*aarch64*) return 1 ;;
            Linux:amd64:*ARM\ aarch64*) return 1 ;;
            Linux:aarch64:*x86-64*) return 1 ;;
            Linux:arm64:*x86-64*) return 1 ;;
        esac
    fi
    return 0
}

platform_vhlt_dir() {
    local root="$1"
    local host
    local machine

    host="$(uname -s 2>/dev/null || printf 'unknown')"
    machine="$(uname -m 2>/dev/null || printf 'unknown')"
    case "$host:$machine" in
        Darwin:arm64|Darwin:aarch64)
            printf '%s/tools/bsp/macos-arm64\n' "$root"
            ;;
        Linux:x86_64|Linux:amd64)
            printf '%s/tools/bsp/linux-x86_64\n' "$root"
            ;;
    esac
}

find_tool() {
    local tool_name="$1"
    local override_value="$2"
    local required="$3"
    local candidate

    if [[ -n "$override_value" ]]; then
        if host_tool_is_executable "$override_value"; then
            absolute_path "$override_value"
            return 0
        fi
        if [[ "$required" == "1" ]]; then
            skip_or_fail "$tool_name override is not executable on this host: $override_value"
        fi
        return 1
    fi

    if [[ -n "${STELLAR_VHLT_DIR:-}" ]]; then
        candidate="$STELLAR_VHLT_DIR/$tool_name"
        if host_tool_is_executable "$candidate"; then
            absolute_path "$candidate"
            return 0
        fi
    fi

    local root="$4"
    local platform_dir
    platform_dir="$(platform_vhlt_dir "$root")"
    for candidate in \
        "${platform_dir:+$platform_dir/$tool_name}" \
        "$root/tools/bsp/$tool_name" \
        "$root/tools/bsp/vhlt/$tool_name" \
        "$root/tools/bsp/bin/$tool_name"; do
        [[ -n "$candidate" ]] || continue
        if host_tool_is_executable "$candidate"; then
            absolute_path "$candidate"
            return 0
        fi
    done

    if command -v "$tool_name" >/dev/null 2>&1; then
        candidate="$(command -v "$tool_name")"
        if host_tool_is_executable "$candidate"; then
            printf '%s\n' "$candidate"
            return 0
        fi
    fi

    if [[ "$required" == "1" ]]; then
        skip_or_fail "$tool_name not found or not executable on this host. Set STELLAR_VHLT_DIR or $tool_name override."
    fi
    return 1
}

read_bsp_version() {
    local bsp="$1"
    od -An -td4 -N4 "$bsp" | tr -d '[:space:]'
}

normalize_bsp_entity_lump() {
    local bsp_path="$1"
    python3 "$root/tools/bsp/map_rewrite.py" normalize-bsp-entity-lump "$bsp_path"
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

inject_worldspawn_key() {
    local map_path="$1"
    local key="$2"
    local value="$3"
    python3 "$root/tools/bsp/map_rewrite.py" inject-worldspawn-key "$map_path" "$key" "$value"
}

inject_wad_key() {
    inject_worldspawn_key "$1" wad "$2"
}

inject_mapversion_key() {
    inject_worldspawn_key "$1" mapversion 220
}

rewrite_vhlt_texture_names_only() {
    local map_path="$1"
    python3 "$root/tools/bsp/map_rewrite.py" rewrite-vhlt-texture-names-only "$map_path"
}

rewrite_vhlt_light_orientations() {
    local map_path="$1"
    python3 "$root/tools/bsp/normalize_vhlt_light_angles.py" "$map_path"
}

rewrite_valve220_to_classic_texture_axes() {
    local map_path="$1"
    python3 "$root/tools/bsp/map_rewrite.py" rewrite-valve220-to-classic-texture-axes "$map_path"
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
source_preflight="1"
classic_texture_axes="0"

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
        --classic-texture-axes)
            classic_texture_axes="1"
            shift
            ;;
        --no-stellar-validation)
            stellar_validation="0"
            shift
            ;;
        --skip-source-preflight)
            source_preflight="0"
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
source_preflight_script="$root/tools/bsp/validate_trenchbroom_map_source.py"
[[ -f "$validate_script" ]] || fail "validation wrapper missing: $validate_script"
[[ -f "$source_preflight_script" ]] || fail "source preflight missing: $source_preflight_script"

fixture="$(basename "$out_path")"
fixture="${fixture%.bsp}"
log_dir="${STELLAR_VHLT_LOG_DIR:-$root/build/tests/fixtures/trenchbroom/vhlt/logs/$fixture}"

if [[ "$profile" != "validate-only" ]]; then
    [[ -n "$map_path" ]] || fail "--map is required for compile profiles"
    [[ -f "$map_path" ]] || fail "MAP does not exist: $map_path"
    [[ "$map_path" == *.map ]] || fail "input must be a .map file: $map_path"
    if [[ "$source_preflight" == "1" ]]; then
        python3 "$source_preflight_script" "$map_path"
    fi

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

    work_root="${STELLAR_VHLT_WORK_ROOT:-$root/build/tests/fixtures/trenchbroom/vhlt/work}"
    work_dir="$work_root/$fixture"
    if [[ "${STELLAR_VHLT_KEEP_WORK:-0}" != "1" ]]; then
        rm -rf "$work_dir"
    fi
    mkdir -p "$work_dir"

    base="$(basename "$map_path" .map)"
    work_map="$work_dir/$base.map"
    cp "$map_path" "$work_map"

    python3 "$root/tools/bsp/create_stellar_dev_wad.py" --out "$work_dir/stellar_dev.wad"
    rewrite_vhlt_texture_names_only "$work_map"
    rewrite_vhlt_light_orientations "$work_map"
    if [[ "$classic_texture_axes" == "1" ]]; then
        rewrite_valve220_to_classic_texture_axes "$work_map"
    fi
    inject_mapversion_key "$work_map"
    inject_wad_key "$work_map" "$work_dir/stellar_dev.wad"
    if [[ "$profile" == "full" ]]; then
        inject_worldspawn_key "$work_map" _stellar_lighting_mode baked
    else
        inject_worldspawn_key "$work_map" _stellar_lighting_mode fullbright
    fi

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
