# TrenchBroom package, BSP fixture, and tool contract tests.

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

stellar_add_fixture_writer_test(
    TEST_NAME bsp_fixture_writer
    OUTPUT ${STELLAR_CLI_BSP_FIXTURE}
    FIXTURE_ID minimal_zup_room
    FIXTURES_SETUP bsp_cli_fixture
)

stellar_add_fixture_writer_test(
    TEST_NAME bsp_bad_fixture_writer
    OUTPUT ${STELLAR_CLI_BAD_BSP_FIXTURE}
    FIXTURE_ID invalid_script_escape_zup
    FIXTURES_SETUP bsp_bad_cli_fixture
)

stellar_add_fixture_writer_test(
    TEST_NAME bsp_authoring_smoke_entity_matrix_fixture_writer
    OUTPUT ${STELLAR_TB_ENTITY_MATRIX_BSP_FIXTURE}
    FIXTURE_ID entity_matrix_zup
)

stellar_add_fixture_writer_test(
    TEST_NAME bsp_authoring_smoke_scripted_fixture_writer
    OUTPUT ${STELLAR_TB_SCRIPTED_BSP_FIXTURE}
    FIXTURE_ID scripted_interaction_zup
)

stellar_add_fixture_writer_test(
    TEST_NAME bsp_lightmaps_lit_fixture_writer
    OUTPUT ${STELLAR_TB_LIT_BSP_FIXTURE}
    FIXTURE_ID lit_zup_room
    FIXTURES_SETUP bsp_lightmaps_lit_fixture_writer
)

stellar_add_fixture_writer_test(
    TEST_NAME trenchbroom_package_material_wad_fixture_writer
    OUTPUT ${STELLAR_TB_MATERIAL_WAD_BSP_FIXTURE}
    FIXTURE_ID material_wad_zup
    FIXTURES_SETUP trenchbroom_package_material_wad_fixture_writer
)

stellar_add_fixture_writer_test(
    TEST_NAME brush_mover_door_button_fixture_writer
    OUTPUT ${STELLAR_TB_MOVING_DOOR_BSP_FIXTURE}
    FIXTURE_ID moving_door_button_zup
    FIXTURES_SETUP brush_mover_door_button_fixture_writer
)

stellar_add_fixture_writer_test(
    TEST_NAME bsp_authoring_smoke_point_volume_fixture_writer
    OUTPUT ${STELLAR_TB_POINT_VOLUME_BSP_FIXTURE}
    FIXTURE_ID point_volume_zup
    FIXTURES_SETUP bsp_authoring_smoke_point_volume_fixture_writer
)

stellar_add_fixture_writer_test(
    TEST_NAME bsp_authoring_smoke_illusionary_static_fixture_writer
    OUTPUT ${STELLAR_TB_ILLUSIONARY_STATIC_BSP_FIXTURE}
    FIXTURE_ID illusionary_static_zup
    FIXTURES_SETUP bsp_authoring_smoke_illusionary_static_fixture_writer
)

add_test(NAME client_cli_map_validation
    COMMAND $<TARGET_FILE:stellar-client> --validate-config --map ${STELLAR_CLI_BSP_FIXTURE}
)
set_tests_properties(client_cli_map_validation PROPERTIES FIXTURES_REQUIRED bsp_cli_fixture)

stellar_add_validate_map_test(
    TEST_NAME client_cli_validate_map
    COMMAND_TARGET stellar-client
    ARGS --validate-map
    MAP ${STELLAR_CLI_BSP_FIXTURE}
    FIXTURES_REQUIRED bsp_cli_fixture
)

stellar_add_validate_map_test(
    TEST_NAME bsp_lightmaps_lit_fixture_validate
    COMMAND_TARGET stellar-client
    ARGS --validate-map
    MAP ${STELLAR_TB_LIT_BSP_FIXTURE}
    FIXTURES_REQUIRED bsp_lightmaps_lit_fixture_writer
)

stellar_add_validate_map_test(
    TEST_NAME trenchbroom_package_material_wad_fixture_validate
    COMMAND_TARGET stellar-client
    ARGS --validate-map
    MAP ${STELLAR_TB_MATERIAL_WAD_BSP_FIXTURE}
    FIXTURES_REQUIRED trenchbroom_package_material_wad_fixture_writer
)

stellar_add_validate_map_test(
    TEST_NAME brush_mover_door_button_fixture_validate
    COMMAND_TARGET stellar-client
    ARGS --validate-map
    MAP ${STELLAR_TB_MOVING_DOOR_BSP_FIXTURE}
    FIXTURES_REQUIRED brush_mover_door_button_fixture_writer
)

stellar_add_validate_map_test(
    TEST_NAME bsp_authoring_smoke_point_volume_fixture_validate
    COMMAND_TARGET stellar-client
    ARGS --validate-map
    MAP ${STELLAR_TB_POINT_VOLUME_BSP_FIXTURE}
    FIXTURES_REQUIRED bsp_authoring_smoke_point_volume_fixture_writer
)

stellar_add_validate_map_test(
    TEST_NAME bsp_authoring_smoke_illusionary_static_fixture_validate
    COMMAND_TARGET stellar-client
    ARGS --validate-map
    MAP ${STELLAR_TB_ILLUSIONARY_STATIC_BSP_FIXTURE}
    FIXTURES_REQUIRED bsp_authoring_smoke_illusionary_static_fixture_writer
)

stellar_add_expected_failure_test(
    TEST_NAME client_cli_map_validation_rejects_bad_map
    FIXTURES_REQUIRED bsp_bad_cli_fixture
    COMMAND $<TARGET_FILE:stellar-client> --validate-map ${STELLAR_CLI_BAD_BSP_FIXTURE}
)

add_test(NAME server_cli_validate_config
    COMMAND $<TARGET_FILE:stellar-server> --validate-config --map ${STELLAR_CLI_BSP_FIXTURE}
)
set_tests_properties(server_cli_validate_config PROPERTIES FIXTURES_REQUIRED bsp_cli_fixture)

stellar_add_expected_failure_test(
    TEST_NAME server_cli_rejects_bad_map
    FIXTURES_REQUIRED bsp_bad_cli_fixture
    COMMAND $<TARGET_FILE:stellar-server> --validate-config --map ${STELLAR_CLI_BAD_BSP_FIXTURE}
)

add_test(NAME bsp_authoring_smoke_compile_wrapper
    COMMAND bash "${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/compile_trenchbroom_bsp30.sh"
        --map "${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/src/minimal_zup_room.map"
        --out "${STELLAR_TRENCHBROOM_FIXTURE_DIR}/compile_wrapper_smoke.bsp"
        --profile fast
        --allow-skip
)
set_tests_properties(bsp_authoring_smoke_compile_wrapper PROPERTIES
    ENVIRONMENT "STELLAR_CLIENT=$<TARGET_FILE:stellar-client>;STELLAR_SERVER=$<TARGET_FILE:stellar-server>"
    SKIP_RETURN_CODE 77
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

add_test(NAME trenchbroom_dev_texture_cpp_header_sync
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/create_stellar_dev_wad.py
        --check-cpp-header ${STELLAR_PROJECT_SOURCE_DIR}/src/import/bsp/DeveloperTextureSpecs.generated.hpp
)

add_test(NAME trenchbroom_dev_texture_doc_table_sync
    COMMAND bash -c "python3 '${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/create_stellar_dev_wad.py' --check-doc-table '${STELLAR_PROJECT_SOURCE_DIR}/docs/BspAuthoring.md' && python3 '${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/create_stellar_dev_wad.py' --check-doc-table '${STELLAR_PROJECT_SOURCE_DIR}/docs/TrenchBroom.md'"
)

add_test(NAME trenchbroom_fgd_sync_check
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/sync_stellar_fgd.py --check
)

add_test(NAME trenchbroom_fixture_manifest_sync
    COMMAND bash -c "python3 '${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/list_trenchbroom_fixtures.py' '${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/fixtures.json' --check-docs '${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/README.md' '${STELLAR_PROJECT_SOURCE_DIR}/docs/TrenchBroom.md' && bash '${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/run_vhlt_fixture_matrix.sh' --source-root '${STELLAR_PROJECT_SOURCE_DIR}' --build-root '${CMAKE_BINARY_DIR}' --list >/dev/null"
)

add_test(NAME docs_consistency
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/docs/check_docs_consistency.py
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

add_test(NAME trenchbroom_map_rewrite
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/test_map_rewrite.py
)

add_test(NAME trenchbroom_source_preflight_fixtures
    COMMAND bash -c "for f in '${STELLAR_TEST_FIXTURE_DIR}'/trenchbroom/src/*.map; do case \"$f\" in *invalid_*.map) continue ;; esac; python3 '${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/validate_trenchbroom_map_source.py' \"$f\" || exit 1; done"
)

stellar_add_expected_failure_test(
    TEST_NAME trenchbroom_source_preflight_rejects_malformed
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/validate_trenchbroom_map_source.py
        ${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/src/invalid_malformed_brush.map
)

stellar_add_expected_failure_test(
    TEST_NAME trenchbroom_source_preflight_rejects_incomplete_brush
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/validate_trenchbroom_map_source.py
        ${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/src/invalid_incomplete_brush.map
)

stellar_add_expected_failure_test(
    TEST_NAME trenchbroom_source_preflight_rejects_script_escape
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/validate_trenchbroom_map_source.py
        ${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/src/invalid_script_escape_zup.map
)

stellar_add_expected_failure_test(
    TEST_NAME trenchbroom_source_preflight_rejects_missing_target
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/validate_trenchbroom_map_source.py
        ${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/src/invalid_missing_target.map
)

stellar_add_expected_failure_test(
    TEST_NAME trenchbroom_source_preflight_rejects_missing_wad_texture
    COMMAND python3 ${STELLAR_PROJECT_SOURCE_DIR}/tools/bsp/validate_trenchbroom_map_source.py
        --strict-textures
        --wad-root ${STELLAR_PROJECT_SOURCE_DIR}/tools/trenchbroom/Stellar/materials
        ${STELLAR_TEST_FIXTURE_DIR}/trenchbroom/src/invalid_missing_wad_texture.map
)
