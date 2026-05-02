#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: tools/bsp/validate_trenchbroom_bsp30.sh --map path/to/map.bsp

Environment overrides:
  STELLAR_CLIENT  Path to stellar-client (default: build/stellar-client, then PATH)
  STELLAR_SERVER  Path to stellar-server (default: build/stellar-server, then PATH)
USAGE
}

fail() {
    printf 'validate_trenchbroom_bsp30.sh: %s\n' "$1" >&2
    exit 1
}

repo_root() {
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    cd "$script_dir/../.." && pwd
}

find_binary() {
    local env_value="$1"
    local default_path="$2"
    local binary_name="$3"

    if [[ -n "$env_value" ]]; then
        [[ -x "$env_value" ]] || fail "$binary_name override is not executable: $env_value"
        printf '%s\n' "$env_value"
        return 0
    fi

    if [[ -x "$default_path" ]]; then
        printf '%s\n' "$default_path"
        return 0
    fi

    if command -v "$binary_name" >/dev/null 2>&1; then
        command -v "$binary_name"
        return 0
    fi

    fail "$binary_name not found. Build the project or set ${binary_name^^} path override."
}

read_bsp_version() {
    local bsp="$1"
    od -An -td4 -N4 "$bsp" | tr -d '[:space:]'
}

map_path=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --map|--bsp)
            [[ $# -ge 2 ]] || fail "$1 requires a path"
            map_path="$2"
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

[[ -n "$map_path" ]] || fail "--map is required"
[[ -f "$map_path" ]] || fail "BSP does not exist: $map_path"
[[ -s "$map_path" ]] || fail "BSP is empty: $map_path"

version="$(read_bsp_version "$map_path")"
[[ "$version" == "30" ]] || fail "expected BSP30 version 30, found '${version:-unreadable}' in $map_path"

root="$(repo_root)"
client="$(find_binary "${STELLAR_CLIENT:-}" "$root/build/stellar-client" "stellar-client")"
server="$(find_binary "${STELLAR_SERVER:-}" "$root/build/stellar-server" "stellar-server")"

"$client" --validate-map "$map_path"
"$server" --validate-config --map "$map_path"

printf 'Validated BSP30 map with Stellar client/server: %s\n' "$map_path"
