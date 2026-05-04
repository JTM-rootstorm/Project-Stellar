# Scripting test registrations.

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
