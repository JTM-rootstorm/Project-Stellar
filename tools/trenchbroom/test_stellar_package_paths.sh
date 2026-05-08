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

stellar_binary_available() {
    local binary="$1"
    local env_name
    local env_value
    local host
    local default_build

    case "$binary" in
        stellar-client) env_name="STELLAR_CLIENT" ;;
        stellar-server) env_name="STELLAR_SERVER" ;;
        *) return 1 ;;
    esac

    env_value="${!env_name:-}"
    [[ -n "$env_value" && -x "$env_value" ]] && return 0
    if [[ -n "${STELLAR_TEST_BUILD_ROOT:-}" ]]; then
        [[ -x "$STELLAR_TEST_BUILD_ROOT/$binary" ]] && return 0
    fi

    host="$(uname -s 2>/dev/null || printf 'unknown')"
    case "$host" in
        Darwin) default_build="build-macos" ;;
        Linux) default_build="build-linux" ;;
        *) default_build="build" ;;
    esac

    [[ -x "$repo_root/$default_build/$binary" ]] && return 0
    [[ -x "$repo_root/build/$binary" ]] && return 0
    command -v "$binary" >/dev/null 2>&1
}

stellar_validation_available() {
    stellar_binary_available stellar-client && stellar_binary_available stellar-server
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
    '"description": "Path to the package-local Stellar BSP30 compile shim"',
    '"description": "Path to the package-local Stellar BSP30 validate shim"',
    '"root": "materials"',
]
for token in required_game_tokens:
    if token not in game_config:
        raise SystemExit(f"GameConfig.cfg missing expected token: {token}")
for token in [
    '"version": 1',
    '"workdir": "${MAP_DIR_PATH}/../compiled"',
    '"tool": "${STELLAR_BSP30_COMPILE}"',
    '"tool": "${STELLAR_BSP30_VALIDATE}"',
    '--map \\"${MAP_DIR_PATH}/${MAP_FULL_NAME}\\"',
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

bad_workdir_profiles="$work_dir/BadWorkdirCompilationProfiles.cfg"
cat > "$bad_workdir_profiles" <<'JSON'
{
    "version": 1,
    "profiles": [
        {
            "name": "Bad Workdir Scope",
            "workdir": "${WORK_DIR_PATH}",
            "tasks": [
                {
                    "type": "tool",
                    "tool": "${STELLAR_BSP30_COMPILE}",
                    "parameters": "--map \"${MAP_DIR_PATH}/${MAP_FULL_NAME}\""
                }
            ]
        }
    ]
}
JSON
if python3 "$repo_root/tools/trenchbroom/lint_stellar_compilation_profiles.py" \
    --game-config "$package/GameConfig.cfg" \
    --profiles "$bad_workdir_profiles" >"$work_dir/bad_workdir_profiles.out" 2>&1; then
    fail "profile linter accepted WORK_DIR_PATH in workdir"
fi
if ! grep -q 'workdir cannot use ${WORK_DIR_PATH}' "$work_dir/bad_workdir_profiles.out"; then
    fail "profile linter did not explain invalid WORK_DIR_PATH workdir scope"
fi

bad_map_full_profiles="$work_dir/BadMapFullPathCompilationProfiles.cfg"
cat > "$bad_map_full_profiles" <<'JSON'
{
    "version": 1,
    "profiles": [
        {
            "name": "Bad Map Full Path Variable",
            "workdir": "${MAP_DIR_PATH}/../compiled",
            "tasks": [
                {
                    "type": "tool",
                    "tool": "${STELLAR_BSP30_COMPILE}",
                    "parameters": "--map \"${MAP_FULL_PATH}\""
                }
            ]
        }
    ]
}
JSON
if python3 "$repo_root/tools/trenchbroom/lint_stellar_compilation_profiles.py" \
    --game-config "$package/GameConfig.cfg" \
    --profiles "$bad_map_full_profiles" >"$work_dir/bad_map_full_profiles.out" 2>&1; then
    fail "profile linter accepted unsupported MAP_FULL_PATH variable"
fi
if ! grep -q 'unsupported TrenchBroom variable ${MAP_FULL_PATH}' \
    "$work_dir/bad_map_full_profiles.out"; then
    fail "profile linter did not explain unsupported MAP_FULL_PATH variable"
fi

bad_tool_ref_profiles="$work_dir/BadToolReferenceCompilationProfiles.cfg"
cat > "$bad_tool_ref_profiles" <<'JSON'
{
    "version": 1,
    "profiles": [
        {
            "name": "Old Display Name Tool Reference",
            "workdir": "${MAP_DIR_PATH}/../compiled",
            "tasks": [
                {
                    "type": "tool",
                    "tool": "Stellar BSP30 compile wrapper",
                    "parameters": "--map \"${MAP_DIR_PATH}/${MAP_FULL_NAME}\""
                }
            ]
        }
    ]
}
JSON
if python3 "$repo_root/tools/trenchbroom/lint_stellar_compilation_profiles.py" \
    --game-config "$package/GameConfig.cfg" \
    --profiles "$bad_tool_ref_profiles" >"$work_dir/bad_tool_ref_profiles.out" 2>&1; then
    fail "profile linter accepted old display-name tool reference"
fi
if ! grep -q 'tool reference contains spaces' "$work_dir/bad_tool_ref_profiles.out"; then
    fail "profile linter did not explain old display-name tool reference"
fi

copy_dest="$work_dir/Games With Spaces"
mkdir -p "$copy_dest"
install_output="$work_dir/install_output.txt"
"$install_helper" --repo-root "$repo_root" --dest "$copy_dest" --copy >"$install_output"
copied_package="$copy_dest/Stellar"
[[ -f "$copied_package/.stellar_repo_root" ]] || fail "copied package missing .stellar_repo_root"
if ! grep -Fq "STELLAR_BSP30_COMPILE  = $copied_package/bin/stellar_tb_compile.sh" \
    "$install_output"; then
    fail "install helper did not print compile shim tool path guidance"
fi
if ! grep -Fq "STELLAR_BSP30_VALIDATE = $copied_package/bin/stellar_tb_validate.sh" \
    "$install_output"; then
    fail "install helper did not print validate shim tool path guidance"
fi
STELLAR_REPO_ROOT="$repo_root" "$copied_package/bin/stellar_tb_compile.sh" --help >/dev/null
STELLAR_REPO_ROOT="$repo_root" "$copied_package/bin/stellar_tb_validate.sh" --help >/dev/null

if compiler_available && stellar_validation_available; then
    smoke_out="$work_dir/copied package compile/minimal_zup_room.bsp"
    mkdir -p "$(dirname "$smoke_out")"
    STELLAR_REPO_ROOT="$repo_root" \
        "$copied_package/bin/stellar_tb_compile.sh" \
        --map "$repo_root/tests/fixtures/trenchbroom/src/minimal_zup_room.map" \
        --out "$smoke_out" \
        --profile fast
    STELLAR_REPO_ROOT="$repo_root" \
        "$copied_package/bin/stellar_tb_validate.sh" --map "$smoke_out"
else
    printf 'Skipping copied-package compile sub-step: missing compiler or Stellar binaries.\n'
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

linked_fake_repo="$work_dir/Linked Fake Repo With Spaces"
linked_dest="$work_dir/Linked Games With Spaces"
mkdir -p \
    "$linked_fake_repo/tools/bsp" \
    "$linked_fake_repo/tools/trenchbroom" \
    "$linked_dest" \
    "$work_dir/Linked Map Dir With Spaces" \
    "$work_dir/Linked Out Dir With Spaces"
touch "$linked_fake_repo/CMakeLists.txt" "$work_dir/Linked Map Dir With Spaces/test map.map"
cp -a "$package" "$linked_fake_repo/tools/trenchbroom/Stellar"
rm -f "$linked_fake_repo/tools/trenchbroom/Stellar/.stellar_repo_root"
cat > "$linked_fake_repo/tools/bsp/validate_trenchbroom_bsp30.sh" <<'SH'
#!/usr/bin/env bash
exit 0
SH
cat > "$linked_fake_repo/tools/bsp/compile_trenchbroom_bsp30.sh" <<'SH'
#!/usr/bin/env bash
python3 - "$STELLAR_TB_ARG_CAPTURE" "$@" <<'PY'
from pathlib import Path
import sys
Path(sys.argv[1]).write_text("\n".join(sys.argv[2:]), encoding="utf-8")
PY
SH
chmod +x \
    "$linked_fake_repo/tools/bsp/compile_trenchbroom_bsp30.sh" \
    "$linked_fake_repo/tools/bsp/validate_trenchbroom_bsp30.sh"
"$install_helper" --repo-root "$linked_fake_repo" --dest "$linked_dest" --link \
    >"$work_dir/linked_install_output.txt"
linked_package="$linked_dest/Stellar"
[[ -L "$linked_package" ]] || fail "linked install did not create a Stellar symlink"
[[ ! -e "$linked_package/.stellar_repo_root" ]] || fail "linked package unexpectedly has .stellar_repo_root"
env -u STELLAR_REPO_ROOT "$linked_package/bin/stellar_tb_compile.sh" --help >/dev/null
env -u STELLAR_REPO_ROOT "$linked_package/bin/stellar_tb_validate.sh" --help >/dev/null
linked_arg_capture="$work_dir/linked-args.txt"
env -u STELLAR_REPO_ROOT STELLAR_TB_ARG_CAPTURE="$linked_arg_capture" \
    "$linked_package/bin/stellar_tb_compile.sh" \
    --map "$work_dir/Linked Map Dir With Spaces/test map.map" \
    --out "$work_dir/Linked Out Dir With Spaces/test map.bsp" \
    --profile fast
env -u STELLAR_REPO_ROOT \
    "$linked_package/bin/stellar_tb_validate.sh" \
    --map "$work_dir/Linked Out Dir With Spaces/test map.bsp"
python3 - "$linked_arg_capture" "$work_dir" <<'PY'
from pathlib import Path
import sys
args = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
work = Path(sys.argv[2])
expected = [
    "--map",
    str(work / "Linked Map Dir With Spaces" / "test map.map"),
    "--out",
    str(work / "Linked Out Dir With Spaces" / "test map.bsp"),
    "--profile",
    "fast",
]
if args != expected:
    raise SystemExit(f"linked quoted argument preservation failed: {args!r} != {expected!r}")
PY

fake_validate_repo="$work_dir/Fake Validate Repo With Spaces"
mkdir -p \
    "$fake_validate_repo/tools/bsp" \
    "$fake_validate_repo/build" \
    "$fake_validate_repo/build-linux" \
    "$fake_validate_repo/build-linux-vulkan-only" \
    "$fake_validate_repo/build-macos"
touch "$fake_validate_repo/CMakeLists.txt"
cp "$repo_root/tools/bsp/validate_trenchbroom_bsp30.sh" \
    "$fake_validate_repo/tools/bsp/validate_trenchbroom_bsp30.sh"
chmod +x "$fake_validate_repo/tools/bsp/validate_trenchbroom_bsp30.sh"
fake_bsp="$work_dir/fake-valid.bsp"
python3 - "$fake_bsp" <<'PY'
from pathlib import Path
import sys
Path(sys.argv[1]).write_bytes((30).to_bytes(4, "little") + b"fake bsp")
PY
make_fake_stellar_binary() {
    local path="$1"
    local label="$2"
    cat > "$path" <<SH
#!/usr/bin/env bash
printf '%s:%s\n' "\$(basename "\$0")" "$label" >> "\$STELLAR_TB_BINARY_CAPTURE"
exit 0
SH
    chmod +x "$path"
}
case "$(uname -s 2>/dev/null || printf 'unknown')" in
    Darwin) host_default_build="build-macos" ;;
    Linux) host_default_build="build-linux" ;;
    *) host_default_build="build" ;;
esac
for binary in stellar-client stellar-server; do
    make_fake_stellar_binary "$fake_validate_repo/build/$binary" "legacy-build"
    make_fake_stellar_binary "$fake_validate_repo/$host_default_build/$binary" "host-default"
    make_fake_stellar_binary "$fake_validate_repo/build-linux-vulkan-only/$binary" \
        "linux-vulkan-only"
done

default_capture="$work_dir/default-binary-resolution.txt"
env -u STELLAR_CLIENT -u STELLAR_SERVER -u STELLAR_BUILD_DIR -u STELLAR_CMAKE_PRESET \
    STELLAR_TB_BINARY_CAPTURE="$default_capture" \
    "$fake_validate_repo/tools/bsp/validate_trenchbroom_bsp30.sh" --map "$fake_bsp" \
    >/dev/null
if ! grep -qx 'stellar-client:host-default' "$default_capture"; then
    fail "validation wrapper did not prefer host-default build directory for stellar-client"
fi
if ! grep -qx 'stellar-server:host-default' "$default_capture"; then
    fail "validation wrapper did not prefer host-default build directory for stellar-server"
fi

preset_capture="$work_dir/preset-binary-resolution.txt"
env -u STELLAR_CLIENT -u STELLAR_SERVER -u STELLAR_BUILD_DIR \
    STELLAR_CMAKE_PRESET=linux-vulkan-only \
    STELLAR_TB_BINARY_CAPTURE="$preset_capture" \
    "$fake_validate_repo/tools/bsp/validate_trenchbroom_bsp30.sh" --map "$fake_bsp" \
    >/dev/null
if ! grep -qx 'stellar-client:linux-vulkan-only' "$preset_capture"; then
    fail "validation wrapper did not honor STELLAR_CMAKE_PRESET for stellar-client"
fi
if ! grep -qx 'stellar-server:linux-vulkan-only' "$preset_capture"; then
    fail "validation wrapper did not honor STELLAR_CMAKE_PRESET for stellar-server"
fi

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
