# Project Stellar — SDL/X11 Client Startup Failure Investigation Plan

Branch: `trenchbroom-compat`  
Issue observed: running `build/stellar-client --map <compiled.bsp>` prints:

```text
Authorization required, but no authorization protocol specified

Client startup failed: SDL initialization failed
```

Purpose: provide an AI-agent-optimized investigation and remediation plan. Do **not** commit without user approval.

---

## 0. Executive Summary

The most probable root cause is not the BSP map itself. The error text is a Linux display-server authorization failure emitted by the display stack, usually X11/Xwayland, before or during `SDL_Init(SDL_INIT_VIDEO)`.

The current code makes this hard to diagnose because `Window::create()` collapses the real SDL error into a generic `Error("SDL initialization failed")` and does not append `SDL_GetError()`. The client also creates an SDL window for every non-validate run, so `stellar-client --map ...` requires an authorized GUI/display session even though `--validate-map` remains display-free.

Primary goals for agents:

1. Reproduce and classify the environment failure.
2. Add actionable SDL/display diagnostics.
3. Document user-facing launch fixes.
4. Add opt-in display smoke tests that skip cleanly when no GUI authorization exists.
5. Preserve display-free map validation and CI behavior.

---

## 1. Evidence From Current Branch

### 1.1 Startup path

`stellar-client` parses config, constructs `Application`, and prints:

```cpp
Client startup failed: <error>
```

when `Application::run()` returns an error.

Relevant files:

- `src/client/main.cpp`
- `src/client/Application.cpp`
- `src/platform/Window.cpp`
- `src/client/ApplicationConfig.cpp`

### 1.2 Display-free validation exists

`Application::run()` calls `prepare_application_runtime(config_)` first. If `config_.validate_only` is true, it returns before creating a window. Therefore:

```bash
build/stellar-client --validate-map maps/compiled/test_room.bsp
```

should remain the first control test. It should not require SDL video/window creation.

### 1.3 Live client always creates a real SDL window

For non-validate mode, `Application::run()` creates a `Window` with either `SDL_WINDOW_OPENGL` or `SDL_WINDOW_VULKAN`.

Relevant behavior:

```cpp
const Uint32 backend_window_flags =
    config_.graphics_backend == stellar::graphics::GraphicsBackend::kVulkan
        ? SDL_WINDOW_VULKAN
        : SDL_WINDOW_OPENGL;

window.create(1280, 720, "Stellar Engine", SDL_WINDOW_SHOWN | backend_window_flags);
```

Thus:

```bash
build/stellar-client --map maps/compiled/test_room.bsp
```

requires an interactive GUI-capable environment.

### 1.4 Error masking in `Window::create()`

Current code:

```cpp
if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    return std::unexpected(Error("SDL initialization failed"));
}

window_ = SDL_CreateWindow(...);

if (!window_) {
    return std::unexpected(Error("Failed to create SDL window"));
}
```

This loses `SDL_GetError()`, the active `SDL_VIDEODRIVER`, and the display environment. That is why users see a generic failure after the underlying display system prints:

```text
Authorization required, but no authorization protocol specified
```

---

## 2. Probable Root Causes To Check First

Treat these as ordered hypotheses.

### H1 — Client is being run as a different user than the desktop session

Most common examples:

```bash
sudo build/stellar-client --map ...
su -
doas build/stellar-client --map ...
```

The X server may reject the process because the new user lacks the active user’s Xauthority cookie. The exact message `Authorization required, but no authorization protocol specified` commonly appears here.

Agent checks:

```bash
whoami
id
echo "DISPLAY=$DISPLAY"
echo "XAUTHORITY=${XAUTHORITY:-}"
ls -l "${XAUTHORITY:-$HOME/.Xauthority}" 2>/dev/null || true
```

Expected fix for user workflow: do not run the game client with `sudo`. Run it as the same user who owns the desktop login.

Temporary diagnostic only:

```bash
sudo --preserve-env=DISPLAY,XAUTHORITY build/stellar-client --map maps/compiled/test_room.bsp
```

Prefer avoiding root entirely.

### H2 — `DISPLAY` points at an X server but no valid Xauthority cookie is available

Agent checks:

```bash
echo "$DISPLAY"
xauth list "$DISPLAY" 2>/dev/null || xauth list 2>/dev/null || true
```

Potential user-side fix:

```bash
export XAUTHORITY="$HOME/.Xauthority"
```

If running inside a container or non-login shell, explicitly pass/mount Xauthority.

### H3 — Wayland/Xwayland mismatch

SDL may default to x11 under Xwayland while the session is Wayland-only or missing proper Xwayland authorization.

Agent checks:

```bash
echo "XDG_SESSION_TYPE=${XDG_SESSION_TYPE:-}"
echo "WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-}"
echo "XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-}"
echo "SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-}"
```

Try:

```bash
SDL_VIDEODRIVER=wayland build/stellar-client --map maps/compiled/test_room.bsp
SDL_VIDEODRIVER=x11 build/stellar-client --map maps/compiled/test_room.bsp
```

Record which one changes the failure.

### H4 — SSH, container, sandbox, or non-interactive terminal has no display access

Agent checks:

```bash
env | grep -E 'DISPLAY|WAYLAND|XAUTHORITY|XDG_RUNTIME_DIR|SSH'
```

If SSH:

```bash
ssh -Y <host>
```

If container:

- Pass `DISPLAY`.
- Mount `/tmp/.X11-unix`.
- Pass or mount `.Xauthority`.
- Run as the desktop user UID if possible.
- Avoid `xhost +` except as a last-resort local diagnostic.

### H5 — OpenGL/Vulkan context creation issue occurs after SDL window creation

This is a second-stage problem, not the current visible one if `SDL_Init` fails. Still, improve diagnostics for:

- `SDL_CreateWindow`
- `SDL_GL_CreateContext`
- Vulkan surface creation paths, if present

because those can produce similarly opaque startup errors.

---

## 3. Immediate User Workarounds To Verify Map Is Not The Problem

Run these from the repo root.

### 3.1 Validate BSP without opening a window

```bash
build/stellar-client --validate-map maps/compiled/test_room.bsp
build/stellar-server --validate-config --map maps/compiled/test_room.bsp
```

Expected: both succeed if the map is valid.

### 3.2 Run client as the desktop user, not root

```bash
whoami
build/stellar-client --map maps/compiled/test_room.bsp
```

If this works, root/sudo/Xauthority was the cause.

### 3.3 Try explicit SDL video drivers

```bash
SDL_VIDEODRIVER=x11 build/stellar-client --map maps/compiled/test_room.bsp
SDL_VIDEODRIVER=wayland build/stellar-client --map maps/compiled/test_room.bsp
```

Record exact stderr.

### 3.4 Try Vulkan if OpenGL context is later failing

```bash
build/stellar-client --renderer vulkan --map maps/compiled/test_room.bsp
```

or:

```bash
build/stellar-client --graphics-backend vulkan --map maps/compiled/test_room.bsp
```

Only do this after SDL window creation is fixed.

---

## 4. Code Fix Plan

### Phase SDL-1 — Preserve Real SDL Error Text

Owner: platform/core agent  
Files:

- `src/platform/Window.cpp`
- `include/stellar/platform/Window.hpp`
- tests if available

Required change:

- On `SDL_Init(SDL_INIT_VIDEO)` failure, include `SDL_GetError()`.
- On `SDL_CreateWindow` failure, include `SDL_GetError()`.
- Ensure empty SDL errors still produce a stable fallback.

Example target behavior:

```text
Client startup failed: SDL video initialization failed: x11 not available
```

or:

```text
Client startup failed: SDL video initialization failed: Authorization required, but no authorization protocol specified
```

Implementation sketch:

```cpp
namespace {
std::string sdl_error_message(std::string_view prefix) {
    const char* raw = SDL_GetError();
    std::string message(prefix);
    if (raw != nullptr && raw[0] != '\0') {
        message += ": ";
        message += raw;
    }
    return message;
}
}
```

Use:

```cpp
return std::unexpected(Error(sdl_error_message("SDL video initialization failed")));
```

and:

```cpp
return std::unexpected(Error(sdl_error_message("Failed to create SDL window")));
```

Acceptance:

- Unit or smoke test confirms returned error includes a prefix and SDL detail when available.
- No display-free test starts requiring a display.

### Phase SDL-2 — Add Display Environment Diagnostics

Owner: platform/client agent  
Files:

- `src/platform/Window.cpp` or new `src/platform/DisplayDiagnostics.cpp`
- `include/stellar/platform/DisplayDiagnostics.hpp`
- docs

Add a small helper that captures:

- `DISPLAY`
- `WAYLAND_DISPLAY`
- `XDG_SESSION_TYPE`
- `XDG_RUNTIME_DIR`
- `XAUTHORITY`
- `SDL_VIDEODRIVER`
- effective uid/user if portable enough on Linux
- whether running as root on Linux

Do not log secrets or Xauthority file contents.

Append a compact diagnostic block to SDL startup failures, for example:

```text
Display environment: DISPLAY=:0, WAYLAND_DISPLAY=wayland-0, XAUTHORITY unset, SDL_VIDEODRIVER unset
Hint: run as the desktop user, or set STELLAR_REPO_ROOT/XAUTHORITY/DISPLAY when launching from a copied package or non-login shell.
```

Acceptance:

- Errors are actionable without being verbose.
- Does not break non-Linux builds.
- Does not expose authentication token contents.

### Phase SDL-3 — Improve OpenGL/Vulkan Context Errors

Owner: graphics agent  
Files:

- `src/graphics/opengl/OpenGLGraphicsDevice.cpp`
- Vulkan device/surface creation files as applicable

Required changes:

- Include `SDL_GetError()` when `SDL_GL_CreateContext` fails.
- Include selected backend and SDL video driver in renderer initialization failures.
- For Vulkan surface creation, include the Vulkan result code and SDL/Vulkan surface creation error where available.

Acceptance:

- Failure messages distinguish:
  - SDL video init failure
  - SDL window creation failure
  - OpenGL context creation failure
  - Vulkan device/surface failure

### Phase SDL-4 — Add CLI-Level Display Smoke Mode

Owner: client/platform agent  
Files:

- `include/stellar/client/ApplicationConfig.hpp`
- `src/client/ApplicationConfig.cpp`
- `src/client/Application.cpp`
- tests/client as needed

Add optional CLI:

```bash
build/stellar-client --validate-display
```

Behavior:

- Creates an SDL window and selected graphics backend enough to verify display/context startup.
- Does not require a BSP map.
- Exits successfully after initialization.
- Fails with the improved display diagnostics.

Alternative name acceptable:

```bash
build/stellar-client --window-smoke
```

Do not overload `--validate-map`; keep `--validate-map` display-free.

Acceptance:

- `--validate-map` remains display-free.
- `--validate-display` clearly tests GUI/display startup.
- CI does not run this by default unless opt-in.

### Phase SDL-5 — Add Opt-In Display CTest

Owner: test/build agent  
Files:

- `CMakeLists.txt`
- tests/client or tests/graphics

Add tests only behind existing or new option:

```cmake
option(STELLAR_ENABLE_DISPLAY_STARTUP_TESTS "Run SDL display startup smoke tests" OFF)
```

Test behavior:

- If no `DISPLAY` and no `WAYLAND_DISPLAY`, exit 77.
- If display exists but authorization fails, fail with improved diagnostics.
- Keep default CI display-free.

Acceptance:

- Default `ctest` remains display-free.
- Opt-in test catches this exact class of issue on developer machines.

### Phase SDL-6 — Documentation and Manual QA Update

Owner: docs/tooling agent  
Files:

- `docs/TrenchBroom.md`
- `docs/TrenchBroomManualQA.md`
- maybe `README.md`

Add troubleshooting section:

```markdown
### Client launch fails with "Authorization required" / "SDL initialization failed"

This is a display server authorization issue, not a map compile issue. First run:
build/stellar-client --validate-map <map.bsp>
build/stellar-server --validate-config --map <map.bsp>

Then launch from a desktop terminal as the desktop user, not sudo/root.
Check DISPLAY, WAYLAND_DISPLAY, XAUTHORITY, and SDL_VIDEODRIVER.
```

Include quick commands:

```bash
echo "$DISPLAY"
echo "$WAYLAND_DISPLAY"
echo "$XAUTHORITY"
whoami
SDL_VIDEODRIVER=x11 build/stellar-client --map <map.bsp>
SDL_VIDEODRIVER=wayland build/stellar-client --map <map.bsp>
```

Acceptance:

- Docs distinguish map validation from live GUI launch.
- Docs do not recommend unsafe `xhost +` as a default fix.
- Manual QA template includes display environment fields.

---

## 5. Investigation Runbook For Agents

Run and capture all output.

### 5.1 Baseline branch state

```bash
git status --short
git branch --show-current
git rev-parse HEAD
```

### 5.2 Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

### 5.3 Map validity control

```bash
build/stellar-client --validate-map maps/compiled/test_room.bsp
build/stellar-server --validate-config --map maps/compiled/test_room.bsp
```

If this fails, stop and investigate map/import validation separately.

### 5.4 Display environment capture

```bash
whoami
id
env | sort | grep -E 'DISPLAY|WAYLAND|XAUTHORITY|XDG_SESSION_TYPE|XDG_RUNTIME_DIR|SDL|SSH|SUDO' || true
ls -l "${XAUTHORITY:-$HOME/.Xauthority}" 2>/dev/null || true
```

### 5.5 Reproduce live client failure

```bash
build/stellar-client --map maps/compiled/test_room.bsp
```

Then:

```bash
SDL_VIDEODRIVER=x11 build/stellar-client --map maps/compiled/test_room.bsp
SDL_VIDEODRIVER=wayland build/stellar-client --map maps/compiled/test_room.bsp
```

### 5.6 Root/sudo check

If failure happened under sudo/root, retest as normal user.

```bash
sudo -n true 2>/dev/null && echo "sudo available"
```

Do not use sudo for the normal client launch.

### 5.7 Optional graphics backend check

Only after SDL video init succeeds:

```bash
build/stellar-client --renderer opengl --map maps/compiled/test_room.bsp
build/stellar-client --renderer vulkan --map maps/compiled/test_room.bsp
```

---

## 6. Expected Final Deliverables

1. Patch with improved SDL/window/context diagnostics.
2. Optional `--validate-display` or `--window-smoke` CLI.
3. Opt-in display startup CTest with skip code 77 when no display exists.
4. Updated TrenchBroom troubleshooting docs.
5. Short handoff note with:
   - exact environment where error reproduced
   - whether map validation passed
   - whether issue was root/Xauthority/Wayland/SSH/container
   - final command that launches successfully

---

## 7. Non-Goals

- Do not make the live rendered client display-free.
- Do not require GUI/display tests in default CI.
- Do not disable SDL/OpenGL/Vulkan initialization failures.
- Do not recommend global unsafe X server access (`xhost +`) as normal guidance.
- Do not treat a display authorization failure as a BSP/TrenchBroom map compile failure.

---

## 8. Quick User-Facing Summary

The likely immediate answer is:

```bash
# Confirm map is valid without a GUI:
build/stellar-client --validate-map maps/compiled/test_room.bsp

# Then launch from a normal desktop terminal as your logged-in user:
build/stellar-client --map maps/compiled/test_room.bsp
```

Avoid:

```bash
sudo build/stellar-client --map maps/compiled/test_room.bsp
```

If still failing, try:

```bash
echo "$DISPLAY"
echo "$WAYLAND_DISPLAY"
echo "$XAUTHORITY"
SDL_VIDEODRIVER=x11 build/stellar-client --map maps/compiled/test_room.bsp
SDL_VIDEODRIVER=wayland build/stellar-client --map maps/compiled/test_room.bsp
```
