#!/usr/bin/env bash
set -u
set -o pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
repo_root="$(cd "${script_dir}/../.." && pwd -P)"

build_dir="${STELLAR_RUNTIME_BUILD_DIR:-build-macos-metal}"
if [[ "${build_dir}" != /* ]]; then
    build_dir="${repo_root}/${build_dir}"
fi
if [[ -d "${build_dir}" ]]; then
    build_dir="$(cd "${build_dir}" && pwd -P)"
fi

client="${STELLAR_CLIENT:-${build_dir}/stellar-client}"
server="${STELLAR_SERVER:-${build_dir}/stellar-server}"
renderer="${STELLAR_RUNTIME_RENDERER:-metal}"
smoke_seconds="${STELLAR_RUNTIME_SMOKE_SECONDS:-5}"
ctest_regex="${STELLAR_RUNTIME_CTEST_REGEX:-^(loopback_transport|transport_loopback|transport_socket|socket_transport|network_session|server_runtime|dedicated_server|listen_server_host|client_single_player_runtime|client_connect|client_world_receiver|client_map_validation_smoke|client_cli_validate_map)$}"

failures=0
skips=0

log() {
    printf 'macos-runtime-smoke: %s\n' "$*"
}

pass() {
    log "PASS $*"
}

skip() {
    skips=$((skips + 1))
    log "SKIP $*"
}

fail() {
    failures=$((failures + 1))
    log "FAIL $*"
}

run_step() {
    local label="$1"
    shift
    log "RUN ${label}: $*"
    if "$@"; then
        pass "${label}"
    else
        local status=$?
        fail "${label} (exit ${status})"
    fi
}

has_display() {
    if [[ "${STELLAR_SKIP_DISPLAY_SMOKE:-0}" == "1" ]]; then
        return 1
    fi
    if [[ "${STELLAR_FORCE_DISPLAY_SMOKE:-0}" == "1" ]]; then
        return 0
    fi

    case "$(uname -s)" in
    Darwin)
        ioreg -r -c IODisplayConnect 2>/dev/null | grep -q IODisplayConnect
        ;;
    *)
        [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]]
        ;;
    esac
}

run_for_duration() {
    local label="$1"
    shift
    log "RUN ${label} for ${smoke_seconds}s: $*"
    "$@" &
    local pid=$!
    sleep "${smoke_seconds}"
    if kill -0 "${pid}" 2>/dev/null; then
        kill "${pid}" 2>/dev/null || true
        wait "${pid}" 2>/dev/null || true
        pass "${label}"
        return
    fi

    wait "${pid}"
    local status=$?
    if [[ "${status}" -eq 0 ]]; then
        pass "${label}"
    else
        fail "${label} exited before smoke window (exit ${status})"
    fi
}

choose_map() {
    if [[ -n "${STELLAR_RUNTIME_SMOKE_MAP:-}" ]]; then
        printf '%s\n' "${STELLAR_RUNTIME_SMOKE_MAP}"
        return
    fi

    local candidate
    for candidate in \
        "${build_dir}/tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp" \
        "${build_dir}/tests/fixtures/trenchbroom/compiled/lit_zup_room.bsp" \
        "${repo_root}/tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp" \
        "${repo_root}/tests/fixtures/trenchbroom/compiled/lit_zup_room.bsp"; do
        if [[ -f "${candidate}" ]]; then
            printf '%s\n' "${candidate}"
            return
        fi
    done
}

if [[ ! -x "${client}" ]]; then
    fail "stellar-client not found or not executable at ${client}"
fi
if [[ ! -x "${server}" ]]; then
    fail "stellar-server not found or not executable at ${server}"
fi
if [[ ! -d "${build_dir}" ]]; then
    fail "build directory not found at ${build_dir}"
fi

map_path="$(choose_map)"
map_abs=""
if [[ -n "${map_path}" && -f "${map_path}" ]]; then
    map_abs="$(cd "$(dirname "${map_path}")" && pwd -P)/$(basename "${map_path}")"
fi

if [[ "${failures}" -eq 0 ]]; then
    run_step "client validate-config ${renderer}" \
        "${client}" --validate-config --renderer "${renderer}"
fi

if [[ -n "${map_abs}" && "${failures}" -eq 0 ]]; then
    build_rel_map="tests/fixtures/trenchbroom/compiled/$(basename "${map_abs}")"

    if [[ "${map_abs}" == "${repo_root}/"* ]]; then
        repo_rel_map="${map_abs#"${repo_root}/"}"
    else
        repo_rel_map="${map_abs}"
    fi

    run_step "client validate-map from repo root" \
        bash -c 'cd "$1" && "$2" --validate-map "$3" --renderer "$4"' \
        bash "${repo_root}" "${client}" "${repo_rel_map}" "${renderer}"

    if [[ -f "${build_dir}/${build_rel_map}" ]]; then
        run_step "client validate-map from build directory" \
            bash -c 'cd "$1" && "$2" --validate-map "$3" --renderer "$4"' \
            bash "${build_dir}" "${client}" "${build_rel_map}" "${renderer}"
    else
        skip "build-relative validate-map fixture unavailable at ${build_rel_map}"
    fi

    run_step "server validate-config map" \
        "${server}" --validate-config --map "${map_abs}" --listen 127.0.0.1:0
else
    skip "validate-map/server map smoke fixture unavailable; set STELLAR_RUNTIME_SMOKE_MAP"
fi

if [[ -d "${build_dir}" ]]; then
    run_step "display-free runtime CTest slice" \
        ctest --test-dir "${build_dir}" -R "${ctest_regex}" --output-on-failure
fi

if [[ -n "${map_abs}" ]]; then
    if has_display; then
        run_for_duration "single-player ${renderer} display smoke" \
            env STELLAR_DEBUG_RENDER=1 STELLAR_DEBUG_RENDER_FRAMES=1 \
            "${client}" --map "${map_abs}" --renderer "${renderer}"
    else
        skip "single-player ${renderer} display smoke requires an attached display; set STELLAR_FORCE_DISPLAY_SMOKE=1 to force"
    fi
else
    skip "single-player display smoke fixture unavailable"
fi

if [[ "${failures}" -ne 0 ]]; then
    log "completed with ${failures} failure(s) and ${skips} skip(s)"
    exit 1
fi

log "completed with ${skips} skip(s)"
exit 0
