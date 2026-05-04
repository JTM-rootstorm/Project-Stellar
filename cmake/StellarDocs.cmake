option(STELLAR_BUILD_DOCS "Enable documentation generation targets" ON)

find_package(Doxygen QUIET)

if(STELLAR_BUILD_DOCS AND DOXYGEN_FOUND)
    set(STELLAR_DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs/doxygen")
    set(STELLAR_DOXYGEN_WARN_LOG "${CMAKE_BINARY_DIR}/docs/doxygen-warnings.log")
    set(STELLAR_DOXYGEN_MAINPAGE "${CMAKE_SOURCE_DIR}/README.md")

    set(STELLAR_DOXYGEN_INPUT_PATHS
        "${CMAKE_SOURCE_DIR}/include/stellar"
        "${CMAKE_SOURCE_DIR}/README.md"
        "${CMAKE_SOURCE_DIR}/docs/Design.md"
        "${CMAKE_SOURCE_DIR}/docs/ImplementationStatus.md"
        "${CMAKE_SOURCE_DIR}/docs/BspAuthoring.md"
        "${CMAKE_SOURCE_DIR}/docs/TrenchBroom.md"
    )

    set(STELLAR_DOXYGEN_EXCLUDE_PATHS
        "${CMAKE_SOURCE_DIR}/thirdparty"
        "${CMAKE_SOURCE_DIR}/Plans/Archived"
        "${CMAKE_SOURCE_DIR}/.kilo"
        "${CMAKE_SOURCE_DIR}/.codex"
        "${CMAKE_BINARY_DIR}"
    )

    set(STELLAR_DOXYGEN_INPUTS "")
    foreach(input_path IN LISTS STELLAR_DOXYGEN_INPUT_PATHS)
        string(APPEND STELLAR_DOXYGEN_INPUTS " \"${input_path}\"")
    endforeach()

    set(STELLAR_DOXYGEN_EXCLUDES "")
    foreach(exclude_path IN LISTS STELLAR_DOXYGEN_EXCLUDE_PATHS)
        string(APPEND STELLAR_DOXYGEN_EXCLUDES " \"${exclude_path}\"")
    endforeach()

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
