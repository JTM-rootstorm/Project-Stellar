# Server-authoritative test registrations.

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
