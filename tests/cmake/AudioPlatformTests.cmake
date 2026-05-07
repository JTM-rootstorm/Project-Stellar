# Audio and platform test registrations.

stellar_add_test_executable(stellar_platform_input_test input
    ${STELLAR_TEST_SOURCE_DIR}/platform/Input.cpp
)
target_link_libraries(stellar_platform_input_test PRIVATE stellar_platform)

stellar_add_test_executable(stellar_audio_event_router_test audio_event_router
    ${STELLAR_TEST_SOURCE_DIR}/audio/AudioEventRouter.cpp
)
target_link_libraries(stellar_audio_event_router_test PRIVATE stellar_audio)

stellar_add_test_executable(stellar_miniaudio_sink_test miniaudio_sink
    ${STELLAR_TEST_SOURCE_DIR}/audio/MiniaudioSink.cpp
)
target_compile_definitions(stellar_miniaudio_sink_test PRIVATE
    STELLAR_PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(stellar_miniaudio_sink_test PRIVATE stellar_audio_miniaudio)

stellar_add_test_executable(stellar_generated_footstep_assets_test generated_footstep_assets
    ${STELLAR_TEST_SOURCE_DIR}/audio/GeneratedFootstepAssets.cpp
)
target_compile_definitions(stellar_generated_footstep_assets_test PRIVATE
    STELLAR_PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

stellar_add_test_executable(stellar_footstep_audio_pipeline_test footstep_audio_pipeline
    ${STELLAR_TEST_SOURCE_DIR}/integration/FootstepAudioPipeline.cpp
)
target_link_libraries(stellar_footstep_audio_pipeline_test PRIVATE
    stellar_audio
    stellar_authority
    stellar_server_core
)
