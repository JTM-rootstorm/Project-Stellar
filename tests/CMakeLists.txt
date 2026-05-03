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

stellar_add_world_test(stellar_runtime_world_test runtime_world
    ${STELLAR_TEST_SOURCE_DIR}/world/RuntimeWorld.cpp
)

stellar_add_world_test(stellar_runtime_collision_state_test runtime_collision_state
    ${STELLAR_TEST_SOURCE_DIR}/world/RuntimeCollisionState.cpp
)

stellar_add_world_test(stellar_trigger_system_test trigger_system
    ${STELLAR_TEST_SOURCE_DIR}/world/TriggerSystem.cpp
)

stellar_add_world_test(stellar_object_collider_test object_collider
    ${STELLAR_TEST_SOURCE_DIR}/world/ObjectCollider.cpp
)

stellar_add_world_test(stellar_collision_validation_test collision_validation
    ${STELLAR_TEST_SOURCE_DIR}/world/CollisionValidation.cpp
)

stellar_add_world_test(stellar_world_metadata_validation_test world_metadata_validation
    ${STELLAR_TEST_SOURCE_DIR}/world/WorldMetadataValidation.cpp
)

stellar_add_test_executable(stellar_core_world_axes_test world_axes
    ${STELLAR_TEST_SOURCE_DIR}/core/WorldAxes.cpp
)

stellar_add_server_test(stellar_server_movement_simulation_test server_movement_simulation
    ${STELLAR_TEST_SOURCE_DIR}/server/MovementSimulation.cpp
)

stellar_add_server_test(stellar_movement_trigger_integration_test movement_trigger_integration
    ${STELLAR_TEST_SOURCE_DIR}/server/MovementTriggerIntegration.cpp
)

stellar_add_server_test(stellar_server_world_session_test server_world_session
    ${STELLAR_TEST_SOURCE_DIR}/server/WorldSession.cpp
)

stellar_add_server_test(stellar_server_gameplay_world_test server_gameplay_world
    ${STELLAR_TEST_SOURCE_DIR}/server/GameplayWorld.cpp
)

stellar_add_test_executable(stellar_dedicated_server_test dedicated_server
    ${STELLAR_TEST_SOURCE_DIR}/server/DedicatedServer.cpp
)
target_include_directories(stellar_dedicated_server_test PRIVATE
    ${STELLAR_TEST_FIXTURE_DIR}
)
target_link_libraries(stellar_dedicated_server_test PRIVATE stellar_dedicated_server)

stellar_add_graphics_test(stellar_render_level_upload_test render_level_upload
    ${STELLAR_TEST_SOURCE_DIR}/graphics/RenderSceneUpload.cpp
)

add_executable(stellar_render_level_inspection_test
    ${STELLAR_TEST_SOURCE_DIR}/graphics/RenderSceneInspection.cpp
)

target_include_directories(stellar_render_level_inspection_test PRIVATE
    ${STELLAR_PROJECT_SOURCE_DIR}/include
    ${STELLAR_TEST_SOURCE_DIR}/graphics
)

target_link_libraries(stellar_render_level_inspection_test PRIVATE
    stellar_graphics
)

add_test(NAME render_level_inspection COMMAND $<TARGET_FILE:stellar_render_level_inspection_test>)

stellar_add_graphics_test(stellar_graphics_backend_selection_test graphics_backend_selection
    ${STELLAR_TEST_SOURCE_DIR}/graphics/BackendSelection.cpp
)

if(STELLAR_ENABLE_OPENGL_CONTEXT_TESTS)
    add_executable(stellar_opengl_context_smoke_test
        ${STELLAR_TEST_SOURCE_DIR}/graphics/OpenGLContextSmoke.cpp
    )

    target_include_directories(stellar_opengl_context_smoke_test PRIVATE
        ${STELLAR_PROJECT_SOURCE_DIR}/include
        ${OPENGL_INCLUDE_DIRS}
        ${GLEW_INCLUDE_DIRS}
    )

    target_link_libraries(stellar_opengl_context_smoke_test PRIVATE
        stellar_graphics
    )

    add_test(NAME opengl_context_smoke COMMAND $<TARGET_FILE:stellar_opengl_context_smoke_test>)
    set_tests_properties(opengl_context_smoke PROPERTIES SKIP_RETURN_CODE 77)
endif()

if(STELLAR_ENABLE_VULKAN_CONTEXT_TESTS)
    add_executable(stellar_vulkan_context_smoke_test
        ${STELLAR_TEST_SOURCE_DIR}/graphics/VulkanContextSmoke.cpp
    )

    target_include_directories(stellar_vulkan_context_smoke_test PRIVATE
        ${STELLAR_PROJECT_SOURCE_DIR}/include
        ${Vulkan_INCLUDE_DIRS}
    )

    target_link_libraries(stellar_vulkan_context_smoke_test PRIVATE
        stellar_graphics
    )

    add_test(NAME vulkan_context_smoke COMMAND $<TARGET_FILE:stellar_vulkan_context_smoke_test>)
endif()

stellar_add_test_executable(stellar_import_bsp_test bsp_importer
    ${STELLAR_TEST_SOURCE_DIR}/import/bsp/BspImporter.cpp
)
target_link_libraries(stellar_import_bsp_test PRIVATE stellar_import_bsp)

stellar_add_test_executable(stellar_bsp_entity_authoring_test bsp_entity_authoring
    ${STELLAR_TEST_SOURCE_DIR}/import/bsp/BspEntityAuthoring.cpp
)
target_link_libraries(stellar_bsp_entity_authoring_test PRIVATE stellar_import_bsp)

stellar_add_test_executable(stellar_fgd_contract_test fgd_contract
    ${STELLAR_TEST_SOURCE_DIR}/import/bsp/FgdContract.cpp
)
target_compile_definitions(stellar_fgd_contract_test PRIVATE
    STELLAR_SOURCE_DIR="${STELLAR_PROJECT_SOURCE_DIR}"
)
add_test(NAME trenchbroom_fgd_contract COMMAND $<TARGET_FILE:stellar_fgd_contract_test>)

stellar_add_test_executable(stellar_bsp_visibility_test bsp_visibility
    ${STELLAR_TEST_SOURCE_DIR}/import/bsp/BspVisibility.cpp
)
target_link_libraries(stellar_bsp_visibility_test PRIVATE stellar_import_bsp)

stellar_add_test_executable(stellar_bsp_materials_test bsp_materials
    ${STELLAR_TEST_SOURCE_DIR}/import/bsp/BspMaterials.cpp
)
target_link_libraries(stellar_bsp_materials_test PRIVATE stellar_import_bsp)

stellar_add_test_executable(stellar_bsp_lightmaps_test bsp_lightmaps
    ${STELLAR_TEST_SOURCE_DIR}/import/bsp/BspLightmaps.cpp
)
target_link_libraries(stellar_bsp_lightmaps_test PRIVATE stellar_import_bsp)

stellar_add_test_executable(stellar_bsp_validation_test bsp_validation
    ${STELLAR_TEST_SOURCE_DIR}/import/bsp/BspValidation.cpp
)
target_include_directories(stellar_bsp_validation_test PRIVATE
    ${STELLAR_TEST_FIXTURE_DIR}
    ${STELLAR_TEST_SOURCE_DIR}/import/bsp
)
target_link_libraries(stellar_bsp_validation_test PRIVATE stellar_import_bsp)

stellar_add_physics_test(stellar_collision_world_test collision_world
    ${STELLAR_TEST_SOURCE_DIR}/physics/CollisionWorld.cpp
)

stellar_add_physics_test(stellar_character_controller_test character_controller
    ${STELLAR_TEST_SOURCE_DIR}/physics/CharacterController.cpp
)

stellar_add_physics_test(stellar_collision_performance_regression_test collision_performance_regression
    ${STELLAR_TEST_SOURCE_DIR}/physics/CollisionPerformanceRegression.cpp
)

stellar_add_test_executable(stellar_platform_input_test input
    ${STELLAR_TEST_SOURCE_DIR}/platform/Input.cpp
)
target_link_libraries(stellar_platform_input_test PRIVATE stellar_platform)

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

stellar_add_client_runtime_test(stellar_client_local_loopback_runtime_test client_local_loopback_runtime
    ${STELLAR_TEST_SOURCE_DIR}/client/LocalLoopbackRuntime.cpp
)

stellar_add_client_runtime_test(stellar_client_world_receiver_test client_world_receiver
    ${STELLAR_TEST_SOURCE_DIR}/client/ClientWorldReceiver.cpp
)

stellar_add_client_runtime_test(stellar_networked_client_runtime_test networked_client_runtime
    ${STELLAR_TEST_SOURCE_DIR}/client/NetworkedClientRuntime.cpp
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

stellar_add_test_executable(stellar_audio_event_router_test audio_event_router
    ${STELLAR_TEST_SOURCE_DIR}/audio/AudioEventRouter.cpp
)
target_link_libraries(stellar_audio_event_router_test PRIVATE stellar_audio)

stellar_add_scripting_test(stellar_lua_runtime_test lua_runtime
    ${STELLAR_TEST_SOURCE_DIR}/scripting/LuaRuntime.cpp
)

stellar_add_scripting_test(stellar_trigger_script_system_test trigger_script
    ${STELLAR_TEST_SOURCE_DIR}/scripting/TriggerScriptSystem.cpp
)

stellar_add_scripting_test(stellar_object_collider_script_system_test object_collider_script
    ${STELLAR_TEST_SOURCE_DIR}/scripting/ObjectColliderScriptSystem.cpp
)

stellar_add_scripting_test(stellar_script_command_processor_test script_command_processor
    ${STELLAR_TEST_SOURCE_DIR}/scripting/ScriptCommandProcessor.cpp
)

stellar_add_scripting_test(stellar_scripted_world_session_test scripted_world_session
    ${STELLAR_TEST_SOURCE_DIR}/scripting/ScriptedWorldSession.cpp
)
target_link_libraries(stellar_scripted_world_session_test PRIVATE stellar_import_bsp)

stellar_add_test_executable(stellar_snapshot_codec_test snapshot_codec
    ${STELLAR_TEST_SOURCE_DIR}/network/SnapshotCodec.cpp
)
target_link_libraries(stellar_snapshot_codec_test PRIVATE stellar_network)

stellar_add_test_executable(stellar_network_session_test network_session
    ${STELLAR_TEST_SOURCE_DIR}/network/Session.cpp
)
target_link_libraries(stellar_network_session_test PRIVATE stellar_network stellar_client_runtime)

stellar_add_test_executable(stellar_loopback_transport_test loopback_transport
    ${STELLAR_TEST_SOURCE_DIR}/network/LoopbackTransport.cpp
)
target_link_libraries(stellar_loopback_transport_test PRIVATE stellar_network)

stellar_add_test_executable(stellar_socket_transport_test socket_transport
    ${STELLAR_TEST_SOURCE_DIR}/network/SocketTransport.cpp
)
target_link_libraries(stellar_socket_transport_test PRIVATE stellar_network stellar_client_runtime)

stellar_add_test_executable(stellar_snapshot_delta_test snapshot_delta
    ${STELLAR_TEST_SOURCE_DIR}/network/SnapshotDelta.cpp
)
target_link_libraries(stellar_snapshot_delta_test PRIVATE stellar_network)

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

add_test(NAME trenchbroom_vhlt_fixture_matrix
    COMMAND bash "${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/run_vhlt_fixture_matrix.sh"
        --source-root "${STELLAR_PROJECT_SOURCE_DIR}"
        --build-root "${CMAKE_BINARY_DIR}"
        --profile full
        --stellar-client "$<TARGET_FILE:stellar-client>"
        --stellar-server "$<TARGET_FILE:stellar-server>"
)
set_tests_properties(trenchbroom_vhlt_fixture_matrix PROPERTIES
    SKIP_RETURN_CODE 77
)

add_executable(stellar_bsp_fixture_writer
    ${STELLAR_TEST_SOURCE_DIR}/fixtures/BspFixtureWriter.cpp
)

target_include_directories(stellar_bsp_fixture_writer PRIVATE
    ${STELLAR_PROJECT_SOURCE_DIR}/include
    ${STELLAR_TEST_FIXTURE_DIR}
)

set(STELLAR_TRENCHBROOM_FIXTURE_DIR
    ${CMAKE_BINARY_DIR}/tests/fixtures/trenchbroom/compiled
)
set(STELLAR_CLI_BSP_FIXTURE
    ${STELLAR_TRENCHBROOM_FIXTURE_DIR}/minimal_zup_room.bsp
)
set(STELLAR_CLI_BAD_BSP_FIXTURE
    ${STELLAR_TRENCHBROOM_FIXTURE_DIR}/invalid_script_escape_zup.bsp
)
set(STELLAR_TB_ENTITY_MATRIX_BSP_FIXTURE
    ${STELLAR_TRENCHBROOM_FIXTURE_DIR}/entity_matrix_zup.bsp
)
set(STELLAR_TB_SCRIPTED_BSP_FIXTURE
    ${STELLAR_TRENCHBROOM_FIXTURE_DIR}/scripted_interaction_zup.bsp
)
set(STELLAR_TB_LIT_BSP_FIXTURE
    ${STELLAR_TRENCHBROOM_FIXTURE_DIR}/lit_zup_room.bsp
)
set(STELLAR_TB_MATERIAL_WAD_BSP_FIXTURE
    ${STELLAR_TRENCHBROOM_FIXTURE_DIR}/material_wad_zup.bsp
)
set(STELLAR_TB_MOVING_DOOR_BSP_FIXTURE
    ${STELLAR_TRENCHBROOM_FIXTURE_DIR}/moving_door_button_zup.bsp
)
set(STELLAR_TB_POINT_VOLUME_BSP_FIXTURE
    ${STELLAR_TRENCHBROOM_FIXTURE_DIR}/point_volume_zup.bsp
)
set(STELLAR_TB_ILLUSIONARY_STATIC_BSP_FIXTURE
    ${STELLAR_TRENCHBROOM_FIXTURE_DIR}/illusionary_static_zup.bsp
)

add_test(NAME bsp_fixture_writer
    COMMAND $<TARGET_FILE:stellar_bsp_fixture_writer> ${STELLAR_CLI_BSP_FIXTURE} minimal_zup_room
)
set_tests_properties(bsp_fixture_writer PROPERTIES FIXTURES_SETUP bsp_cli_fixture)

add_test(NAME bsp_bad_fixture_writer
    COMMAND $<TARGET_FILE:stellar_bsp_fixture_writer> ${STELLAR_CLI_BAD_BSP_FIXTURE} invalid_script_escape_zup
)
set_tests_properties(bsp_bad_fixture_writer PROPERTIES FIXTURES_SETUP bsp_bad_cli_fixture)

add_test(NAME bsp_authoring_smoke_entity_matrix_fixture_writer
    COMMAND $<TARGET_FILE:stellar_bsp_fixture_writer> ${STELLAR_TB_ENTITY_MATRIX_BSP_FIXTURE} entity_matrix_zup
)

add_test(NAME bsp_authoring_smoke_scripted_fixture_writer
    COMMAND $<TARGET_FILE:stellar_bsp_fixture_writer> ${STELLAR_TB_SCRIPTED_BSP_FIXTURE} scripted_interaction_zup
)

add_test(NAME bsp_lightmaps_lit_fixture_writer
    COMMAND $<TARGET_FILE:stellar_bsp_fixture_writer> ${STELLAR_TB_LIT_BSP_FIXTURE} lit_zup_room
)
set_tests_properties(bsp_lightmaps_lit_fixture_writer PROPERTIES
    FIXTURES_SETUP bsp_lightmaps_lit_fixture_writer
)

add_test(NAME trenchbroom_package_material_wad_fixture_writer
    COMMAND $<TARGET_FILE:stellar_bsp_fixture_writer> ${STELLAR_TB_MATERIAL_WAD_BSP_FIXTURE} material_wad_zup
)
set_tests_properties(trenchbroom_package_material_wad_fixture_writer PROPERTIES
    FIXTURES_SETUP trenchbroom_package_material_wad_fixture_writer
)

add_test(NAME brush_mover_door_button_fixture_writer
    COMMAND $<TARGET_FILE:stellar_bsp_fixture_writer> ${STELLAR_TB_MOVING_DOOR_BSP_FIXTURE} moving_door_button_zup
)
set_tests_properties(brush_mover_door_button_fixture_writer PROPERTIES
    FIXTURES_SETUP brush_mover_door_button_fixture_writer
)

add_test(NAME bsp_authoring_smoke_point_volume_fixture_writer
    COMMAND $<TARGET_FILE:stellar_bsp_fixture_writer> ${STELLAR_TB_POINT_VOLUME_BSP_FIXTURE} point_volume_zup
)
set_tests_properties(bsp_authoring_smoke_point_volume_fixture_writer PROPERTIES
    FIXTURES_SETUP bsp_authoring_smoke_point_volume_fixture_writer
)

add_test(NAME bsp_authoring_smoke_illusionary_static_fixture_writer
    COMMAND $<TARGET_FILE:stellar_bsp_fixture_writer> ${STELLAR_TB_ILLUSIONARY_STATIC_BSP_FIXTURE} illusionary_static_zup
)
set_tests_properties(bsp_authoring_smoke_illusionary_static_fixture_writer PROPERTIES
    FIXTURES_SETUP bsp_authoring_smoke_illusionary_static_fixture_writer
)

add_test(NAME client_cli_map_validation
    COMMAND $<TARGET_FILE:stellar-client> --validate-config --map ${STELLAR_CLI_BSP_FIXTURE}
)
set_tests_properties(client_cli_map_validation PROPERTIES FIXTURES_REQUIRED bsp_cli_fixture)

add_test(NAME client_cli_validate_map
    COMMAND $<TARGET_FILE:stellar-client> --validate-map ${STELLAR_CLI_BSP_FIXTURE}
)
set_tests_properties(client_cli_validate_map PROPERTIES FIXTURES_REQUIRED bsp_cli_fixture)

add_test(NAME bsp_lightmaps_lit_fixture_validate
    COMMAND $<TARGET_FILE:stellar-client> --validate-map ${STELLAR_TB_LIT_BSP_FIXTURE}
)
set_tests_properties(bsp_lightmaps_lit_fixture_validate PROPERTIES
    FIXTURES_REQUIRED bsp_lightmaps_lit_fixture_writer
)

add_test(NAME trenchbroom_package_material_wad_fixture_validate
    COMMAND $<TARGET_FILE:stellar-client> --validate-map ${STELLAR_TB_MATERIAL_WAD_BSP_FIXTURE}
)
set_tests_properties(trenchbroom_package_material_wad_fixture_validate PROPERTIES
    FIXTURES_REQUIRED trenchbroom_package_material_wad_fixture_writer
)

add_test(NAME brush_mover_door_button_fixture_validate
    COMMAND $<TARGET_FILE:stellar-client> --validate-map ${STELLAR_TB_MOVING_DOOR_BSP_FIXTURE}
)
set_tests_properties(brush_mover_door_button_fixture_validate PROPERTIES
    FIXTURES_REQUIRED brush_mover_door_button_fixture_writer
)

add_test(NAME bsp_authoring_smoke_point_volume_fixture_validate
    COMMAND $<TARGET_FILE:stellar-client> --validate-map ${STELLAR_TB_POINT_VOLUME_BSP_FIXTURE}
)
set_tests_properties(bsp_authoring_smoke_point_volume_fixture_validate PROPERTIES
    FIXTURES_REQUIRED bsp_authoring_smoke_point_volume_fixture_writer
)

add_test(NAME bsp_authoring_smoke_illusionary_static_fixture_validate
    COMMAND $<TARGET_FILE:stellar-client> --validate-map ${STELLAR_TB_ILLUSIONARY_STATIC_BSP_FIXTURE}
)
set_tests_properties(bsp_authoring_smoke_illusionary_static_fixture_validate PROPERTIES
    FIXTURES_REQUIRED bsp_authoring_smoke_illusionary_static_fixture_writer
)

add_test(NAME client_cli_map_validation_rejects_bad_map
    COMMAND $<TARGET_FILE:stellar-client> --validate-map ${STELLAR_CLI_BAD_BSP_FIXTURE}
)
set_tests_properties(client_cli_map_validation_rejects_bad_map PROPERTIES
    FIXTURES_REQUIRED bsp_bad_cli_fixture
    WILL_FAIL TRUE
)

add_test(NAME server_cli_validate_config
    COMMAND $<TARGET_FILE:stellar-server> --validate-config --map ${STELLAR_CLI_BSP_FIXTURE}
)
set_tests_properties(server_cli_validate_config PROPERTIES FIXTURES_REQUIRED bsp_cli_fixture)

add_test(NAME server_cli_rejects_bad_map
    COMMAND $<TARGET_FILE:stellar-server> --validate-config --map ${STELLAR_CLI_BAD_BSP_FIXTURE}
)
set_tests_properties(server_cli_rejects_bad_map PROPERTIES
    FIXTURES_REQUIRED bsp_bad_cli_fixture
    WILL_FAIL TRUE
)

add_executable(stellar_bsp_playable_world_smoke_test
    ${STELLAR_TEST_SOURCE_DIR}/integration/BspPlayableWorldSmoke.cpp
)

target_include_directories(stellar_bsp_playable_world_smoke_test PRIVATE
    ${STELLAR_PROJECT_SOURCE_DIR}/include
    ${STELLAR_TEST_FIXTURE_DIR}
)

target_link_libraries(stellar_bsp_playable_world_smoke_test PRIVATE
    stellar_import_bsp
    stellar_client_runtime
    stellar_scripting
)

add_test(NAME bsp_playable_world_smoke COMMAND $<TARGET_FILE:stellar_bsp_playable_world_smoke_test>)
add_test(NAME bsp_scripted_playable_world_smoke COMMAND $<TARGET_FILE:stellar_bsp_playable_world_smoke_test>)
add_test(NAME bsp_scripted_collision_smoke COMMAND $<TARGET_FILE:stellar_bsp_playable_world_smoke_test>)
add_test(NAME bsp_scripted_object_collider_smoke COMMAND $<TARGET_FILE:stellar_bsp_playable_world_smoke_test>)

add_executable(stellar_bsp_authoring_smoke_test
    ${STELLAR_TEST_SOURCE_DIR}/integration/BspAuthoringSmoke.cpp
)

target_include_directories(stellar_bsp_authoring_smoke_test PRIVATE
    ${STELLAR_PROJECT_SOURCE_DIR}/include
    ${STELLAR_TEST_FIXTURE_DIR}
)

target_link_libraries(stellar_bsp_authoring_smoke_test PRIVATE
    stellar_client_config
)

add_test(NAME bsp_authoring_smoke COMMAND $<TARGET_FILE:stellar_bsp_authoring_smoke_test>)

add_test(NAME bsp_authoring_smoke_compile_wrapper
    COMMAND bash -c "if command -v ericw-qbsp >/dev/null 2>&1 || command -v qbsp >/dev/null 2>&1 || command -v hqbsp >/dev/null 2>&1 || command -v tyr-qbsp >/dev/null 2>&1 || [[ -n \"$STELLAR_BSP30_COMPILER\" || -n \"$QBSP\" ]]; then STELLAR_CLIENT='$<TARGET_FILE:stellar-client>' STELLAR_SERVER='$<TARGET_FILE:stellar-server>' '${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/compile_trenchbroom_bsp30.sh' --map '${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/src/minimal_zup_room.map' --out '${STELLAR_TRENCHBROOM_FIXTURE_DIR}/compile_wrapper_smoke.bsp' --profile fast; else printf 'Skipping BSP30 compile wrapper smoke: no compiler found.\\n'; fi"
)

add_test(NAME trenchbroom_package_path_smoke
    COMMAND ${STELLAR_PROJECT_SOURCE_DIR}/tools/trenchbroom/test_stellar_package_paths.sh ${STELLAR_PROJECT_SOURCE_DIR}
)
set_tests_properties(trenchbroom_package_path_smoke PROPERTIES
    ENVIRONMENT "STELLAR_TEST_BUILD_ROOT=${CMAKE_BINARY_DIR};STELLAR_CLIENT=$<TARGET_FILE:stellar-client>;STELLAR_SERVER=$<TARGET_FILE:stellar-server>"
)

add_test(NAME trenchbroom_wad_tool_verify
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/create_stellar_dev_wad.py
        --verify ${STELLAR_PROJECT_SOURCE_DIR}/tools/trenchbroom/Stellar/materials/stellar_dev.wad
)

add_test(NAME trenchbroom_wad_tool_list
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/create_stellar_dev_wad.py --list
)

add_test(NAME trenchbroom_dev_png_tool_verify
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/create_stellar_dev_wad.py
        --verify-png ${STELLAR_PROJECT_SOURCE_DIR}/tools/trenchbroom/Stellar/materials
)

add_test(NAME trenchbroom_vhlt_preserves_valve220_axes
    COMMAND bash ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/test_compile_vhlt_texture_axes.sh
        ${STELLAR_PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)

add_test(NAME trenchbroom_vhlt_light_angle_normalizer
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/test_normalize_vhlt_light_angles.py
)

add_test(NAME trenchbroom_source_preflight_fixtures
    COMMAND bash -c "for f in '${STELLAR_TEST_FIXTURE_DIR}'/trenchbroom/src/*.map; do case \"$f\" in *invalid_*.map) continue ;; esac; python3 '${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/validate_trenchbroom_map_source.py' \"$f\" || exit 1; done"
)

add_test(NAME trenchbroom_source_preflight_rejects_malformed
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/validate_trenchbroom_map_source.py
        ${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/src/invalid_malformed_brush.map
)
set_tests_properties(trenchbroom_source_preflight_rejects_malformed PROPERTIES WILL_FAIL TRUE)

add_test(NAME trenchbroom_source_preflight_rejects_incomplete_brush
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/validate_trenchbroom_map_source.py
        ${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/src/invalid_incomplete_brush.map
)
set_tests_properties(trenchbroom_source_preflight_rejects_incomplete_brush PROPERTIES WILL_FAIL TRUE)

add_test(NAME trenchbroom_source_preflight_rejects_script_escape
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/validate_trenchbroom_map_source.py
        ${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/src/invalid_script_escape_zup.map
)
set_tests_properties(trenchbroom_source_preflight_rejects_script_escape PROPERTIES WILL_FAIL TRUE)

add_test(NAME trenchbroom_source_preflight_rejects_missing_target
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/validate_trenchbroom_map_source.py
        ${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/src/invalid_missing_target.map
)
set_tests_properties(trenchbroom_source_preflight_rejects_missing_target PROPERTIES WILL_FAIL TRUE)

add_test(NAME trenchbroom_source_preflight_rejects_missing_wad_texture
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/validate_trenchbroom_map_source.py
        --strict-textures
        --wad-root ${STELLAR_PROJECT_SOURCE_DIR}/tools/trenchbroom/Stellar/materials
        ${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/src/invalid_missing_wad_texture.map
)
set_tests_properties(trenchbroom_source_preflight_rejects_missing_wad_texture PROPERTIES WILL_FAIL TRUE)
