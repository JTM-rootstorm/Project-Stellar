# Shared helpers for Stellar Engine test registration.

function(stellar_add_test_executable target_name test_name)
    add_executable(${target_name} ${ARGN})
    target_include_directories(${target_name} PRIVATE
        ${STELLAR_PROJECT_SOURCE_DIR}/include
    )
    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${target_name}>)
endfunction()

function(stellar_add_world_test target_name test_name)
    stellar_add_test_executable(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE stellar_world)
endfunction()

function(stellar_add_server_test target_name test_name)
    stellar_add_test_executable(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE stellar_server_core)
endfunction()

function(stellar_add_physics_test target_name test_name)
    stellar_add_test_executable(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE stellar_physics)
endfunction()

function(stellar_add_scripting_test target_name test_name)
    stellar_add_test_executable(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE stellar_scripting)
endfunction()

function(stellar_add_graphics_test target_name test_name)
    stellar_add_test_executable(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE stellar_graphics)
endfunction()

function(stellar_add_client_runtime_test target_name test_name)
    stellar_add_test_executable(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE stellar_client_runtime)
endfunction()

function(stellar_add_fixture_writer_test)
    cmake_parse_arguments(ARG "" "TEST_NAME;OUTPUT;FIXTURE_ID;FIXTURES_SETUP" "" ${ARGN})
    add_test(NAME ${ARG_TEST_NAME}
        COMMAND $<TARGET_FILE:stellar_bsp_fixture_writer> ${ARG_OUTPUT} ${ARG_FIXTURE_ID}
    )
    if(ARG_FIXTURES_SETUP)
        set_tests_properties(${ARG_TEST_NAME} PROPERTIES FIXTURES_SETUP ${ARG_FIXTURES_SETUP})
    endif()
endfunction()

function(stellar_add_validate_map_test)
    cmake_parse_arguments(ARG "" "TEST_NAME;MAP;FIXTURES_REQUIRED;COMMAND_TARGET" "ARGS" ${ARGN})
    add_test(NAME ${ARG_TEST_NAME}
        COMMAND $<TARGET_FILE:${ARG_COMMAND_TARGET}> ${ARG_ARGS} ${ARG_MAP}
    )
    if(ARG_FIXTURES_REQUIRED)
        set_tests_properties(${ARG_TEST_NAME} PROPERTIES
            FIXTURES_REQUIRED ${ARG_FIXTURES_REQUIRED}
        )
    endif()
endfunction()

function(stellar_add_expected_failure_test)
    cmake_parse_arguments(ARG "" "TEST_NAME;FIXTURES_REQUIRED" "COMMAND" ${ARGN})
    add_test(NAME ${ARG_TEST_NAME} COMMAND ${ARG_COMMAND})
    if(ARG_FIXTURES_REQUIRED)
        set_tests_properties(${ARG_TEST_NAME} PROPERTIES
            FIXTURES_REQUIRED ${ARG_FIXTURES_REQUIRED}
            WILL_FAIL TRUE
        )
    else()
        set_tests_properties(${ARG_TEST_NAME} PROPERTIES WILL_FAIL TRUE)
    endif()
endfunction()
