#include "stellar/client/ClientWorldReceiver.hpp"

#include <iterator>
#include <utility>

#include "stellar/network/SnapshotCodec.hpp"
#include "stellar/network/SnapshotDelta.hpp"

namespace stellar::client {
namespace {

void merge(ClientWorldReceiverDrainResult& total, ClientWorldReceiverDrainResult next) {
    total.snapshots += next.snapshots;
    total.deltas += next.deltas;
    total.events += next.events;
    total.rejected_packets += next.rejected_packets;
    total.errors.insert(total.errors.end(), std::make_move_iterator(next.errors.begin()),
                        std::make_move_iterator(next.errors.end()));
}

[[nodiscard]] ClientWorldReceiverDrainResult rejected(std::string code, std::string message) {
    ClientWorldReceiverDrainResult result;
    result.rejected_packets = 1;
    result.errors.push_back(ClientWorldReceiverError{.code = std::move(code),
                                                     .message = std::move(message)});
    return result;
}

} // namespace

ClientWorldReceiverDrainResult ClientWorldReceiver::accept_packet(
    const stellar::network::TransportPacket& packet) noexcept {
    const std::vector<std::uint8_t> bytes = stellar::network::from_payload(packet.payload);

    if (auto snapshot = stellar::network::decode_snapshot(bytes)) {
        latest_snapshot_ = std::move(*snapshot);
        ClientWorldReceiverDrainResult result;
        result.snapshots = 1;
        return result;
    }

    if (auto delta = stellar::network::decode_snapshot_delta(bytes)) {
        if (!latest_snapshot_) {
            return rejected("missing_snapshot_baseline",
                            "Snapshot delta arrived before any authoritative snapshot baseline");
        }
        auto applied = stellar::network::apply_snapshot_delta(*latest_snapshot_, *delta);
        if (!applied) {
            return rejected(applied.error().code, applied.error().message);
        }
        latest_snapshot_ = std::move(*applied);
        ClientWorldReceiverDrainResult result;
        result.deltas = 1;
        return result;
    }

    if (auto event = stellar::network::decode_gameplay_event(bytes)) {
        queued_events_.push_back(std::move(*event));
        ClientWorldReceiverDrainResult result;
        result.events = 1;
        return result;
    }

    return rejected("unknown_server_packet", "Packet did not decode as snapshot, delta, or event");
}

ClientWorldReceiverDrainResult ClientWorldReceiver::drain(
    stellar::network::ClientTransport& transport) noexcept {
    ClientWorldReceiverDrainResult total;
    for (const stellar::network::TransportPacket& packet : transport.receive_from_server()) {
        merge(total, accept_packet(packet));
    }
    return total;
}

bool ClientWorldReceiver::has_snapshot() const noexcept {
    return latest_snapshot_.has_value();
}

const std::optional<stellar::network::NetworkWorldSnapshot>& ClientWorldReceiver::latest_snapshot()
    const noexcept {
    return latest_snapshot_;
}

const std::vector<stellar::network::GameplayEvent>& ClientWorldReceiver::queued_events() const noexcept {
    return queued_events_;
}

std::vector<stellar::network::GameplayEvent> ClientWorldReceiver::take_queued_events() noexcept {
    std::vector<stellar::network::GameplayEvent> events;
    events.swap(queued_events_);
    return events;
}

void ClientWorldReceiver::reset() noexcept {
    latest_snapshot_.reset();
    queued_events_.clear();
}

} // namespace stellar::client
