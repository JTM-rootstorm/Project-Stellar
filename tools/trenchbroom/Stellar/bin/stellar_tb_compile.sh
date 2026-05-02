#!/usr/bin/env bash
set -euo pipefail

STELLAR_TB_SHIM_NAME="stellar_tb_compile.sh"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=stellar_tb_env.sh
source "$script_dir/stellar_tb_env.sh"

usage() {
    cat <<'USAGE'
Usage: stellar_tb_compile.sh --map path/to/source.map --out path/to/output.bsp [--profile fast|full|validate-only] [--toolchain auto|single|vhlt]

Package-local shim for tools/bsp/compile_trenchbroom_bsp30.sh.

Repository root resolution order:
  1. STELLAR_REPO_ROOT
  2. package .stellar_repo_root file
  3. walking upward from the package path for repo-local installs
  4. current working directory when it is a Stellar checkout

Environment passed through to the repo wrapper includes STELLAR_CLIENT, STELLAR_SERVER,
STELLAR_BSP30_TOOLCHAIN, STELLAR_BSP30_COMPILER, QBSP, STELLAR_VHLT_DIR, HLCSG,
HLBSP, HLVIS, HLRAD, and RIPENT.
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
compile_wrapper="$repo_root/tools/bsp/compile_trenchbroom_bsp30.sh"

[[ -f "$compile_wrapper" ]] || stellar_tb_env_fail "compile wrapper missing: $compile_wrapper"

exec bash "$compile_wrapper" "$@"
