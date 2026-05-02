#!/usr/bin/env bash
set -euo pipefail

fail() {
    printf 'test_stellar_package_paths.sh: %s\n' "$1" >&2
    exit 1
}

repo_root="${1:-}"
if [[ -z "$repo_root" ]]; then
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    repo_root="$(cd "$script_dir/../.." && pwd)"
fi

[[ -f "$repo_root/CMakeLists.txt" ]] || fail "repo root missing CMakeLists.txt: $repo_root"
package="$repo_root/tools/trenchbroom/Stellar"
compile_shim="$package/bin/stellar_tb_compile.sh"
validate_shim="$package/bin/stellar_tb_validate.sh"
install_helper="$repo_root/tools/trenchbroom/install_stellar_game_package.sh"

for script in \
    "$compile_shim" \
    "$validate_shim" \
    "$package/bin/stellar_tb_env.sh" \
    "$install_helper"; do
    bash -n "$script"
done

"$compile_shim" --help >/dev/null
"$validate_shim" --help >/dev/null

python3 - "$package/GameConfig.cfg" "$package/CompilationProfiles.cfg" <<'PY'
from pathlib import Path
import sys

game_config = Path(sys.argv[1]).read_text(encoding="utf-8")
profiles = Path(sys.argv[2]).read_text(encoding="utf-8")
required_game_tokens = [
    '"icon": "Icon.png"',
    '"path": "bin/stellar_tb_compile.sh"',
    '"path": "bin/stellar_tb_validate.sh"',
    '"root": "materials"',
]
for token in required_game_tokens:
    if token not in game_config:
        raise SystemExit(f"GameConfig.cfg missing expected token: {token}")
for token in [
    '--map \\"${MAP_FULL_PATH}\\"',
    '--out \\"${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp\\"',
    '--map \\"${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp\\"',
]:
    if token not in profiles:
        raise SystemExit(f"CompilationProfiles.cfg missing expected quoted token: {token}")
PY

[[ -f "$package/Icon.png" ]] || fail "Icon.png missing"
[[ -f "$package/materials/stellar_dev.wad" ]] || fail "package WAD missing"

work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT
copy_dest="$work_dir/Games With Spaces"
mkdir -p "$copy_dest"
"$install_helper" --repo-root "$repo_root" --dest "$copy_dest" --copy >/dev/null
copied_package="$copy_dest/Stellar"
[[ -f "$copied_package/.stellar_repo_root" ]] || fail "copied package missing .stellar_repo_root"
STELLAR_REPO_ROOT="$repo_root" "$copied_package/bin/stellar_tb_compile.sh" --help >/dev/null
STELLAR_REPO_ROOT="$repo_root" "$copied_package/bin/stellar_tb_validate.sh" --help >/dev/null

fake_repo="$work_dir/Fake Repo With Spaces"
mkdir -p "$fake_repo/tools/bsp" "$work_dir/Map Dir With Spaces" "$work_dir/Out Dir With Spaces"
touch "$fake_repo/CMakeLists.txt" "$work_dir/Map Dir With Spaces/test map.map"
cat > "$fake_repo/tools/bsp/validate_trenchbroom_bsp30.sh" <<'SH'
#!/usr/bin/env bash
exit 0
SH
cat > "$fake_repo/tools/bsp/compile_trenchbroom_bsp30.sh" <<'SH'
#!/usr/bin/env bash
python3 - "$STELLAR_TB_ARG_CAPTURE" "$@" <<'PY'
from pathlib import Path
import sys
Path(sys.argv[1]).write_text("\n".join(sys.argv[2:]), encoding="utf-8")
PY
SH
chmod +x \
    "$fake_repo/tools/bsp/compile_trenchbroom_bsp30.sh" \
    "$fake_repo/tools/bsp/validate_trenchbroom_bsp30.sh"
arg_capture="$work_dir/args.txt"
STELLAR_REPO_ROOT="$fake_repo" STELLAR_TB_ARG_CAPTURE="$arg_capture" \
    "$copied_package/bin/stellar_tb_compile.sh" \
    --map "$work_dir/Map Dir With Spaces/test map.map" \
    --out "$work_dir/Out Dir With Spaces/test map.bsp" \
    --profile fast
python3 - "$arg_capture" "$work_dir" <<'PY'
from pathlib import Path
import sys
args = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
work = Path(sys.argv[2])
expected = [
    "--map",
    str(work / "Map Dir With Spaces" / "test map.map"),
    "--out",
    str(work / "Out Dir With Spaces" / "test map.bsp"),
    "--profile",
    "fast",
]
if args != expected:
    raise SystemExit(f"quoted argument preservation failed: {args!r} != {expected!r}")
PY

missing_root_package="$work_dir/MissingRoot/Stellar"
mkdir -p "$(dirname "$missing_root_package")"
cp -a "$package" "$missing_root_package"
rm -f "$missing_root_package/.stellar_repo_root"
if (cd "$work_dir" && env -u STELLAR_REPO_ROOT "$missing_root_package/bin/stellar_tb_compile.sh" --out nowhere.bsp) >/tmp/stellar_tb_missing_root.out 2>&1; then
    fail "copied package without repo root unexpectedly succeeded"
fi
if ! grep -q 'unable to locate Stellar repository root' /tmp/stellar_tb_missing_root.out; then
    fail "missing-root diagnostic was not actionable"
fi
rm -f /tmp/stellar_tb_missing_root.out

printf 'Stellar TrenchBroom package path smoke passed.\n'
