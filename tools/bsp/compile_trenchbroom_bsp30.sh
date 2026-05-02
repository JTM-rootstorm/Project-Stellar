#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: tools/bsp/compile_trenchbroom_bsp30.sh --map maps/src/room.map --out maps/compiled/room.bsp [--profile fast|full|validate-only]

Environment overrides:
  STELLAR_BSP30_COMPILER  BSP30-capable map compiler executable.
  QBSP                    Alternative compiler executable if STELLAR_BSP30_COMPILER is unset.
  STELLAR_CLIENT          Path to stellar-client for post-compile validation.
  STELLAR_SERVER          Path to stellar-server for post-compile validation.

Compiler arguments:
  The wrapper invokes the selected compiler as: <compiler> <map> <out>
  If your compiler uses a different interface, wrap it in a small adapter and point
  STELLAR_BSP30_COMPILER at that adapter.
USAGE
}

fail() {
    printf 'compile_trenchbroom_bsp30.sh: %s\n' "$1" >&2
    exit 1
}

repo_root() {
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    cd "$script_dir/../.." && pwd
}

find_compiler() {
    if [[ -n "${STELLAR_BSP30_COMPILER:-}" ]]; then
        [[ -x "$STELLAR_BSP30_COMPILER" ]] || fail "STELLAR_BSP30_COMPILER is not executable: $STELLAR_BSP30_COMPILER"
        printf '%s\n' "$STELLAR_BSP30_COMPILER"
        return 0
    fi

    if [[ -n "${QBSP:-}" ]]; then
        [[ -x "$QBSP" ]] || fail "QBSP is not executable: $QBSP"
        printf '%s\n' "$QBSP"
        return 0
    fi

    local candidate
    for candidate in ericw-qbsp qbsp hqbsp tyr-qbsp; do
        if command -v "$candidate" >/dev/null 2>&1; then
            command -v "$candidate"
            return 0
        fi
    done

    fail "no BSP30 compiler found. Set STELLAR_BSP30_COMPILER or install qbsp/ericw-qbsp/hqbsp."
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
[[ -n "$out_path" ]] || fail "--out is required"

root="$(repo_root)"
validate_script="$root/tools/bsp/validate_trenchbroom_bsp30.sh"
[[ -x "$validate_script" || -f "$validate_script" ]] || fail "validation wrapper missing: $validate_script"

if [[ "$profile" != "validate-only" ]]; then
    [[ -n "$map_path" ]] || fail "--map is required for compile profiles"
    [[ -f "$map_path" ]] || fail "MAP does not exist: $map_path"
    [[ "$map_path" == *.map ]] || fail "input must be a .map file: $map_path"

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
