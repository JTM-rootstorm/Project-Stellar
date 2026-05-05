# Phase DG-4 — Documentation Validation and Warning Policy

## Objective

Add a safe validation gate for docs generation without making day-to-day builds brittle.

## Recommended Files to Modify

- `tests/CMakeLists.txt` or a new `tests/cmake/DocsTests.cmake`
- Maybe root `CMakeLists.txt`
- Maybe `tools/docs/` if adding a small warning-budget checker

## Implementation Requirements

Add one or both of these checks:

### Check A — Existing Markdown Consistency Stays Active

Keep existing `docs_consistency` CTest intact. Do not replace it with Doxygen.

### Check B — Optional Doxygen Generation CTest

If Doxygen is found, register a CTest that builds/generates docs.

Suggested shape:

```cmake
if(STELLAR_BUILD_DOCS AND DOXYGEN_FOUND)
    add_test(NAME docs_doxygen_generate
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target stellar_docs_doxygen
    )
endif()
```

This should be optional and only registered when the target exists.

### Check C — Warning Budget, Later Only

Do not make Doxygen warnings fatal immediately unless DG-3 has cleaned the warning surface.

A later safe approach:

- Capture warnings to `build/docs/doxygen-warnings.log`.
- Add a script like `tools/docs/check_doxygen_warnings.py`.
- Allow known warning classes temporarily.
- Ratchet down count over time.

Do not enable `WARN_AS_ERROR = YES` in this phase unless the generated output is already clean and the team explicitly wants strict enforcement.

## Validation Commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build -R '^(docs_consistency|docs_doxygen_generate)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- Existing `docs_consistency` still runs.
- Doxygen docs generation can be validated through CTest when Doxygen exists.
- No failure occurs on machines that lack Doxygen unless docs were explicitly requested as a required target.
- Normal display-free CTest remains the default project gate.

## Parallelization

Do not parallelize this with DG-1 because both may edit docs/CMake integration.

---
