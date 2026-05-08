#!/usr/bin/env bash
# Shared environment helpers for Stellar TrenchBroom package-local shims.

stellar_tb_env_fail() {
    printf '%s: %s\n' "${STELLAR_TB_SHIM_NAME:-stellar_tb}" "$1" >&2
    exit 1
}

stellar_tb_env_package_root() {
    local source_path="${BASH_SOURCE[0]}"
    local source_dir
    source_dir="$(cd -P "$(dirname "$source_path")" && pwd -P)" || return 1
    cd -P "$source_dir/.." && pwd -P
}

stellar_tb_env_repo_is_valid() {
    local root="$1"
    [[ -n "$root" ]] || return 1
    [[ -f "$root/CMakeLists.txt" ]] || return 1
    [[ -f "$root/tools/bsp/compile_trenchbroom_bsp30.sh" ]] || return 1
    [[ -f "$root/tools/bsp/validate_trenchbroom_bsp30.sh" ]] || return 1
    return 0
}

stellar_tb_env_normalize_dir() {
    local path="$1"
    [[ -n "$path" ]] || return 1
    [[ -d "$path" ]] || return 1
    cd -P "$path" && pwd -P
}

stellar_tb_env_read_configured_root() {
    local package_root="$1"
    local root_file="$package_root/.stellar_repo_root"
    local configured=""

    [[ -f "$root_file" ]] || return 1
    IFS= read -r configured < "$root_file" || return 1
    configured="${configured%$'\r'}"
    [[ -n "$configured" ]] || return 1
    stellar_tb_env_normalize_dir "$configured"
}

stellar_tb_env_walk_for_repo_root() {
    local start_dir="$1"
    local current
    current="$(stellar_tb_env_normalize_dir "$start_dir")" || return 1

    while [[ "$current" != "/" ]]; do
        if stellar_tb_env_repo_is_valid "$current"; then
            printf '%s\n' "$current"
            return 0
        fi
        current="$(dirname "$current")"
    done

    return 1
}

stellar_tb_env_resolve_repo_root() {
    local package_root="$1"
    local candidate=""

    if [[ -n "${STELLAR_REPO_ROOT:-}" ]]; then
        candidate="$(stellar_tb_env_normalize_dir "$STELLAR_REPO_ROOT")" || \
            stellar_tb_env_fail "STELLAR_REPO_ROOT does not name an existing directory: $STELLAR_REPO_ROOT"
        stellar_tb_env_repo_is_valid "$candidate" || \
            stellar_tb_env_fail "STELLAR_REPO_ROOT is not a Stellar checkout with tools/bsp wrappers: $candidate"
        printf '%s\n' "$candidate"
        return 0
    fi

    if candidate="$(stellar_tb_env_read_configured_root "$package_root")"; then
        stellar_tb_env_repo_is_valid "$candidate" || \
            stellar_tb_env_fail ".stellar_repo_root points at an invalid Stellar checkout: $candidate"
        printf '%s\n' "$candidate"
        return 0
    fi

    if candidate="$(stellar_tb_env_walk_for_repo_root "$package_root")"; then
        printf '%s\n' "$candidate"
        return 0
    fi

    if candidate="$(stellar_tb_env_normalize_dir "$PWD")"; then
        if stellar_tb_env_repo_is_valid "$candidate"; then
            printf '%s\n' "$candidate"
            return 0
        fi
    fi

    stellar_tb_env_fail "unable to locate Stellar repository root. Set STELLAR_REPO_ROOT or write package .stellar_repo_root with the checkout path."
}
