# Phase DG-1 — Add Doxygen Configuration and CMake Docs Target

## Objective

Add the minimum viable Doxygen generation path while keeping normal builds independent of Doxygen.

## Recommended Files to Add/Modify

Add:

- `docs/Doxyfile.in`

Modify:

- `CMakeLists.txt`

Optional if project prefers CMake modularity:

- `cmake/StellarDocs.cmake`

## Implementation Requirements

### Doxygen Configuration

Create `docs/Doxyfile.in` with settings that:

- Set `PROJECT_NAME = "Stellar Engine"`.
- Use CMake-substituted output directory in the build tree.
- Generate HTML.
- Disable LaTeX unless specifically needed.
- Treat `README.md` or a small docs landing page as the main page.
- Include public API headers from `include/stellar`.
- Include selected active Markdown pages as narrative pages.
- Exclude generated build outputs, archived plans, `.git`, `.kilo`, `.codex`, third-party code, and generated assets.
- Enable Markdown support.
- Extract public APIs.
- Avoid making undocumented symbols fatal at first.
- Emit warnings to a known build-tree log file.

Suggested starter values:

```ini
PROJECT_NAME           = "Stellar Engine"
PROJECT_NUMBER         = @PROJECT_VERSION@
OUTPUT_DIRECTORY       = @STELLAR_DOXYGEN_OUTPUT_DIR@
CREATE_SUBDIRS         = NO
GENERATE_HTML          = YES
GENERATE_LATEX         = NO
GENERATE_XML           = NO
MARKDOWN_SUPPORT       = YES
AUTOLINK_SUPPORT       = YES
USE_MDFILE_AS_MAINPAGE = @STELLAR_DOXYGEN_MAINPAGE@

INPUT                  = @STELLAR_DOXYGEN_INPUTS@
FILE_PATTERNS          = *.hpp *.h *.md
RECURSIVE              = YES

EXCLUDE                = @STELLAR_DOXYGEN_EXCLUDES@
EXCLUDE_PATTERNS       = */build/* */thirdparty/* */Plans/Archived/* */.kilo/* */.codex/*

EXTRACT_ALL            = NO
EXTRACT_PRIVATE        = NO
EXTRACT_STATIC         = NO
HIDE_UNDOC_MEMBERS     = NO
HIDE_UNDOC_CLASSES     = NO
WARNINGS               = YES
WARN_IF_UNDOCUMENTED   = YES
WARN_AS_ERROR          = NO
WARN_LOGFILE           = @STELLAR_DOXYGEN_WARN_LOG@
```

Codex should adjust syntax to valid Doxygen/CMake escaping.

### CMake Integration

In `CMakeLists.txt`, add a non-default docs target:

```cmake
option(STELLAR_BUILD_DOCS "Enable documentation generation targets" ON)

find_package(Doxygen QUIET)

if(STELLAR_BUILD_DOCS AND DOXYGEN_FOUND)
    set(STELLAR_DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs/doxygen")
    set(STELLAR_DOXYGEN_WARN_LOG "${CMAKE_BINARY_DIR}/docs/doxygen-warnings.log")
    set(STELLAR_DOXYGEN_MAINPAGE "${CMAKE_SOURCE_DIR}/README.md")

    set(STELLAR_DOXYGEN_INPUTS
        "${CMAKE_SOURCE_DIR}/include/stellar"
        "${CMAKE_SOURCE_DIR}/README.md"
        "${CMAKE_SOURCE_DIR}/docs/Design.md"
        "${CMAKE_SOURCE_DIR}/docs/ImplementationStatus.md"
        "${CMAKE_SOURCE_DIR}/docs/BspAuthoring.md"
        "${CMAKE_SOURCE_DIR}/docs/TrenchBroom.md"
    )

    set(STELLAR_DOXYGEN_EXCLUDES
        "${CMAKE_SOURCE_DIR}/thirdparty"
        "${CMAKE_SOURCE_DIR}/Plans/Archived"
        "${CMAKE_SOURCE_DIR}/.kilo"
        "${CMAKE_SOURCE_DIR}/.codex"
        "${CMAKE_BINARY_DIR}"
    )

    configure_file(
        "${CMAKE_SOURCE_DIR}/docs/Doxyfile.in"
        "${CMAKE_BINARY_DIR}/docs/Doxyfile"
        @ONLY
    )

    add_custom_target(stellar_docs_doxygen
        COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_BINARY_DIR}/docs/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Generating Stellar Doxygen API documentation"
        VERBATIM
    )
elseif(STELLAR_BUILD_DOCS)
    message(STATUS "Doxygen not found; stellar_docs_doxygen target will not be available")
endif()
```

Prefer a dedicated `cmake/StellarDocs.cmake` if root `CMakeLists.txt` becomes too noisy.

## Validation Commands

With Doxygen installed:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_docs_doxygen
test -f build/docs/doxygen/html/index.html
```

Without Doxygen installed:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- Configure succeeds whether Doxygen is installed or not.
- Normal build/test remains independent of Doxygen.
- `stellar_docs_doxygen` exists only when Doxygen is available.
- Generated HTML goes under `build/docs/doxygen/html`.
- No generated HTML or warning logs are written to source-controlled docs directories.
- Active Markdown docs still pass existing `docs_consistency`.

## Parallelization

This phase should be done by one agent because CMake and Doxyfile substitutions are tightly coupled.

---
