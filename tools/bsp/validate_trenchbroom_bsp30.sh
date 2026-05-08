#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: tools/bsp/validate_trenchbroom_bsp30.sh --map path/to/map.bsp
       tools/bsp/validate_trenchbroom_bsp30.sh path/to/map.bsp

Environment overrides:
  STELLAR_CLIENT        Path to stellar-client.
  STELLAR_SERVER        Path to stellar-server.
  STELLAR_BUILD_DIR     Build directory containing Stellar binaries.
  STELLAR_CMAKE_PRESET  CMake preset name used to select the matching build directory.

Defaults search the host preset build directory first (build-linux on Linux,
build-macos on macOS), then build/, then PATH.
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

env_name_for_binary() {
    case "$1" in
        stellar-client) printf 'STELLAR_CLIENT\n' ;;
        stellar-server) printf 'STELLAR_SERVER\n' ;;
        *)
            printf '%s\n' "$1"
            ;;
    esac
}

normalize_build_dir() {
    local root="$1"
    local build_dir="$2"

    [[ -n "$build_dir" ]] || return 1
    case "$build_dir" in
        /*) printf '%s\n' "$build_dir" ;;
        *) printf '%s/%s\n' "$root" "$build_dir" ;;
    esac
}

build_dir_for_cmake_preset() {
    local root="$1"
    local preset="$2"

    case "$preset" in
        linux-default)
            printf '%s/build-linux\n' "$root"
            ;;
        linux-vulkan)
            printf '%s/build-linux-vulkan\n' "$root"
            ;;
        linux-vulkan-only)
            printf '%s/build-linux-vulkan-only\n' "$root"
            ;;
        macos-default)
            printf '%s/build-macos\n' "$root"
            ;;
        macos-metal)
            printf '%s/build-macos-metal\n' "$root"
            ;;
        macos-metal-only)
            printf '%s/build-macos-metal-only\n' "$root"
            ;;
        "")
            return 1
            ;;
        *)
            return 1
            ;;
    esac
}

host_default_build_dir() {
    local root="$1"
    local host

    host="$(uname -s 2>/dev/null || printf 'unknown')"
    case "$host" in
        Darwin)
            printf '%s/build-macos\n' "$root"
            ;;
        Linux)
            printf '%s/build-linux\n' "$root"
            ;;
        *)
            printf '%s/build\n' "$root"
            ;;
    esac
}

find_binary() {
    local env_value="$1"
    local binary_name="$2"
    local root="$3"
    local build_dirs=()
    local build_dir
    local env_name
    local preset_dir
    local fallback_dir
    local message

    if [[ -n "$env_value" ]]; then
        [[ -x "$env_value" ]] || fail "$binary_name override is not executable: $env_value"
        printf '%s\n' "$env_value"
        return 0
    fi

    if [[ -n "${STELLAR_BUILD_DIR:-}" ]]; then
        build_dirs+=("$(normalize_build_dir "$root" "$STELLAR_BUILD_DIR")")
    fi

    if preset_dir="$(build_dir_for_cmake_preset "$root" "${STELLAR_CMAKE_PRESET:-}")"; then
        build_dirs+=("$preset_dir")
    fi

    build_dirs+=("$(host_default_build_dir "$root")")
    build_dirs+=("$root/build")

    for build_dir in "${build_dirs[@]}"; do
        if [[ -x "$build_dir/$binary_name" ]]; then
            printf '%s\n' "$build_dir/$binary_name"
            return 0
        fi
    done

    if [[ -n "${STELLAR_CMAKE_PRESET:-}" && ${#build_dirs[@]} -gt 0 ]]; then
        fallback_dir="${build_dirs[0]}"
        env_name="$(env_name_for_binary "$binary_name")"
        message="$binary_name not found for STELLAR_CMAKE_PRESET=$STELLAR_CMAKE_PRESET."
        message+=" Expected $fallback_dir/$binary_name or set $env_name."
        fail "$message"
    fi

    if [[ -n "${STELLAR_BUILD_DIR:-}" && ${#build_dirs[@]} -gt 0 ]]; then
        fallback_dir="${build_dirs[0]}"
        fail "$binary_name not found in STELLAR_BUILD_DIR: $fallback_dir/$binary_name"
    fi

    if command -v "$binary_name" >/dev/null 2>&1; then
        command -v "$binary_name"
        return 0
    fi

    env_name="$(env_name_for_binary "$binary_name")"
    message="$binary_name not found. Build the project or set $env_name,"
    message+=" STELLAR_BUILD_DIR, or STELLAR_CMAKE_PRESET."
    fail "$message"
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
            if [[ -z "$map_path" && "$1" != -* ]]; then
                map_path="$1"
                shift
            else
                fail "unknown argument: $1"
            fi
            ;;
    esac
done

[[ -n "$map_path" ]] || fail "--map is required"
[[ -f "$map_path" ]] || fail "BSP does not exist: $map_path"
[[ -s "$map_path" ]] || fail "BSP is empty: $map_path"

version="$(read_bsp_version "$map_path")"
[[ "$version" == "30" ]] || fail "expected BSP30 version 30, found '${version:-unreadable}' in $map_path"

root="$(repo_root)"
client="$(find_binary "${STELLAR_CLIENT:-}" "stellar-client" "$root")"
server="$(find_binary "${STELLAR_SERVER:-}" "stellar-server" "$root")"

"$client" --validate-map "$map_path"
"$server" --validate-config --map "$map_path"

printf 'Validated BSP30 map with Stellar client/server: %s\n' "$map_path"
