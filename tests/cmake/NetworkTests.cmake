# Network test registrations.

stellar_add_test_executable(stellar_snapshot_codec_test snapshot_codec
    ${STELLAR_TEST_SOURCE_DIR}/network/SnapshotCodec.cpp
)
target_link_libraries(stellar_snapshot_codec_test PRIVATE stellar_protocol)
add_test(NAME protocol_snapshot_codec COMMAND $<TARGET_FILE:stellar_snapshot_codec_test>)

stellar_add_test_executable(stellar_network_session_test network_session
    ${STELLAR_TEST_SOURCE_DIR}/network/Session.cpp
)
target_link_libraries(stellar_network_session_test PRIVATE stellar_server_runtime stellar_client_net)

stellar_add_test_executable(stellar_loopback_transport_test loopback_transport
    ${STELLAR_TEST_SOURCE_DIR}/network/LoopbackTransport.cpp
)
target_link_libraries(stellar_loopback_transport_test PRIVATE stellar_transport)
add_test(NAME transport_loopback COMMAND $<TARGET_FILE:stellar_loopback_transport_test>)

stellar_add_test_executable(stellar_socket_transport_test socket_transport
    ${STELLAR_TEST_SOURCE_DIR}/network/SocketTransport.cpp
)
target_link_libraries(stellar_socket_transport_test PRIVATE stellar_transport stellar_client_net)
add_test(NAME transport_socket COMMAND $<TARGET_FILE:stellar_socket_transport_test>)

stellar_add_test_executable(stellar_snapshot_delta_test snapshot_delta
    ${STELLAR_TEST_SOURCE_DIR}/network/SnapshotDelta.cpp
)
target_link_libraries(stellar_snapshot_delta_test PRIVATE stellar_protocol)
add_test(NAME protocol_snapshot_delta COMMAND $<TARGET_FILE:stellar_snapshot_delta_test>)

add_custom_target(stellar_protocol_test
    DEPENDS
        stellar_snapshot_codec_test
        stellar_snapshot_delta_test
)

add_custom_target(stellar_transport_test
    DEPENDS
        stellar_loopback_transport_test
        stellar_socket_transport_test
)
