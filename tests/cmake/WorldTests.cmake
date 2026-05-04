# World, core, and physics test registrations.

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

stellar_add_physics_test(stellar_collision_world_test collision_world
    ${STELLAR_TEST_SOURCE_DIR}/physics/CollisionWorld.cpp
)

stellar_add_physics_test(stellar_character_controller_test character_controller
    ${STELLAR_TEST_SOURCE_DIR}/physics/CharacterController.cpp
)

stellar_add_physics_test(stellar_collision_performance_regression_test collision_performance_regression
    ${STELLAR_TEST_SOURCE_DIR}/physics/CollisionPerformanceRegression.cpp
)
