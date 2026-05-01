#include "stellar/network/Transport.hpp"

#include <deque>
#include <memory>
#include <utility>

namespace stellar::network {
namespace {

struct LoopbackQueues {
    std::deque<TransportPacket> client_to_server;
    std::deque<TransportPacket> server_to_client;
};

[[nodiscard]] std::vector<TransportPacket> drain(std::deque<TransportPacket>& queue) {
    std::vector<TransportPacket> packets;
    packets.reserve(queue.size());
    while (!queue.empty()) {
        packets.push_back(std::move(queue.front()));
        queue.pop_front();
    }
    return packets;
}

class LoopbackClientTransport final : public ClientTransport {
public:
    explicit LoopbackClientTransport(std::shared_ptr<LoopbackQueues> queues)
        : queues_(std::move(queues)) {}

    std::expected<void, TransportError> send_to_server(TransportPacket packet) override {
        if (!queues_) {
            return std::unexpected(TransportError{"disconnected", "Loopback client is disconnected"});
        }
        queues_->client_to_server.push_back(std::move(packet));
        return {};
    }

    std::vector<TransportPacket> receive_from_server() override {
        if (!queues_) {
            return {};
        }
        return drain(queues_->server_to_client);
    }

private:
    std::shared_ptr<LoopbackQueues> queues_;
};

class LoopbackServerTransport final : public ServerTransport {
public:
    explicit LoopbackServerTransport(std::shared_ptr<LoopbackQueues> queues)
        : queues_(std::move(queues)) {}

    std::vector<TransportPacket> receive_from_client() override {
        if (!queues_) {
            return {};
        }
        return drain(queues_->client_to_server);
    }

    std::expected<void, TransportError> send_to_client(TransportPacket packet) override {
        if (!queues_) {
            return std::unexpected(TransportError{"disconnected", "Loopback server is disconnected"});
        }
        queues_->server_to_client.push_back(std::move(packet));
        return {};
    }

private:
    std::shared_ptr<LoopbackQueues> queues_;
};

} // namespace

LoopbackTransportPair make_loopback_transport_pair() {
    auto queues = std::make_shared<LoopbackQueues>();
    return LoopbackTransportPair{
        .client = std::make_unique<LoopbackClientTransport>(queues),
        .server = std::make_unique<LoopbackServerTransport>(std::move(queues)),
    };
}

std::vector<std::byte> to_payload(const std::vector<std::uint8_t>& bytes) {
    std::vector<std::byte> payload;
    payload.reserve(bytes.size());
    for (std::uint8_t byte : bytes) {
        payload.push_back(static_cast<std::byte>(byte));
    }
    return payload;
}

std::vector<std::uint8_t> from_payload(const std::vector<std::byte>& payload) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(payload.size());
    for (std::byte byte : payload) {
        bytes.push_back(static_cast<std::uint8_t>(byte));
    }
    return bytes;
}

} // namespace stellar::network
