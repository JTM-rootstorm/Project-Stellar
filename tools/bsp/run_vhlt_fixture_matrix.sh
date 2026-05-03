#!/usr/bin/env bash
set -uo pipefail

usage() {
    cat <<'USAGE'
Usage: tools/bsp/run_vhlt_fixture_matrix.sh [options]

Options:
  --source-root <repo>       Repository root (default: inferred from this script).
  --build-root <build>       Build root (default: <source-root>/build).
  --profile full|fast        VHLT compile profile (default: full).
  --keep-going               Continue after fixture failures and report all results.
  --fixture <name>           Run one fixture from the matrix.
  --list                     List fixture names and expected outcomes, then exit.
  --stellar-client <path>    Path to stellar-client for validation.
  --stellar-server <path>    Path to stellar-server for validation.
  --help, -h                 Show this help.

Environment overrides:
  STELLAR_VHLT_DIR           Directory containing hlcsg, hlbsp, hlvis, hlrad, and optional ripent.
  HLCSG, HLBSP, HLVIS, HLRAD Explicit per-tool paths.
  STELLAR_CLIENT             Path to stellar-client for validation.
  STELLAR_SERVER             Path to stellar-server for validation.
USAGE
}

fail() {
    printf 'run_vhlt_fixture_matrix.sh: %s\n' "$1" >&2
    exit 1
}

script_source_root() {
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" || return 1
    cd "$script_dir/../.." && pwd
}

absolute_existing_dir() {
    local path="$1"
    [[ -d "$path" ]] || fail "directory does not exist: $path"
    cd "$path" && pwd
}

absolute_path_for_parent() {
    local path="$1"
    local dir
    local base
    dir="$(dirname "$path")"
    base="$(basename "$path")"
    mkdir -p "$dir" || return 1
    dir="$(cd "$dir" && pwd)" || return 1
    printf '%s/%s\n' "$dir" "$base"
}

find_tool() {
    local tool_name="$1"
    local override_value="$2"
    local root="$3"
    local candidate

    if [[ -n "$override_value" ]]; then
        [[ -x "$override_value" ]] && absolute_path_for_parent "$override_value" && return 0
        return 1
    fi

    if [[ -n "${STELLAR_VHLT_DIR:-}" ]]; then
        candidate="$STELLAR_VHLT_DIR/$tool_name"
        [[ -x "$candidate" ]] && absolute_path_for_parent "$candidate" && return 0
    fi

    for candidate in \
        "$root/tools/bsp/$tool_name" \
        "$root/tools/bsp/vhlt/$tool_name" \
        "$root/tools/bsp/bin/$tool_name"; do
        [[ -x "$candidate" ]] && absolute_path_for_parent "$candidate" && return 0
    done

    if command -v "$tool_name" >/dev/null 2>&1; then
        command -v "$tool_name"
        return 0
    fi

    return 1
}

skip_missing_vhlt_tools() {
    local root="$1"
    local missing=()

    find_tool hlcsg "${HLCSG:-}" "$root" >/dev/null || missing+=(hlcsg)
    find_tool hlbsp "${HLBSP:-}" "$root" >/dev/null || missing+=(hlbsp)
    if [[ "$profile" == "full" ]]; then
        find_tool hlvis "${HLVIS:-}" "$root" >/dev/null || missing+=(hlvis)
        find_tool hlrad "${HLRAD:-}" "$root" >/dev/null || missing+=(hlrad)
    fi

    if (( ${#missing[@]} > 0 )); then
        printf 'SKIP: required VHLT tools are absent or not executable: %s\n' "${missing[*]}"
        exit 77
    fi
}

run_and_log() {
    local log_path="$1"
    shift
    mkdir -p "$(dirname "$log_path")"
    printf '+ %s\n' "$*" >"$log_path"
    "$@" >>"$log_path" 2>&1
}

record_result() {
    local fixture="$1"
    local status="$2"
    local detail="$3"
    results+=("$fixture:$status:$detail")
    printf '%-30s %s %s\n' "$fixture" "$status" "$detail" | tee -a "$summary_log"
}

copy_wrapper_work_dir() {
    local fixture="$1"
    local wrapper_work_dir="$source_root/build/tests/fixtures/trenchbroom/vhlt/work/$fixture"
    local target_work_dir="$work_dir/$fixture"

    if [[ "$wrapper_work_dir" == "$target_work_dir" || ! -d "$wrapper_work_dir" ]]; then
        return 0
    fi

    rm -rf "$target_work_dir"
    mkdir -p "$(dirname "$target_work_dir")"
    cp -a "$wrapper_work_dir" "$target_work_dir"
}

run_positive_fixture() {
    local fixture="$1"
    local map_path="$src_dir/$fixture.map"
    local out_path="$compiled_dir/$fixture.bsp"
    local fixture_log_dir="$logs_dir/$fixture"
    local compile_log="$fixture_log_dir/matrix_compile.log"
    local validate_log="$fixture_log_dir/matrix_validate.log"

    [[ -f "$map_path" ]] || {
        record_result "$fixture" "FAIL" "missing source fixture: $map_path"
        return 1
    }

    rm -f "$out_path"
    mkdir -p "$fixture_log_dir"
    if ! STELLAR_VHLT_KEEP_WORK=1 STELLAR_VHLT_WORK_ROOT="$work_dir" \
        STELLAR_VHLT_LOG_DIR="$fixture_log_dir" \
        run_and_log "$compile_log" bash "$compile_script" \
            --map "$map_path" --out "$out_path" --profile "$profile" \
            --no-stellar-validation; then
        record_result "$fixture" "FAIL" "VHLT compile failed; log=$compile_log"
        return 1
    fi
    copy_wrapper_work_dir "$fixture"

    mkdir -p "$compiled_dir/scripts"
    cp -f "$source_root/tests/fixtures/trenchbroom/scripts/gate.lua" "$compiled_dir/scripts/"
    cp -f "$source_root/tests/fixtures/trenchbroom/scripts/pickup.lua" "$compiled_dir/scripts/"

    if ! run_and_log "$validate_log" bash "$validate_script" --map "$out_path"; then
        record_result "$fixture" "FAIL" "Stellar validation failed; log=$validate_log"
        return 1
    fi

    record_result "$fixture" "PASS" "compiled=$out_path logs=$fixture_log_dir"
    return 0
}

run_negative_fixture() {
    local fixture="$1"
    local expected_regex="$2"
    local strict_textures="${3:-0}"
    local map_path="$src_dir/$fixture.map"
    local out_path="$compiled_dir/$fixture.bsp"
    local fixture_log_dir="$logs_dir/$fixture"
    local compile_log="$fixture_log_dir/matrix_compile.log"
    local validate_log="$fixture_log_dir/matrix_validate_expected_failure.log"
    local preflight_log="$fixture_log_dir/matrix_source_preflight_expected_failure.log"

    [[ -f "$map_path" ]] || {
        record_result "$fixture" "FAIL" "missing source fixture: $map_path"
        return 1
    }

    rm -f "$out_path"
    mkdir -p "$fixture_log_dir"
    local preflight_args=(python3 "$source_preflight_script")
    if [[ "$strict_textures" == "1" ]]; then
        preflight_args+=(--strict-textures --wad-root "$source_root/tools/trenchbroom/Stellar/materials")
    fi
    preflight_args+=("$map_path")

    if run_and_log "$preflight_log" "${preflight_args[@]}"; then
        record_result "$fixture" "FAIL" "source preflight unexpectedly passed; log=$preflight_log"
        return 1
    fi
    if grep -Eiq "$expected_regex" "$preflight_log"; then
        record_result "$fixture" "PASS" "source preflight failed for expected diagnostic; logs=$fixture_log_dir"
        return 0
    fi

    if [[ "$fixture" != "invalid_script_escape_zup" ]]; then
        record_result "$fixture" "FAIL" "expected source preflight diagnostic missing; log=$preflight_log"
        return 1
    fi

    if ! STELLAR_VHLT_KEEP_WORK=1 STELLAR_VHLT_WORK_ROOT="$work_dir" \
        STELLAR_VHLT_LOG_DIR="$fixture_log_dir" \
        run_and_log "$compile_log" bash "$compile_script" \
            --map "$map_path" --out "$out_path" --profile "$profile" \
            --skip-source-preflight \
            --no-stellar-validation; then
        record_result "$fixture" "FAIL" "VHLT compile failed before expected validation failure; log=$compile_log"
        return 1
    fi
    copy_wrapper_work_dir "$fixture"

    if run_and_log "$validate_log" bash "$validate_script" --map "$out_path"; then
        record_result "$fixture" "FAIL" "validation unexpectedly passed; log=$validate_log"
        return 1
    fi

    if ! grep -Eiq "$expected_regex|script.*escape|kScriptPathEscape|path.*escape" "$validate_log"; then
        record_result "$fixture" "FAIL" "expected script path escape diagnostic missing; log=$validate_log"
        return 1
    fi

    record_result "$fixture" "PASS" "validation failed for expected script path escape; logs=$fixture_log_dir"
    return 0
}

source_root=""
build_root=""
profile="full"
keep_going="0"
stellar_client_arg=""
stellar_server_arg=""
selected_fixture=""
list_only="0"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --source-root)
            [[ $# -ge 2 ]] || fail "--source-root requires a path"
            source_root="$2"
            shift 2
            ;;
        --build-root)
            [[ $# -ge 2 ]] || fail "--build-root requires a path"
            build_root="$2"
            shift 2
            ;;
        --profile)
            [[ $# -ge 2 ]] || fail "--profile requires full or fast"
            profile="$2"
            shift 2
            ;;
        --keep-going)
            keep_going="1"
            shift
            ;;
        --fixture)
            [[ $# -ge 2 ]] || fail "--fixture requires a fixture name"
            selected_fixture="$2"
            shift 2
            ;;
        --list)
            list_only="1"
            shift
            ;;
        --stellar-client)
            [[ $# -ge 2 ]] || fail "--stellar-client requires a path"
            stellar_client_arg="$2"
            shift 2
            ;;
        --stellar-server)
            [[ $# -ge 2 ]] || fail "--stellar-server requires a path"
            stellar_server_arg="$2"
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

[[ "$profile" == "full" || "$profile" == "fast" ]] || fail "unsupported profile: $profile"

if [[ -z "$source_root" ]]; then
    source_root="$(script_source_root)" || fail "could not infer source root"
else
    source_root="$(absolute_existing_dir "$source_root")"
fi

if [[ -z "$build_root" ]]; then
    build_root="$source_root/build"
fi
build_root="$(absolute_path_for_parent "$build_root/.anchor")"
build_root="$(dirname "$build_root")"
mkdir -p "$build_root"

compile_script="$source_root/tools/bsp/compile_vhlt_bsp30.sh"
validate_script="$source_root/tools/bsp/validate_trenchbroom_bsp30.sh"
source_preflight_script="$source_root/tools/bsp/validate_trenchbroom_map_source.py"
src_dir="$source_root/tests/fixtures/trenchbroom/src"
compiled_dir="$build_root/tests/fixtures/trenchbroom/vhlt/compiled"
logs_dir="$build_root/tests/fixtures/trenchbroom/vhlt/logs"
work_dir="$build_root/tests/fixtures/trenchbroom/vhlt/work"
summary_log="$logs_dir/matrix_summary.log"
results=()
failures=0
positive_fixtures=(
    minimal_zup_room
    entity_matrix_zup
    scripted_interaction_zup
    lit_zup_room
    texture_axes_zup
    material_wad_zup
    moving_door_button_zup
    point_volume_zup
    illusionary_static_zup
)
negative_fixtures=(
    invalid_script_escape_zup
    invalid_incomplete_brush
    invalid_malformed_brush
    invalid_missing_target
    invalid_missing_wad_texture
)
declare -A negative_regex=(
    [invalid_script_escape_zup]='script.*path|path.*escape|absolute|\.\.'
    [invalid_incomplete_brush]='fewer than 4 planes|unclosed|expected Valve 220'
    [invalid_malformed_brush]='fewer than 4 planes|unclosed|expected Valve 220|closing brace'
    [invalid_missing_target]='target.*does not match|MissingDoor'
    [invalid_missing_wad_texture]='texture.*not.*resolvable|missing_tex|worldspawn wad'
)
declare -A negative_strict_textures=(
    [invalid_missing_wad_texture]=1
)

if [[ "$list_only" == "1" ]]; then
    printf 'Positive fixtures:\n'
    printf '  %s\n' "${positive_fixtures[@]}"
    printf 'Negative fixtures:\n'
    printf '  %s\n' "${negative_fixtures[@]}"
    exit 0
fi

[[ -f "$compile_script" ]] || fail "missing compile wrapper: $compile_script"
[[ -f "$validate_script" ]] || fail "missing validation wrapper: $validate_script"
[[ -f "$source_preflight_script" ]] || fail "missing source preflight: $source_preflight_script"

skip_missing_vhlt_tools "$source_root"

mkdir -p "$compiled_dir" "$logs_dir" "$work_dir"
: >"$summary_log"
printf 'VHLT fixture matrix: source_root=%s build_root=%s profile=%s\n' \
    "$source_root" "$build_root" "$profile" | tee -a "$summary_log"
printf 'Outputs: compiled=%s logs=%s work=%s\n' \
    "$compiled_dir" "$logs_dir" "$work_dir" | tee -a "$summary_log"

export STELLAR_CLIENT="${stellar_client_arg:-${STELLAR_CLIENT:-}}"
export STELLAR_SERVER="${stellar_server_arg:-${STELLAR_SERVER:-}}"

for fixture in "${positive_fixtures[@]}"; do
    [[ -z "$selected_fixture" || "$selected_fixture" == "$fixture" ]] || continue
    if ! run_positive_fixture "$fixture"; then
        failures=$((failures + 1))
        [[ "$keep_going" == "1" ]] || exit 1
    fi
done

for fixture in "${negative_fixtures[@]}"; do
    [[ -z "$selected_fixture" || "$selected_fixture" == "$fixture" ]] || continue
    if ! run_negative_fixture "$fixture" "${negative_regex[$fixture]}" "${negative_strict_textures[$fixture]:-0}"; then
        failures=$((failures + 1))
        [[ "$keep_going" == "1" ]] || exit 1
    fi
done

if [[ -n "$selected_fixture" && ${#results[@]} -eq 0 ]]; then
    fail "unknown fixture: $selected_fixture"
fi

printf 'Summary log: %s\n' "$summary_log"

if (( failures > 0 )); then
    printf 'VHLT fixture matrix failed: %d fixture(s) failed.\n' "$failures" >&2
    exit 1
fi

printf 'VHLT fixture matrix passed: %d fixture(s) passed.\n' "${#results[@]}"
