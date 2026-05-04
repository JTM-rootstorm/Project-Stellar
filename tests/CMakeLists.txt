# Test targets and CTest registrations for StellarEngine.
#
# Keep this file focused on test-wide settings and domain registration includes so
# the repository root CMakeLists.txt can stay focused on production libraries and
# applications.

option(STELLAR_ENABLE_OPENGL_CONTEXT_TESTS
       "Build optional OpenGL context/readback smoke tests" OFF)
option(STELLAR_ENABLE_DISPLAY_STARTUP_TESTS
       "Run SDL display startup smoke tests" OFF)

set(STELLAR_PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
set(STELLAR_TEST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(STELLAR_TEST_FIXTURE_DIR "${STELLAR_TEST_SOURCE_DIR}/fixtures")
set(STELLAR_TEST_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(${STELLAR_TEST_CMAKE_DIR}/TestHelpers.cmake)
include(${STELLAR_TEST_CMAKE_DIR}/WorldTests.cmake)
include(${STELLAR_TEST_CMAKE_DIR}/ServerTests.cmake)
include(${STELLAR_TEST_CMAKE_DIR}/GraphicsTests.cmake)
include(${STELLAR_TEST_CMAKE_DIR}/BspTests.cmake)
include(${STELLAR_TEST_CMAKE_DIR}/ClientTests.cmake)
include(${STELLAR_TEST_CMAKE_DIR}/ScriptingTests.cmake)
include(${STELLAR_TEST_CMAKE_DIR}/NetworkTests.cmake)
include(${STELLAR_TEST_CMAKE_DIR}/AudioPlatformTests.cmake)
include(${STELLAR_TEST_CMAKE_DIR}/TrenchBroomTests.cmake)

if(STELLAR_BUILD_DOCS AND TARGET stellar_docs_doxygen)
    add_test(NAME docs_doxygen_generate
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target stellar_docs_doxygen
    )
endif()
