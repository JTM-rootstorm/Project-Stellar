# AI Agent Implementation Plan: Split Test CMake From Root CMakeLists

**Repository:** `JTM-rootstorm/Project-Stellar`  
**Target branch:** `fixes`  
**Plan date:** 2026-05-03  
**Primary objective:** Reduce the size and responsibility of the root `CMakeLists.txt` by moving test-only target declarations, helper functions, and CTest registrations into the `tests/` tree while preserving current build/test behavior.

---

## 1. Operating Constraints

1. Work on branch `fixes`.
2. Do not change runtime code, gameplay code, graphics code, BSP importer code, or test source code unless a CMake issue proves it is required.
3. Keep production target definitions in the repository root for now.
4. Move test-related CMake only:
   - test options,
   - test helper functions,
   - test executable definitions,
   - `add_test(...)` registrations,
   - `set_tests_properties(...)` calls for tests,
   - test fixture-writer target setup.
5. Preserve the current test names so existing CI filters and developer workflows keep working.
6. Preserve the current fixture output locations under `${CMAKE_BINARY_DIR}/tests/fixtures/...`.
7. Prefer `PROJECT_SOURCE_DIR` over `CMAKE_SOURCE_DIR` in new CMake code where possible.
8. Do not commit automatically unless the user explicitly asks for a commit.

---

## 2. Current State Summary

The root `CMakeLists.txt` currently handles too many responsibilities:

- project declaration and C++ standard setup,
- dependency discovery,
- third-party subdirectories,
- production library targets,
- client/server executable targets,
- reusable test helper functions,
- dozens of unit/integration/smoke test target declarations,
- TrenchBroom/BSP fixture generation tests,
- CTest property wiring.

The test section begins very early with test helper functions and continues across most of the file. This makes the root file difficult to scan because production build structure and test registration are interleaved.

---

## 3. Target Structure

Implement the following minimal structure:

```text
Project-Stellar/
├── CMakeLists.txt
└── tests/
    └── CMakeLists.txt
```

Optional follow-up structure if `tests/CMakeLists.txt` is still considered too large after the initial split:

```text
Project-Stellar/
├── CMakeLists.txt
└── tests/
    ├── CMakeLists.txt
    └── cmake/
        ├── TestHelpers.cmake
        ├── UnitTests.cmake
        ├── ClientServerTests.cmake
        ├── BspFixtureTests.cmake
        └── TrenchBroomTests.cmake
```

For this task, implement the minimal structure first. Do not over-split unless the root split is already working.

---

## 4. Root `CMakeLists.txt` Changes

### 4.1 Replace unconditional testing setup with standard CTest gate

Near the top of the root file, replace:

```cmake
enable_testing()
```

with:

```cmake
include(CTest)
```

This preserves default test enablement through CMake’s standard `BUILD_TESTING` option while allowing developers or packagers to configure with:

```bash
cmake -S . -B build -DBUILD_TESTING=OFF
```

### 4.2 Remove test-only options from root

Move these options out of the root file and into `tests/CMakeLists.txt`:

```cmake
option(STELLAR_ENABLE_OPENGL_CONTEXT_TESTS
       "Build optional OpenGL context/readback smoke tests" OFF)
option(STELLAR_ENABLE_VULKAN_CONTEXT_TESTS
       "Build optional Vulkan context/init smoke tests" OFF)
option(STELLAR_ENABLE_DISPLAY_STARTUP_TESTS
       "Run SDL display startup smoke tests" OFF)
```

### 4.3 Remove test helper functions from root

Move these functions into `tests/CMakeLists.txt`:

```cmake
stellar_add_test_executable
stellar_add_world_test
stellar_add_server_test
stellar_add_physics_test
stellar_add_scripting_test
stellar_add_graphics_test
stellar_add_client_runtime_test
```

### 4.4 Remove all test target and CTest declarations from root

Move every test executable and `add_test(...)` registration into `tests/CMakeLists.txt`.

The moved block starts at the first test target declaration:

```cmake
stellar_add_world_test(stellar_runtime_world_test runtime_world
```

and continues through the final TrenchBroom source preflight test:

```cmake
set_tests_properties(trenchbroom_source_preflight_rejects_missing_wad_texture PROPERTIES WILL_FAIL TRUE)
```

Also move the display startup test block:

```cmake
if(STELLAR_ENABLE_DISPLAY_STARTUP_TESTS)
    add_test(NAME client_display_startup_smoke
    ...
endif()
```

### 4.5 Add `tests/` after all production targets exist

At the end of the root file, after `stellar-server` has been declared and linked, add:

```cmake
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

Placement matters. `tests/CMakeLists.txt` references these production targets:

- `stellar_world`
- `stellar_server_core`
- `stellar_physics`
- `stellar_scripting`
- `stellar_graphics`
- `stellar_client_runtime`
- `stellar_dedicated_server`
- `stellar_client_config`
- `stellar_import_bsp`
- `stellar_platform`
- `stellar_audio`
- `stellar_network`
- `stellar-client`
- `stellar-server`

Adding `tests/` after the client and server executable targets ensures all of those names exist before test registration.

---

## 5. New `tests/CMakeLists.txt`

Create `tests/CMakeLists.txt`.

Recommended header:

```cmake
# Test targets and CTest registrations for StellarEngine.
#
# Keep this file focused on tests so the repository root CMakeLists.txt can stay
# focused on production libraries and applications.

option(STELLAR_ENABLE_OPENGL_CONTEXT_TESTS
       "Build optional OpenGL context/readback smoke tests" OFF)
option(STELLAR_ENABLE_VULKAN_CONTEXT_TESTS
       "Build optional Vulkan context/init smoke tests" OFF)
option(STELLAR_ENABLE_DISPLAY_STARTUP_TESTS
       "Run SDL display startup smoke tests" OFF)

set(STELLAR_PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
set(STELLAR_TEST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(STELLAR_TEST_FIXTURE_DIR "${STELLAR_TEST_SOURCE_DIR}/fixtures")
```

Then paste the moved helper functions and test declarations below this header.

---

## 6. Required Path Adjustments After Moving to `tests/`

Because `tests/CMakeLists.txt` is processed from the `tests/` directory, every moved path should be normalized.

### 6.1 Test source files

Convert root-relative test source paths:

```cmake
tests/world/RuntimeWorld.cpp
```

to:

```cmake
${STELLAR_TEST_SOURCE_DIR}/world/RuntimeWorld.cpp
```

Apply this to all test sources under:

- `tests/audio`
- `tests/client`
- `tests/core`
- `tests/fixtures`
- `tests/graphics`
- `tests/import/bsp`
- `tests/integration`
- `tests/network`
- `tests/physics`
- `tests/platform`
- `tests/scripting`
- `tests/server`
- `tests/world`

### 6.2 Include directories

Convert:

```cmake
${CMAKE_SOURCE_DIR}/include
${CMAKE_SOURCE_DIR}/tests/fixtures
${CMAKE_SOURCE_DIR}/tests/graphics
${CMAKE_SOURCE_DIR}/tests/import/bsp
```

to:

```cmake
${STELLAR_PROJECT_SOURCE_DIR}/include
${STELLAR_TEST_FIXTURE_DIR}
${STELLAR_TEST_SOURCE_DIR}/graphics
${STELLAR_TEST_SOURCE_DIR}/import/bsp
```

### 6.3 Tool paths

Convert:

```cmake
${CMAKE_SOURCE_DIR}/tools/...
```

to:

```cmake
${STELLAR_PROJECT_SOURCE_DIR}/tools/...
```

### 6.4 TrenchBroom fixture source paths

Convert:

```cmake
${CMAKE_SOURCE_DIR}/tests/fixtures/trenchbroom/src/minimal_zup_room.map
```

to:

```cmake
${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/src/minimal_zup_room.map
```

### 6.5 Preserve binary output paths

Keep these paths rooted at `${CMAKE_BINARY_DIR}`:

```cmake
set(STELLAR_TRENCHBROOM_FIXTURE_DIR
    ${CMAKE_BINARY_DIR}/tests/fixtures/trenchbroom/compiled
)
```

Do not change this to `${CMAKE_CURRENT_BINARY_DIR}` unless the team explicitly wants fixture output to move. The objective is no behavior change.

---

## 7. Test Command Robustness Improvements

Where tests invoke built executables, prefer generator expressions over bare target names.

Recommended conversions:

```cmake
COMMAND stellar-client --validate-map ${STELLAR_CLI_BSP_FIXTURE}
```

to:

```cmake
COMMAND $<TARGET_FILE:stellar-client> --validate-map ${STELLAR_CLI_BSP_FIXTURE}
```

and:

```cmake
COMMAND stellar-server --validate-config --map ${STELLAR_CLI_BSP_FIXTURE}
```

to:

```cmake
COMMAND $<TARGET_FILE:stellar-server> --validate-config --map ${STELLAR_CLI_BSP_FIXTURE}
```

For the fixture writer:

```cmake
COMMAND stellar_bsp_fixture_writer ${STELLAR_CLI_BSP_FIXTURE} minimal_zup_room
```

can become:

```cmake
COMMAND $<TARGET_FILE:stellar_bsp_fixture_writer> ${STELLAR_CLI_BSP_FIXTURE} minimal_zup_room
```

This avoids ambiguity when `add_test(...)` is registered from the `tests/` subdirectory.

---

## 8. Suggested Implementation Order

### Step 1 — Create `tests/CMakeLists.txt`

1. Add the test options.
2. Add the helper variables:
   - `STELLAR_PROJECT_SOURCE_DIR`
   - `STELLAR_TEST_SOURCE_DIR`
   - `STELLAR_TEST_FIXTURE_DIR`
3. Move the `stellar_add_*_test` helper functions.
4. Move all test target declarations and `add_test(...)` registrations.
5. Update paths as described above.

### Step 2 — Trim root `CMakeLists.txt`

1. Replace `enable_testing()` with `include(CTest)`.
2. Remove the moved test options.
3. Remove the moved helper functions.
4. Remove the moved test declarations.
5. Add the gated test subdirectory after `stellar-server`:

```cmake
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

### Step 3 — Configure-only validation

Run:

```bash
cmake -S . -B build/cmake-split-check -DCMAKE_BUILD_TYPE=Debug
```

Expected result:

- configure succeeds,
- no “unknown target” errors,
- no missing source file errors,
- no duplicate test name errors.

### Step 4 — Test enumeration validation

Run:

```bash
ctest --test-dir build/cmake-split-check -N
```

Expected result:

- all previous test names are still listed,
- optional OpenGL/Vulkan/display tests remain absent unless their options are enabled,
- no duplicate tests.

### Step 5 — Build smoke validation

Run:

```bash
cmake --build build/cmake-split-check --target stellar-client stellar-server
```

Then build a representative set of test targets:

```bash
cmake --build build/cmake-split-check --target \
  stellar_runtime_world_test \
  stellar_import_bsp_test \
  stellar_bsp_fixture_writer \
  stellar_client_connect_test \
  stellar_lua_runtime_test
```

### Step 6 — CTest smoke validation

Run a focused smoke set first:

```bash
ctest --test-dir build/cmake-split-check --output-on-failure \
  -R "runtime_world|bsp_importer|lua_runtime|client_connect|trenchbroom_source_preflight"
```

If that passes, run the full test suite:

```bash
ctest --test-dir build/cmake-split-check --output-on-failure
```

Some BSP/TrenchBroom/VHLT-related tests may skip with return code `77` when external compilers or display servers are not available. That is acceptable if it matches prior behavior.

### Step 7 — Verify `BUILD_TESTING=OFF`

Run:

```bash
cmake -S . -B build/cmake-split-no-tests -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=OFF
cmake --build build/cmake-split-no-tests --target stellar-client stellar-server
ctest --test-dir build/cmake-split-no-tests -N
```

Expected result:

- production targets still build,
- no test targets are registered,
- CTest reports no tests or an empty test list.

---

## 9. Acceptance Criteria

The refactor is complete when all of the following are true:

1. Root `CMakeLists.txt` is focused on:
   - project setup,
   - dependencies,
   - third-party subdirectories,
   - production libraries,
   - production executables,
   - adding `tests/` when `BUILD_TESTING` is enabled.
2. `tests/CMakeLists.txt` owns all test-specific CMake definitions.
3. Existing test names are unchanged.
4. Existing target names are unchanged.
5. Existing fixture output paths are unchanged.
6. Default configure still includes tests.
7. `-DBUILD_TESTING=OFF` configures without test targets.
8. `ctest -N` succeeds and shows expected tests.
9. At least one representative test target from each major area configures/builds:
   - world,
   - server/client,
   - BSP/import,
   - scripting,
   - network,
   - graphics or platform,
   - TrenchBroom/BSP fixtures.
10. No production source files are changed.

---

## 10. Known Risks and Mitigations

### Risk: target lookup in `add_test(...)` changes from subdirectory scope

Mitigation: use `$<TARGET_FILE:target-name>` for test commands that invoke CMake targets.

### Risk: moved relative source paths break

Mitigation: use absolute paths through:

```cmake
${STELLAR_TEST_SOURCE_DIR}
${STELLAR_PROJECT_SOURCE_DIR}
```

### Risk: fixture output path accidentally changes

Mitigation: keep fixture output rooted at `${CMAKE_BINARY_DIR}`.

### Risk: optional test options are no longer visible in CMake GUI/cache

Mitigation: `option(...)` in `tests/CMakeLists.txt` still creates cache entries when `BUILD_TESTING=ON`, which is the default. If the project wants these options visible even when `BUILD_TESTING=OFF`, leave only those option declarations in the root file and move everything else.

### Risk: root file remains large after moving tests

Mitigation: defer production-target splitting until after this test split lands. A later refactor can move production target groups into `cmake/StellarTargets.cmake`, `src/*/CMakeLists.txt`, or domain-specific files.

---

## 11. Optional Follow-Up Refactor

After the minimal split is working, consider splitting `tests/CMakeLists.txt` by domain:

```cmake
include(cmake/TestHelpers.cmake)
include(cmake/WorldTests.cmake)
include(cmake/ServerTests.cmake)
include(cmake/ClientTests.cmake)
include(cmake/GraphicsTests.cmake)
include(cmake/BspTests.cmake)
include(cmake/TrenchBroomTests.cmake)
include(cmake/ScriptingTests.cmake)
include(cmake/NetworkTests.cmake)
```

Do this only after the first refactor is validated, because smaller steps make regressions easier to isolate.

---

## 12. Suggested Commit Message

Use this only if the user later asks for a commit:

```text
Split test CMake setup into tests directory

Move test helper functions, test executable declarations, and CTest
registrations out of the root CMakeLists.txt into tests/CMakeLists.txt.
Keep production target setup in the root and add tests through BUILD_TESTING.
```

---

## 13. Final AI Agent Checklist

Before handing back the result, report:

- files changed,
- root `CMakeLists.txt` approximate line reduction,
- configure command result,
- `ctest -N` result,
- focused CTest smoke result,
- any skipped tests and why,
- any tests not run because of missing local dependencies.
