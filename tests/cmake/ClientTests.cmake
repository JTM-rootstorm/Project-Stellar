# Client runtime and CLI smoke test registrations.

add_executable(stellar_client_map_validation_smoke
    ${STELLAR_TEST_SOURCE_DIR}/client/AssetValidationSmoke.cpp
)

target_include_directories(stellar_client_map_validation_smoke PRIVATE
    ${STELLAR_PROJECT_SOURCE_DIR}/include
    ${STELLAR_TEST_FIXTURE_DIR}
)

target_link_libraries(stellar_client_map_validation_smoke PRIVATE
    stellar_client_config
)

add_test(NAME client_map_validation_smoke COMMAND $<TARGET_FILE:stellar_client_map_validation_smoke>)

stellar_add_client_runtime_test(stellar_client_movement_input_mapper_test client_movement_input_mapper
    ${STELLAR_TEST_SOURCE_DIR}/client/MovementInputMapper.cpp
)

stellar_add_client_runtime_test(stellar_player_presentation_test player_presentation
    ${STELLAR_TEST_SOURCE_DIR}/client/PlayerPresentation.cpp
)

stellar_add_client_runtime_test(stellar_gameplay_presentation_test gameplay_presentation
    ${STELLAR_TEST_SOURCE_DIR}/client/GameplayPresentation.cpp
)

stellar_add_client_runtime_test(stellar_client_world_receiver_test client_world_receiver
    ${STELLAR_TEST_SOURCE_DIR}/client/ClientWorldReceiver.cpp
)

stellar_add_client_runtime_test(stellar_client_single_player_runtime_test client_single_player_runtime
    ${STELLAR_TEST_SOURCE_DIR}/client/SinglePlayerRuntime.cpp
)

stellar_add_test_executable(stellar_listen_server_host_test listen_server_host
    ${STELLAR_TEST_SOURCE_DIR}/client/ListenHostRuntime.cpp
)
target_include_directories(stellar_listen_server_host_test PRIVATE
    ${STELLAR_TEST_FIXTURE_DIR}
)
target_link_libraries(stellar_listen_server_host_test PRIVATE
    stellar_client_config
)

stellar_add_test_executable(stellar_client_connect_test client_connect
    ${STELLAR_TEST_SOURCE_DIR}/client/ClientConnect.cpp
)
target_include_directories(stellar_client_connect_test PRIVATE
    ${STELLAR_TEST_FIXTURE_DIR}
)
target_link_libraries(stellar_client_connect_test PRIVATE
    stellar_client_config
    stellar_dedicated_server
)

stellar_add_client_runtime_test(stellar_hud_presentation_test hud_presentation
    ${STELLAR_TEST_SOURCE_DIR}/client/HudPresentation.cpp
)

if(STELLAR_ENABLE_DISPLAY_STARTUP_TESTS)
    add_test(NAME client_display_startup_smoke
        COMMAND bash -c [=[
if [[ -z "$DISPLAY" && -z "$WAYLAND_DISPLAY" ]]; then
    printf 'Skipping display startup smoke: DISPLAY and WAYLAND_DISPLAY are unset.\n'
    exit 77
fi
"$1" --validate-display
]=] bash "$<TARGET_FILE:stellar-client>"
    )
    set_tests_properties(client_display_startup_smoke PROPERTIES SKIP_RETURN_CODE 77)
endif()
