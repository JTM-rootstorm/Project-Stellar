#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace stellar::network {

/** @brief Transport delivery class requested by the message path. */
enum class TransportChannel : std::uint8_t {
    kReliable = 0,
    kUnreliable = 1,
};

/** @brief Opaque transport packet carrying one encoded network message. */
struct TransportPacket {
    /** @brief Requested delivery channel for this packet. */
    TransportChannel channel = TransportChannel::kReliable;

    /** @brief Encoded network payload bytes. */
    std::vector<std::byte> payload;
};

/** @brief Failure returned by transport endpoints. */
struct TransportError {
    /** @brief Stable machine-readable failure code. */
    std::string code;

    /** @brief Human-readable deterministic diagnostic. */
    std::string message;
};

/** @brief Client-side endpoint for transport-neutral message exchange. */
class ClientTransport {
public:
    virtual ~ClientTransport() = default;

    /** @brief Queue one packet for delivery to the authoritative server endpoint. */
    [[nodiscard]] virtual std::expected<void, TransportError> send_to_server(
        TransportPacket packet) = 0;

    /** @brief Drain packets delivered by the authoritative server endpoint. */
    [[nodiscard]] virtual std::vector<TransportPacket> receive_from_server() = 0;
};

/** @brief Server-side endpoint for transport-neutral message exchange. */
class ServerTransport {
public:
    virtual ~ServerTransport() = default;

    /** @brief Drain packets delivered by the client endpoint. */
    [[nodiscard]] virtual std::vector<TransportPacket> receive_from_client() = 0;

    /** @brief Queue one packet for delivery to the client endpoint. */
    [[nodiscard]] virtual std::expected<void, TransportError> send_to_client(
        TransportPacket packet) = 0;
};

/** @brief Pair of connected in-memory client/server transport endpoints. */
struct LoopbackTransportPair {
    /** @brief Client endpoint connected to the paired server endpoint. */
    std::unique_ptr<ClientTransport> client;

    /** @brief Server endpoint connected to the paired client endpoint. */
    std::unique_ptr<ServerTransport> server;
};

/** @brief Create connected in-memory endpoints preserving FIFO order for reliable packets. */
[[nodiscard]] LoopbackTransportPair make_loopback_transport_pair();

/** @brief Convert uint8_t codec bytes into transport payload bytes. */
[[nodiscard]] std::vector<std::byte> to_payload(const std::vector<std::uint8_t>& bytes);

/** @brief Convert transport payload bytes into uint8_t codec bytes. */
[[nodiscard]] std::vector<std::uint8_t> from_payload(const std::vector<std::byte>& payload);

} // namespace stellar::network
