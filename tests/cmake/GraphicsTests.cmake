# Graphics test registrations.

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

if(STELLAR_ENABLE_OPENGL_BACKEND AND STELLAR_ENABLE_OPENGL_CONTEXT_TESTS)
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

if(STELLAR_ENABLE_METAL AND STELLAR_ENABLE_METAL_CONTEXT_TESTS)
    add_executable(stellar_metal_context_smoke_test
        ${STELLAR_TEST_SOURCE_DIR}/graphics/MetalContextSmoke.cpp
    )

    target_include_directories(stellar_metal_context_smoke_test PRIVATE
        ${STELLAR_PROJECT_SOURCE_DIR}/include
    )

    target_link_libraries(stellar_metal_context_smoke_test PRIVATE
        stellar_graphics
    )

    add_test(NAME metal_context_smoke COMMAND $<TARGET_FILE:stellar_metal_context_smoke_test>)
    set_tests_properties(metal_context_smoke PROPERTIES SKIP_RETURN_CODE 77)
endif()
