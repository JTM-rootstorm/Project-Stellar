#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "stellar/network/Messages.hpp"
#include "stellar/network/Transport.hpp"

namespace stellar::client {

/** @brief Failure returned while decoding authoritative client-world packets. */
struct ClientWorldReceiverError {
    /** @brief Stable machine-readable failure code. */
    std::string code;

    /** @brief Human-readable deterministic diagnostic. */
    std::string message;
};

/** @brief Result of draining authoritative server packets into the receiver. */
struct ClientWorldReceiverDrainResult {
    /** @brief Count of full snapshots accepted. */
    std::uint32_t snapshots = 0;

    /** @brief Count of deltas accepted and applied to the current baseline. */
    std::uint32_t deltas = 0;

    /** @brief Count of server-approved gameplay events queued for presentation. */
    std::uint32_t events = 0;

    /** @brief Count of malformed packets rejected without crashing. */
    std::uint32_t rejected_packets = 0;

    /** @brief Diagnostics for rejected packets. */
    std::vector<ClientWorldReceiverError> errors;
};

/**
 * @brief Client-side authoritative world receiver with no prediction or reconciliation.
 *
 * The receiver accepts full snapshots, applies deltas to the current baseline, and queues
 * server-approved gameplay events for presentation systems.
 */
class ClientWorldReceiver {
public:
    /** @brief Decode and apply one authoritative server packet. */
    [[nodiscard]] ClientWorldReceiverDrainResult accept_packet(
        const stellar::network::TransportPacket& packet) noexcept;

    /** @brief Drain and apply all currently delivered server packets. */
    [[nodiscard]] ClientWorldReceiverDrainResult drain(
        stellar::network::ClientTransport& transport) noexcept;

    /** @brief Return true when at least one authoritative snapshot baseline exists. */
    [[nodiscard]] bool has_snapshot() const noexcept;

    /** @brief Return the latest authoritative snapshot baseline, if present. */
    [[nodiscard]] const std::optional<stellar::network::NetworkWorldSnapshot>& latest_snapshot()
        const noexcept;

    /** @brief Return queued server-approved gameplay events without clearing them. */
    [[nodiscard]] const std::vector<stellar::network::GameplayEvent>& queued_events() const noexcept;

    /** @brief Move queued server-approved gameplay events out of the receiver. */
    [[nodiscard]] std::vector<stellar::network::GameplayEvent> take_queued_events() noexcept;

    /** @brief Clear authoritative baseline and queued events. */
    void reset() noexcept;

private:
    std::optional<stellar::network::NetworkWorldSnapshot> latest_snapshot_;
    std::vector<stellar::network::GameplayEvent> queued_events_;
};

} // namespace stellar::client
