# BSP importer and BSP gameplay smoke test registrations.

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
