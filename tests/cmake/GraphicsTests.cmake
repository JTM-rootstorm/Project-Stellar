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

if(STELLAR_ENABLE_METAL)
    add_executable(stellar_metal_shader_compile_test
        ${STELLAR_TEST_SOURCE_DIR}/graphics/MetalShaderCompile.mm
    )

    target_include_directories(stellar_metal_shader_compile_test PRIVATE
        ${STELLAR_PROJECT_SOURCE_DIR}/include
    )

    target_link_libraries(stellar_metal_shader_compile_test PRIVATE
        stellar_graphics
        ${METAL_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
    )

    add_test(NAME metal_shader_compile COMMAND $<TARGET_FILE:stellar_metal_shader_compile_test>)
    set_tests_properties(metal_shader_compile PROPERTIES SKIP_RETURN_CODE 77)
endif()

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

if(STELLAR_ENABLE_VULKAN_BACKEND)
    add_executable(stellar_vulkan_shader_compile_test
        ${STELLAR_TEST_SOURCE_DIR}/graphics/VulkanShaderCompile.cpp
    )

    target_compile_definitions(stellar_vulkan_shader_compile_test PRIVATE
        STELLAR_VULKAN_VERTEX_SPV="${STELLAR_VULKAN_VERTEX_SPV}"
        STELLAR_VULKAN_FRAGMENT_SPV="${STELLAR_VULKAN_FRAGMENT_SPV}"
    )

    target_include_directories(stellar_vulkan_shader_compile_test PRIVATE
        ${STELLAR_PROJECT_SOURCE_DIR}/include
    )

    add_dependencies(stellar_vulkan_shader_compile_test stellar_vulkan_shaders)

    add_test(NAME vulkan_shader_compile
        COMMAND $<TARGET_FILE:stellar_vulkan_shader_compile_test>)
endif()

if(STELLAR_ENABLE_VULKAN_BACKEND AND STELLAR_ENABLE_VULKAN_CONTEXT_TESTS)
    add_executable(stellar_vulkan_context_smoke_test
        ${STELLAR_TEST_SOURCE_DIR}/graphics/VulkanContextSmoke.cpp
    )

    target_include_directories(stellar_vulkan_context_smoke_test PRIVATE
        ${STELLAR_PROJECT_SOURCE_DIR}/include
    )

    target_link_libraries(stellar_vulkan_context_smoke_test PRIVATE
        stellar_graphics
    )

    add_test(NAME vulkan_context_smoke
        COMMAND $<TARGET_FILE:stellar_vulkan_context_smoke_test>)
    set_tests_properties(vulkan_context_smoke PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(stellar_vulkan_render_readback_test
        ${STELLAR_TEST_SOURCE_DIR}/graphics/VulkanRenderReadback.cpp
    )

    target_include_directories(stellar_vulkan_render_readback_test PRIVATE
        ${STELLAR_PROJECT_SOURCE_DIR}/include
    )

    target_link_libraries(stellar_vulkan_render_readback_test PRIVATE
        stellar_graphics
    )

    add_test(NAME vulkan_render_readback
        COMMAND $<TARGET_FILE:stellar_vulkan_render_readback_test>)
    set_tests_properties(vulkan_render_readback PROPERTIES SKIP_RETURN_CODE 77)
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

    add_executable(stellar_metal_render_readback_test
        ${STELLAR_TEST_SOURCE_DIR}/graphics/MetalRenderReadback.mm
    )

    target_include_directories(stellar_metal_render_readback_test PRIVATE
        ${STELLAR_PROJECT_SOURCE_DIR}/include
    )

    target_link_libraries(stellar_metal_render_readback_test PRIVATE
        stellar_graphics
    )

    add_test(NAME metal_render_readback
        COMMAND $<TARGET_FILE:stellar_metal_render_readback_test>)
    set_tests_properties(metal_render_readback PROPERTIES SKIP_RETURN_CODE 77)
endif()
