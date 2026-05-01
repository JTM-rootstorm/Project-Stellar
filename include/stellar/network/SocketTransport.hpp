#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "stellar/network/Transport.hpp"

namespace stellar::network {

/** @brief Parsed TCP socket endpoint host and port. */
struct SocketAddress {
    /** @brief DNS name or numeric IPv4 address. */
    std::string host;

    /** @brief TCP port in host byte order. Port zero requests an ephemeral bind port. */
    std::uint16_t port = 0;
};

/** @brief TCP socket transport configuration. */
struct SocketTransportConfig {
    /** @brief Maximum payload bytes accepted or sent in one transport packet. */
    std::size_t max_packet_bytes = 1024 * 1024;

    /** @brief Use nonblocking sockets and bounded poll loops. */
    bool nonblocking = true;
};

/** @brief Server transport plus the concrete bound endpoint selected by the OS. */
struct TcpServerTransportHandle {
    /** @brief Single-client server transport endpoint. */
    std::unique_ptr<ServerTransport> server;

    /** @brief Concrete address clients can use after bind, including ephemeral port resolution. */
    SocketAddress bound_address;
};

/** @brief Maximum default TCP payload size accepted by socket transport. */
inline constexpr std::size_t kDefaultSocketMaxPayloadBytes = 1024 * 1024;

/** @brief Parse a host:port socket endpoint with deterministic diagnostics. */
[[nodiscard]] std::expected<SocketAddress, TransportError> parse_socket_address(
    std::string_view endpoint);

/** @brief Format a parsed socket endpoint as host:port. */
[[nodiscard]] std::string format_socket_address(const SocketAddress& address);

/** @brief Encode one packet into the TCP length-prefixed socket envelope. */
[[nodiscard]] std::expected<std::vector<std::byte>, TransportError> encode_socket_packet(
    const TransportPacket& packet,
    SocketTransportConfig config = {});

/** @brief Decode all complete packets currently present in an envelope byte stream. */
[[nodiscard]] std::expected<std::vector<TransportPacket>, TransportError> decode_socket_packets(
    std::vector<std::byte>& stream,
    SocketTransportConfig config = {});

/** @brief Connect a TCP client transport to a parsed host:port endpoint. */
[[nodiscard]] std::expected<std::unique_ptr<ClientTransport>, TransportError> connect_tcp_client(
    std::string_view endpoint,
    SocketTransportConfig config = {});

/** @brief Bind/listen a single-client TCP server transport on a parsed host:port endpoint. */
[[nodiscard]] std::expected<std::unique_ptr<ServerTransport>, TransportError> listen_tcp_server_once(
    std::string_view bind_endpoint,
    SocketTransportConfig config = {});

/** @brief Bind/listen a single-client TCP server and return the concrete bound address. */
[[nodiscard]] std::expected<TcpServerTransportHandle, TransportError>
listen_tcp_server_once_with_bound_address(std::string_view bind_endpoint,
                                          SocketTransportConfig config = {});

} // namespace stellar::network
