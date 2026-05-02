#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: tools/trenchbroom/install_stellar_game_package.sh --dest DIR (--copy|--link) [--repo-root DIR]

Installs the Stellar TrenchBroom game package into DIR/Stellar.

Options:
  --repo-root DIR   Stellar checkout root. Defaults to this script's checkout.
  --dest DIR        TrenchBroom games directory that should contain Stellar/.
  --copy           Copy the package and write Stellar/.stellar_repo_root.
  --link           Symlink DIR/Stellar to the checkout package directory.
  --help, -h       Show this help.
USAGE
}

fail() {
    printf 'install_stellar_game_package.sh: %s\n' "$1" >&2
    exit 1
}

normalize_dir() {
    local path="$1"
    [[ -d "$path" ]] || return 1
    cd "$path" && pwd
}

validate_repo_root() {
    local root="$1"
    [[ -f "$root/CMakeLists.txt" ]] || return 1
    [[ -f "$root/tools/trenchbroom/Stellar/GameConfig.cfg" ]] || return 1
    [[ -f "$root/tools/bsp/compile_trenchbroom_bsp30.sh" ]] || return 1
    [[ -f "$root/tools/bsp/validate_trenchbroom_bsp30.sh" ]] || return 1
}

validate_package() {
    local package="$1"
    local required
    for required in \
        GameConfig.cfg \
        CompilationProfiles.cfg \
        stellar_entities.fgd \
        Icon.png \
        materials/StellarDevMaterials.txt \
        materials/stellar_dev.wad \
        bin/stellar_tb_compile.sh \
        bin/stellar_tb_validate.sh \
        bin/stellar_tb_env.sh; do
        [[ -e "$package/$required" ]] || fail "installed package is missing $required at $package"
    done
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
default_repo_root="$(cd "$script_dir/../.." && pwd)"
repo_root="$default_repo_root"
dest=""
mode=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --repo-root)
            [[ $# -ge 2 ]] || fail "--repo-root requires a directory"
            repo_root="$2"
            shift 2
            ;;
        --dest)
            [[ $# -ge 2 ]] || fail "--dest requires a directory"
            dest="$2"
            shift 2
            ;;
        --copy)
            [[ -z "$mode" ]] || fail "choose only one of --copy or --link"
            mode="copy"
            shift
            ;;
        --link)
            [[ -z "$mode" ]] || fail "choose only one of --copy or --link"
            mode="link"
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

[[ -n "$dest" ]] || fail "--dest is required"
[[ -n "$mode" ]] || fail "choose --copy or --link"
repo_root="$(normalize_dir "$repo_root")" || fail "repo root does not exist: $repo_root"
validate_repo_root "$repo_root" || fail "repo root is not a Stellar checkout: $repo_root"
mkdir -p "$dest"
dest="$(normalize_dir "$dest")" || fail "destination does not exist: $dest"

source_package="$repo_root/tools/trenchbroom/Stellar"
target_package="$dest/Stellar"

if [[ -e "$target_package" || -L "$target_package" ]]; then
    fail "target already exists: $target_package"
fi

if [[ "$mode" == "copy" ]]; then
    cp -a "$source_package" "$target_package"
    printf '%s\n' "$repo_root" > "$target_package/.stellar_repo_root"
else
    ln -s "$source_package" "$target_package"
fi

validate_package "$target_package"

cat <<EOF
Installed Stellar TrenchBroom package: $target_package

Next steps:
  1. Open TrenchBroom Preferences > Games.
  2. Add or refresh the game package directory: $target_package
  3. Select the Stellar game profile.
  4. For copied packages, keep STELLAR_REPO_ROOT exported if you move the checkout:
       export STELLAR_REPO_ROOT="$repo_root"
  5. Build Stellar before validation profiles, or set STELLAR_CLIENT/STELLAR_SERVER.
EOF
