# Audio and platform test registrations.

stellar_add_test_executable(stellar_platform_input_test input
    ${STELLAR_TEST_SOURCE_DIR}/platform/Input.cpp
)
target_link_libraries(stellar_platform_input_test PRIVATE stellar_platform)

stellar_add_test_executable(stellar_audio_event_router_test audio_event_router
    ${STELLAR_TEST_SOURCE_DIR}/audio/AudioEventRouter.cpp
)
target_link_libraries(stellar_audio_event_router_test PRIVATE stellar_audio)

stellar_add_test_executable(stellar_generated_footstep_assets_test generated_footstep_assets
    ${STELLAR_TEST_SOURCE_DIR}/audio/GeneratedFootstepAssets.cpp
)
target_compile_definitions(stellar_generated_footstep_assets_test PRIVATE
    STELLAR_PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
