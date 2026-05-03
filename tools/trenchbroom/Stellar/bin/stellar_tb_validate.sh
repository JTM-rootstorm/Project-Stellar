#!/usr/bin/env bash
set -euo pipefail

STELLAR_TB_SHIM_NAME="stellar_tb_validate.sh"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=stellar_tb_env.sh
source "$script_dir/stellar_tb_env.sh"

usage() {
    cat <<'USAGE'
Usage: stellar_tb_validate.sh --map path/to/map.bsp
       stellar_tb_validate.sh path/to/map.bsp

Package-local shim for tools/bsp/validate_trenchbroom_bsp30.sh.
Set STELLAR_REPO_ROOT for copied-package installs, or install with
 tools/trenchbroom/install_stellar_game_package.sh --copy
so the package-local .stellar_repo_root file is written.
USAGE
}

for arg in "$@"; do
    case "$arg" in
        --help|-h)
            usage
            exit 0
            ;;
    esac
done

package_root="$(stellar_tb_env_package_root)" || stellar_tb_env_fail "unable to resolve package root"
repo_root="$(stellar_tb_env_resolve_repo_root "$package_root")"
validate_wrapper="$repo_root/tools/bsp/validate_trenchbroom_bsp30.sh"

[[ -f "$validate_wrapper" ]] || stellar_tb_env_fail "validation wrapper missing: $validate_wrapper"

exec bash "$validate_wrapper" "$@"
