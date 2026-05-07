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

compiler_available() {
    [[ -n "${STELLAR_BSP30_COMPILER:-}" || -n "${QBSP:-}" ]] && return 0
    for compiler in ericw-qbsp qbsp hqbsp tyr-qbsp; do
        command -v "$compiler" >/dev/null 2>&1 && return 0
    done
    host_can_execute_pair() {
        local csg="$1"
        local bsp="$2"
        local host
        local machine
        local info

        [[ -x "$csg" && -x "$bsp" ]] || return 1
        host="$(uname -s 2>/dev/null || printf 'unknown')"
        machine="$(uname -m 2>/dev/null || printf 'unknown')"
        if command -v file >/dev/null 2>&1; then
            info="$(file "$csg" "$bsp" 2>/dev/null || true)"
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
        case "$(uname -s 2>/dev/null || printf 'unknown'):$(uname -m 2>/dev/null || printf 'unknown')" in
            Darwin:arm64|Darwin:aarch64)
                printf '%s/tools/bsp/macos-arm64\n' "$repo_root"
                ;;
            Linux:x86_64|Linux:amd64)
                printf '%s/tools/bsp/linux-x86_64\n' "$repo_root"
                ;;
        esac
    }
    if [[ -n "${STELLAR_VHLT_DIR:-}" ]] && host_can_execute_pair "$STELLAR_VHLT_DIR/hlcsg" "$STELLAR_VHLT_DIR/hlbsp"; then
        return 0
    fi
    platform_dir="$(platform_vhlt_dir)"
    for dir in \
        "${platform_dir:+$platform_dir}" \
        "$repo_root/tools/bsp" \
        "$repo_root/tools/bsp/vhlt" \
        "$repo_root/tools/bsp/bin"; do
        [[ -n "$dir" ]] || continue
        host_can_execute_pair "$dir/hlcsg" "$dir/hlbsp" && return 0
    done
    return 1
}

for script in \
    "$compile_shim" \
    "$validate_shim" \
    "$package/bin/stellar_tb_env.sh" \
    "$install_helper"; do
    bash -n "$script"
done

python3 "$repo_root/tools/trenchbroom/lint_stellar_compilation_profiles.py" \
    --game-config "$package/GameConfig.cfg" \
    --profiles "$package/CompilationProfiles.cfg" >/dev/null

"$compile_shim" --help >/dev/null
"$validate_shim" --help >/dev/null

python3 - "$package/GameConfig.cfg" "$package/CompilationProfiles.cfg" <<'PY'
from pathlib import Path
import sys

game_config = Path(sys.argv[1]).read_text(encoding="utf-8")
profiles = Path(sys.argv[2]).read_text(encoding="utf-8")
required_game_tokens = [
    '"icon": "Icon.png"',
    '"name": "STELLAR_BSP30_COMPILE"',
    '"name": "STELLAR_BSP30_VALIDATE"',
    '"path": "bin/stellar_tb_compile.sh"',
    '"path": "bin/stellar_tb_validate.sh"',
    '"root": "materials"',
]
for token in required_game_tokens:
    if token not in game_config:
        raise SystemExit(f"GameConfig.cfg missing expected token: {token}")
for token in [
    '"version": 1',
    '"workdir": "${WORK_DIR_PATH}"',
    '"tool": "${STELLAR_BSP30_COMPILE}"',
    '"tool": "${STELLAR_BSP30_VALIDATE}"',
    '--map \\"${MAP_FULL_PATH}\\"',
    '--out \\"${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp\\"',
    '--toolchain vhlt',
    '--map \\"${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp\\"',
]:
    if token not in profiles:
        raise SystemExit(f"CompilationProfiles.cfg missing expected quoted token: {token}")
PY

[[ -f "$package/Icon.png" ]] || fail "Icon.png missing"
[[ -f "$package/materials/stellar_dev.wad" ]] || fail "package WAD missing"

work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

bad_profiles="$work_dir/BadCompilationProfiles.cfg"
cat > "$bad_profiles" <<'JSON'
{
    "version": 1,
    "profiles": [
        {
            "name": "Old Display Name Tool Reference",
            "workdir": "${WORK_DIR_PATH}",
            "tasks": [
                {
                    "type": "tool",
                    "tool": "Stellar BSP30 compile wrapper",
                    "parameters": "--map \"${MAP_FULL_PATH}\""
                }
            ]
        }
    ]
}
JSON
if python3 "$repo_root/tools/trenchbroom/lint_stellar_compilation_profiles.py" \
    --game-config "$package/GameConfig.cfg" \
    --profiles "$bad_profiles" >"$work_dir/bad_profiles.out" 2>&1; then
    fail "profile linter accepted old display-name tool reference"
fi
if ! grep -q 'tool reference contains spaces' "$work_dir/bad_profiles.out"; then
    fail "profile linter did not explain old display-name tool reference"
fi

copy_dest="$work_dir/Games With Spaces"
mkdir -p "$copy_dest"
"$install_helper" --repo-root "$repo_root" --dest "$copy_dest" --copy >/dev/null
copied_package="$copy_dest/Stellar"
[[ -f "$copied_package/.stellar_repo_root" ]] || fail "copied package missing .stellar_repo_root"
STELLAR_REPO_ROOT="$repo_root" "$copied_package/bin/stellar_tb_compile.sh" --help >/dev/null
STELLAR_REPO_ROOT="$repo_root" "$copied_package/bin/stellar_tb_validate.sh" --help >/dev/null

if compiler_available; then
    smoke_out="$work_dir/copied package compile/minimal_zup_room.bsp"
    mkdir -p "$(dirname "$smoke_out")"
    STELLAR_REPO_ROOT="$repo_root" \
    STELLAR_CLIENT="${STELLAR_CLIENT:-${STELLAR_TEST_BUILD_ROOT:-$repo_root/build}/stellar-client}" \
    STELLAR_SERVER="${STELLAR_SERVER:-${STELLAR_TEST_BUILD_ROOT:-$repo_root/build}/stellar-server}" \
        "$copied_package/bin/stellar_tb_compile.sh" \
        --map "$repo_root/tests/fixtures/trenchbroom/src/minimal_zup_room.map" \
        --out "$smoke_out" \
        --profile fast
    STELLAR_REPO_ROOT="$repo_root" \
    STELLAR_CLIENT="${STELLAR_CLIENT:-${STELLAR_TEST_BUILD_ROOT:-$repo_root/build}/stellar-client}" \
    STELLAR_SERVER="${STELLAR_SERVER:-${STELLAR_TEST_BUILD_ROOT:-$repo_root/build}/stellar-server}" \
        "$copied_package/bin/stellar_tb_validate.sh" --map "$smoke_out"
else
    printf 'Skipping copied-package compile sub-step: no external BSP30 compiler found.\n'
fi

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
missing_root_out="$work_dir/stellar_tb_missing_root.out"
if (cd "$work_dir" && env -u STELLAR_REPO_ROOT "$missing_root_package/bin/stellar_tb_compile.sh" --out nowhere.bsp) >"$missing_root_out" 2>&1; then
    fail "copied package without repo root unexpectedly succeeded"
fi
if ! grep -q 'unable to locate Stellar repository root' "$missing_root_out"; then
    fail "missing-root diagnostic was not actionable"
fi

printf 'Stellar TrenchBroom package path smoke passed.\n'
