#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: tools/bsp/compile_trenchbroom_bsp30.sh --map maps/src/room.map --out maps/compiled/room.bsp [--profile fast|full|validate-only] [--toolchain auto|single|vhlt] [--allow-skip] [--skip-source-preflight]

Examples:
  tools/bsp/compile_trenchbroom_bsp30.sh --map maps/src/room.map --out maps/compiled/room.bsp --profile fast
  tools/bsp/compile_trenchbroom_bsp30.sh --map maps/src/room.map --out maps/compiled/room.bsp --profile full --toolchain single
  tools/bsp/compile_trenchbroom_bsp30.sh --map maps/src/room.map --out maps/compiled/room.bsp --profile full --toolchain vhlt
  STELLAR_BSP30_TOOLCHAIN=vhlt tools/bsp/compile_trenchbroom_bsp30.sh --map maps/src/room.map --out maps/compiled/room.bsp --profile full
  tools/bsp/compile_trenchbroom_bsp30.sh --out maps/compiled/room.bsp --profile validate-only

Environment overrides:
  STELLAR_BSP30_TOOLCHAIN Toolchain selection: auto, single, or vhlt (default: auto).
  STELLAR_BSP30_COMPILER  BSP30-capable map compiler executable.
  QBSP                    Alternative compiler executable if STELLAR_BSP30_COMPILER is unset.
  STELLAR_CLIENT          Path to stellar-client for post-compile validation.
  STELLAR_SERVER          Path to stellar-server for post-compile validation.

Compiler arguments:
  The single toolchain invokes the selected compiler as: <compiler> <map> <out>
  If your compiler uses a different interface, wrap it in a small adapter and point
  STELLAR_BSP30_COMPILER at that adapter.

VHLT arguments:
  The vhlt toolchain delegates to tools/bsp/compile_vhlt_bsp30.sh with the selected
  map, output path, and profile. STELLAR_CLIENT and STELLAR_SERVER are preserved for
  VHLT post-compile validation.
USAGE
}

fail() {
    printf 'compile_trenchbroom_bsp30.sh: %s\n' "$1" >&2
    exit 1
}

skip_or_fail() {
    if [[ "$allow_skip" == "1" ]]; then
        printf 'compile_trenchbroom_bsp30.sh: skipping: %s\n' "$1" >&2
        exit 77
    fi
    fail "$1"
}

repo_root() {
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    cd "$script_dir/../.." && pwd
}

host_tool_is_executable() {
    local path="$1"
    local host
    local info

    [[ -x "$path" ]] || return 1
    host="$(uname -s 2>/dev/null || printf 'unknown')"
    if command -v file >/dev/null 2>&1; then
        info="$(file "$path" 2>/dev/null || true)"
        case "$host:$info" in
            Darwin:*ELF*) return 1 ;;
            Linux:*Mach-O*) return 1 ;;
        esac
    fi
    return 0
}

find_compiler() {
    if [[ -n "${STELLAR_BSP30_COMPILER:-}" ]]; then
        host_tool_is_executable "$STELLAR_BSP30_COMPILER" || skip_or_fail "STELLAR_BSP30_COMPILER is not executable on this host: $STELLAR_BSP30_COMPILER"
        printf '%s\n' "$STELLAR_BSP30_COMPILER"
        return 0
    fi

    if [[ -n "${QBSP:-}" ]]; then
        host_tool_is_executable "$QBSP" || skip_or_fail "QBSP is not executable on this host: $QBSP"
        printf '%s\n' "$QBSP"
        return 0
    fi

    local candidate
    for candidate in ericw-qbsp qbsp hqbsp tyr-qbsp; do
        if command -v "$candidate" >/dev/null 2>&1; then
            candidate="$(command -v "$candidate")"
            if host_tool_is_executable "$candidate"; then
                printf '%s\n' "$candidate"
                return 0
            fi
        fi
    done

    skip_or_fail "no BSP30 compiler found. Set STELLAR_BSP30_COMPILER or install qbsp/ericw-qbsp/hqbsp."
}

find_legacy_compiler() {
    local candidate
    for candidate in ericw-qbsp qbsp hqbsp tyr-qbsp; do
        if command -v "$candidate" >/dev/null 2>&1; then
            command -v "$candidate"
            return 0
        fi
    done

    return 1
}

explicit_single_compiler() {
    if [[ -n "${STELLAR_BSP30_COMPILER:-}" ]]; then
        host_tool_is_executable "$STELLAR_BSP30_COMPILER" || skip_or_fail "STELLAR_BSP30_COMPILER is not executable on this host: $STELLAR_BSP30_COMPILER"
        printf '%s\n' "$STELLAR_BSP30_COMPILER"
        return 0
    fi

    if [[ -n "${QBSP:-}" ]]; then
        host_tool_is_executable "$QBSP" || skip_or_fail "QBSP is not executable on this host: $QBSP"
        printf '%s\n' "$QBSP"
        return 0
    fi

    return 1
}

has_vhlt_tools_under_bsp() {
    local root="$1"
    local tool
    local candidate
    local found

    for tool in hlcsg hlbsp hlvis hlrad; do
        found="0"
        for candidate in \
            "$root/tools/bsp/$tool" \
            "$root/tools/bsp/vhlt/$tool" \
            "$root/tools/bsp/bin/$tool"; do
            if host_tool_is_executable "$candidate"; then
                found="1"
                break
            fi
        done
        [[ "$found" == "1" ]] || return 1
    done

    return 0
}

select_auto_toolchain() {
    local root="$1"

    if explicit_single_compiler >/dev/null; then
        printf 'single\n'
        return 0
    fi

    if has_vhlt_tools_under_bsp "$root"; then
        printf 'vhlt\n'
        return 0
    fi

    if find_legacy_compiler >/dev/null; then
        printf 'single\n'
        return 0
    fi

    skip_or_fail "no BSP30 toolchain found. Set STELLAR_BSP30_COMPILER, QBSP, STELLAR_BSP30_TOOLCHAIN=vhlt, or install VHLT/qbsp tools."
}

read_bsp_version() {
    local bsp="$1"
    od -An -td4 -N4 "$bsp" | tr -d '[:space:]'
}

validate_entity_text() {
    local bsp="$1"
    strings "$bsp" | grep -q '"classname"' || fail "compiled BSP appears to be missing entity classname keys"
    strings "$bsp" | grep -q 'info_player_start' || fail "compiled BSP is missing info_player_start; add a Z-up player start at origin 0 0 36"
}

map_path=""
out_path=""
profile="fast"
toolchain="${STELLAR_BSP30_TOOLCHAIN:-auto}"
source_preflight="1"
allow_skip="0"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --map)
            [[ $# -ge 2 ]] || fail "--map requires a path"
            map_path="$2"
            shift 2
            ;;
        --out)
            [[ $# -ge 2 ]] || fail "--out requires a path"
            out_path="$2"
            shift 2
            ;;
        --profile)
            [[ $# -ge 2 ]] || fail "--profile requires fast, full, or validate-only"
            profile="$2"
            shift 2
            ;;
        --toolchain)
            [[ $# -ge 2 ]] || fail "--toolchain requires auto, single, or vhlt"
            toolchain="$2"
            shift 2
            ;;
        --skip-source-preflight)
            source_preflight="0"
            shift
            ;;
        --allow-skip)
            allow_skip="1"
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

[[ "$profile" == "fast" || "$profile" == "full" || "$profile" == "validate-only" ]] || fail "unsupported profile: $profile"
[[ "$toolchain" == "auto" || "$toolchain" == "single" || "$toolchain" == "vhlt" ]] || fail "unsupported toolchain: $toolchain"
[[ -n "$out_path" ]] || fail "--out is required"

root="$(repo_root)"
validate_script="$root/tools/bsp/validate_trenchbroom_bsp30.sh"
source_preflight_script="$root/tools/bsp/validate_trenchbroom_map_source.py"
vhlt_script="$root/tools/bsp/compile_vhlt_bsp30.sh"
[[ -x "$validate_script" || -f "$validate_script" ]] || fail "validation wrapper missing: $validate_script"
[[ -f "$source_preflight_script" ]] || fail "source preflight missing: $source_preflight_script"

selected_toolchain="$toolchain"
if [[ "$selected_toolchain" == "auto" ]]; then
    selected_toolchain="$(select_auto_toolchain "$root")"
fi

if [[ "$selected_toolchain" == "vhlt" ]]; then
    [[ -x "$vhlt_script" || -f "$vhlt_script" ]] || fail "VHLT wrapper missing: $vhlt_script"
    if [[ "$profile" == "validate-only" ]]; then
        bash "$vhlt_script" --bsp "$out_path" --profile validate-only
    else
        [[ -n "$map_path" ]] || fail "--map is required for compile profiles"
        [[ -f "$map_path" ]] || fail "MAP does not exist: $map_path"
        [[ "$map_path" == *.map ]] || fail "input must be a .map file: $map_path"
        if [[ "$source_preflight" == "1" ]]; then
            python3 "$source_preflight_script" "$map_path"
        fi
        vhlt_args=(--map "$map_path" --out "$out_path" --profile "$profile")
        if [[ "$source_preflight" == "0" ]]; then
            vhlt_args+=(--skip-source-preflight)
        fi
        if [[ "$allow_skip" == "1" ]]; then
            vhlt_args+=(--allow-skip)
        fi
        bash "$vhlt_script" "${vhlt_args[@]}"
    fi
    exit 0
fi

if [[ "$profile" != "validate-only" ]]; then
    [[ -n "$map_path" ]] || fail "--map is required for compile profiles"
    [[ -f "$map_path" ]] || fail "MAP does not exist: $map_path"
    [[ "$map_path" == *.map ]] || fail "input must be a .map file: $map_path"
    if [[ "$source_preflight" == "1" ]]; then
        python3 "$source_preflight_script" "$map_path"
    fi

    out_dir="$(dirname "$out_path")"
    mkdir -p "$out_dir"

    compiler="$(find_compiler)"
    printf 'Compiling BSP30 with %s (%s): %s -> %s\n' "$compiler" "$profile" "$map_path" "$out_path"
    "$compiler" "$map_path" "$out_path"
else
    [[ -f "$out_path" ]] || fail "validate-only output BSP does not exist: $out_path"
fi

[[ -f "$out_path" ]] || fail "compiler did not create BSP output: $out_path"
[[ -s "$out_path" ]] || fail "compiler created an empty BSP output: $out_path"

version="$(read_bsp_version "$out_path")"
[[ "$version" == "30" ]] || fail "expected BSP30 version 30, found '${version:-unreadable}' in $out_path"

validate_entity_text "$out_path"
bash "$validate_script" --map "$out_path"

printf 'Compiled and validated BSP30 map: %s\n' "$out_path"
