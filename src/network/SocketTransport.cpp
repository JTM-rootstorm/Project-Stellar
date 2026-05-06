#include "stellar/network/SocketTransport.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace stellar::network {
namespace {

constexpr std::array<std::byte, 4> kMagic{std::byte{'S'}, std::byte{'T'}, std::byte{'C'},
                                          std::byte{'P'}};
constexpr std::uint8_t kEnvelopeVersion = 1;
constexpr std::size_t kHeaderBytes = 10;
constexpr int kPollTimeoutMs = 1;
constexpr int kBoundedIoAttempts = 1024;

[[nodiscard]] TransportError error(std::string code, std::string message) {
    return TransportError{.code = std::move(code), .message = std::move(message)};
}

[[nodiscard]] TransportError errno_error(std::string code, std::string operation) {
    return error(std::move(code), operation + ": " + std::strerror(errno));
}

[[nodiscard]] bool would_block() noexcept {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

[[nodiscard]] int send_flags() noexcept {
#if defined(MSG_NOSIGNAL)
    return MSG_NOSIGNAL;
#else
    return 0;
#endif
}

void configure_sigpipe_policy(int fd) noexcept {
#if defined(__APPLE__) && defined(SO_NOSIGPIPE)
    const int enabled = 1;
    (void)::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#else
    (void)fd;
#endif
}

[[nodiscard]] std::uint8_t channel_byte(TransportChannel channel) noexcept {
    return static_cast<std::uint8_t>(channel);
}

[[nodiscard]] std::expected<TransportChannel, TransportError> parse_channel(std::byte byte) {
    const auto value = static_cast<std::uint8_t>(byte);
    if (value == static_cast<std::uint8_t>(TransportChannel::kReliable)) {
        return TransportChannel::kReliable;
    }
    if (value == static_cast<std::uint8_t>(TransportChannel::kUnreliable)) {
        return TransportChannel::kUnreliable;
    }
    return std::unexpected(error("invalid_channel", "Socket packet channel is unsupported"));
}

void append_u32_le(std::vector<std::byte>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 24U) & 0xFFU));
}

[[nodiscard]] std::uint32_t read_u32_le(const std::vector<std::byte>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[offset])) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[offset + 1])) << 8U) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[offset + 2])) << 16U) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[offset + 3])) << 24U);
}

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)

class SocketHandle {
public:
    SocketHandle() = default;
    explicit SocketHandle(int fd) noexcept : fd_(fd) {}
    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    SocketHandle(SocketHandle&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    SocketHandle& operator=(SocketHandle&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }
    ~SocketHandle() { reset(); }

    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }
    void reset(int next = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = next;
    }

private:
    int fd_ = -1;
};

[[nodiscard]] std::expected<void, TransportError> set_nonblocking(int fd, bool enabled) {
    if (!enabled) {
        return {};
    }
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return std::unexpected(errno_error("fcntl_failed", "fcntl(F_GETFL) failed"));
    }
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return std::unexpected(errno_error("fcntl_failed", "fcntl(F_SETFL) failed"));
    }
    return {};
}

[[nodiscard]] std::expected<sockaddr_storage, TransportError> resolve_endpoint(
    const SocketAddress& address,
    bool passive) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = passive ? AI_PASSIVE : 0;

    addrinfo* results = nullptr;
    const std::string port = std::to_string(address.port);
    const int rc = ::getaddrinfo(address.host.c_str(), port.c_str(), &hints, &results);
    if (rc != 0) {
        return std::unexpected(error("resolve_failed", std::string("Endpoint resolution failed: ") +
                                                        ::gai_strerror(rc)));
    }

    sockaddr_storage storage{};
    bool found = false;
    for (addrinfo* current = results; current != nullptr; current = current->ai_next) {
        if (current->ai_addrlen <= sizeof(storage)) {
            std::memcpy(&storage, current->ai_addr, current->ai_addrlen);
            found = true;
            break;
        }
    }
    ::freeaddrinfo(results);
    if (!found) {
        return std::unexpected(error("resolve_failed", "Endpoint resolution returned no IPv4 TCP address"));
    }
    return storage;
}

[[nodiscard]] SocketAddress bound_address_for(int fd, const std::string& fallback_host) {
    sockaddr_storage storage{};
    socklen_t length = sizeof(storage);
    SocketAddress address{.host = fallback_host, .port = 0};
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&storage), &length) != 0) {
        return address;
    }
    if (storage.ss_family == AF_INET) {
        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(&storage);
        address.port = ntohs(ipv4->sin_port);
        if (address.host == "0.0.0.0") {
            address.host = "127.0.0.1";
        }
    }
    return address;
}

class TcpEndpoint {
public:
    explicit TcpEndpoint(SocketTransportConfig config) : config_(config) {}

    [[nodiscard]] std::expected<void, TransportError> attach(SocketHandle socket) {
        socket_ = std::move(socket);
        return set_nonblocking(socket_.get(), config_.nonblocking);
    }

    [[nodiscard]] std::expected<void, TransportError> send_packet(TransportPacket packet) {
        if (!socket_.valid() || disconnected_) {
            return std::unexpected(error("disconnected", "TCP peer is disconnected"));
        }
        auto encoded = encode_socket_packet(packet, config_);
        if (!encoded) {
            return std::unexpected(encoded.error());
        }
        std::size_t written = 0;
        int attempts = 0;
        while (written < encoded->size() && attempts++ < kBoundedIoAttempts) {
            const auto* data = reinterpret_cast<const char*>(encoded->data() + written);
            const std::size_t remaining = encoded->size() - written;
            const ssize_t result = ::send(socket_.get(), data, remaining, send_flags());
            if (result > 0) {
                written += static_cast<std::size_t>(result);
                continue;
            }
            if (result == 0) {
                disconnected_ = true;
                return std::unexpected(error("disconnected", "TCP peer closed during send"));
            }
            if (would_block()) {
                pollfd descriptor{.fd = socket_.get(), .events = POLLOUT, .revents = 0};
                (void)::poll(&descriptor, 1, kPollTimeoutMs);
                continue;
            }
            disconnected_ = true;
            return std::unexpected(errno_error("send_failed", "TCP send failed"));
        }
        if (written != encoded->size()) {
            return std::unexpected(error("send_would_block", "TCP send did not complete within bounded attempts"));
        }
        return {};
    }

    [[nodiscard]] std::vector<TransportPacket> receive_packets() {
        if (!socket_.valid() || disconnected_) {
            return {};
        }
        std::array<std::byte, 4096> buffer{};
        for (int attempts = 0; attempts < kBoundedIoAttempts; ++attempts) {
            const ssize_t count = ::recv(socket_.get(), buffer.data(), buffer.size(), 0);
            if (count > 0) {
                rx_.insert(rx_.end(), buffer.begin(), buffer.begin() + count);
                continue;
            }
            if (count == 0) {
                disconnected_ = true;
                break;
            }
            if (would_block()) {
                break;
            }
            disconnected_ = true;
            break;
        }
        auto decoded = decode_socket_packets(rx_, config_);
        if (!decoded) {
            disconnected_ = true;
            rx_.clear();
            return {};
        }
        return std::move(*decoded);
    }

private:
    SocketTransportConfig config_;
    SocketHandle socket_;
    std::vector<std::byte> rx_;
    bool disconnected_ = false;
};

class TcpClientTransport final : public ClientTransport {
public:
    TcpClientTransport(SocketHandle socket, SocketTransportConfig config) : endpoint_(config) {
        [[maybe_unused]] auto attached = endpoint_.attach(std::move(socket));
    }

    std::expected<void, TransportError> send_to_server(TransportPacket packet) override {
        return endpoint_.send_packet(std::move(packet));
    }

    std::vector<TransportPacket> receive_from_server() override { return endpoint_.receive_packets(); }

private:
    TcpEndpoint endpoint_;
};

class TcpServerTransport final : public ServerTransport {
public:
    TcpServerTransport(SocketHandle listen_socket, SocketTransportConfig config)
        : listen_socket_(std::move(listen_socket)), config_(config), endpoint_(config) {}

    std::vector<TransportPacket> receive_from_client() override {
        accept_once();
        if (!accepted_) {
            return {};
        }
        return endpoint_.receive_packets();
    }

    std::expected<void, TransportError> send_to_client(TransportPacket packet) override {
        accept_once();
        if (!accepted_) {
            return std::unexpected(error("not_connected", "TCP server has not accepted a client"));
        }
        return endpoint_.send_packet(std::move(packet));
    }

private:
    void accept_once() {
        if (accepted_ || !listen_socket_.valid()) {
            return;
        }
        sockaddr_storage address{};
        socklen_t length = sizeof(address);
        const int fd = ::accept(listen_socket_.get(), reinterpret_cast<sockaddr*>(&address), &length);
        if (fd < 0) {
            return;
        }
        SocketHandle client(fd);
        configure_sigpipe_policy(client.get());
        if (auto nonblocking = set_nonblocking(client.get(), config_.nonblocking); !nonblocking) {
            return;
        }
        if (auto attached = endpoint_.attach(std::move(client)); attached) {
            accepted_ = true;
        }
    }

    SocketHandle listen_socket_;
    SocketTransportConfig config_;
    TcpEndpoint endpoint_;
    bool accepted_ = false;
};

#endif

} // namespace

std::expected<SocketAddress, TransportError> parse_socket_address(std::string_view endpoint) {
    if (endpoint.empty()) {
        return std::unexpected(error("empty_endpoint", "Socket endpoint is empty"));
    }
    const std::size_t colon = endpoint.rfind(':');
    if (colon == std::string_view::npos) {
        return std::unexpected(error("missing_port", "Socket endpoint must be host:port"));
    }
    if (colon == 0) {
        return std::unexpected(error("empty_host", "Socket endpoint host is empty"));
    }
    if (colon + 1 == endpoint.size()) {
        return std::unexpected(error("empty_port", "Socket endpoint port is empty"));
    }
    const std::string_view host = endpoint.substr(0, colon);
    const std::string_view port_text = endpoint.substr(colon + 1);
    unsigned int port = 0;
    const auto [ptr, ec] = std::from_chars(port_text.data(), port_text.data() + port_text.size(), port);
    if (ec != std::errc{} || ptr != port_text.data() + port_text.size()) {
        return std::unexpected(error("invalid_port", "Socket endpoint port is not a base-10 integer"));
    }
    if (port > std::numeric_limits<std::uint16_t>::max()) {
        return std::unexpected(error("port_out_of_range", "Socket endpoint port is out of range"));
    }
    return SocketAddress{.host = std::string(host), .port = static_cast<std::uint16_t>(port)};
}

std::string format_socket_address(const SocketAddress& address) {
    return address.host + ":" + std::to_string(address.port);
}

std::expected<std::vector<std::byte>, TransportError> encode_socket_packet(
    const TransportPacket& packet,
    SocketTransportConfig config) {
    if (config.max_packet_bytes == 0) {
        return std::unexpected(error("invalid_max_payload", "Socket max payload must be nonzero"));
    }
    if (packet.payload.size() > config.max_packet_bytes ||
        packet.payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(error("payload_too_large", "Socket packet payload exceeds configured maximum"));
    }
    std::vector<std::byte> bytes;
    bytes.reserve(kHeaderBytes + packet.payload.size());
    bytes.insert(bytes.end(), kMagic.begin(), kMagic.end());
    bytes.push_back(static_cast<std::byte>(kEnvelopeVersion));
    bytes.push_back(static_cast<std::byte>(channel_byte(packet.channel)));
    append_u32_le(bytes, static_cast<std::uint32_t>(packet.payload.size()));
    bytes.insert(bytes.end(), packet.payload.begin(), packet.payload.end());
    return bytes;
}

std::expected<std::vector<TransportPacket>, TransportError> decode_socket_packets(
    std::vector<std::byte>& stream,
    SocketTransportConfig config) {
    if (config.max_packet_bytes == 0) {
        return std::unexpected(error("invalid_max_payload", "Socket max payload must be nonzero"));
    }
    std::vector<TransportPacket> packets;
    std::size_t offset = 0;
    while (stream.size() - offset >= kHeaderBytes) {
        if (!std::equal(kMagic.begin(), kMagic.end(), stream.begin() + offset)) {
            return std::unexpected(error("invalid_magic", "Socket packet magic is invalid"));
        }
        if (static_cast<std::uint8_t>(stream[offset + 4]) != kEnvelopeVersion) {
            return std::unexpected(error("invalid_version", "Socket packet envelope version is unsupported"));
        }
        auto channel = parse_channel(stream[offset + 5]);
        if (!channel) {
            return std::unexpected(channel.error());
        }
        const std::uint32_t payload_size = read_u32_le(stream, offset + 6);
        if (payload_size > config.max_packet_bytes) {
            return std::unexpected(error("payload_too_large", "Socket packet payload exceeds configured maximum"));
        }
        const std::size_t packet_size = kHeaderBytes + static_cast<std::size_t>(payload_size);
        if (stream.size() - offset < packet_size) {
            break;
        }
        TransportPacket packet{.channel = *channel, .payload = {}};
        packet.payload.insert(packet.payload.end(), stream.begin() + offset + kHeaderBytes,
                              stream.begin() + offset + packet_size);
        packets.push_back(std::move(packet));
        offset += packet_size;
    }
    if (offset > 0) {
        stream.erase(stream.begin(), stream.begin() + static_cast<std::ptrdiff_t>(offset));
    }
    return packets;
}

std::expected<std::unique_ptr<ClientTransport>, TransportError> connect_tcp_client(
    std::string_view endpoint,
    SocketTransportConfig config) {
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    auto address = parse_socket_address(endpoint);
    if (!address) {
        return std::unexpected(address.error());
    }
    auto resolved = resolve_endpoint(*address, false);
    if (!resolved) {
        return std::unexpected(resolved.error());
    }
    SocketHandle socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (!socket.valid()) {
        return std::unexpected(errno_error("socket_failed", "TCP socket creation failed"));
    }
    configure_sigpipe_policy(socket.get());
    if (::connect(socket.get(), reinterpret_cast<sockaddr*>(&*resolved), sizeof(sockaddr_in)) != 0) {
        return std::unexpected(errno_error("connect_failed", "TCP connect failed"));
    }
    if (auto nonblocking = set_nonblocking(socket.get(), config.nonblocking); !nonblocking) {
        return std::unexpected(nonblocking.error());
    }
    return std::unique_ptr<ClientTransport>(
        std::make_unique<TcpClientTransport>(std::move(socket), config));
#else
    (void)endpoint;
    (void)config;
    return std::unexpected(error("unsupported_platform", "TCP socket transport requires POSIX sockets"));
#endif
}

std::expected<TcpServerTransportHandle, TransportError> listen_tcp_server_once_with_bound_address(
    std::string_view bind_endpoint,
    SocketTransportConfig config) {
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    auto address = parse_socket_address(bind_endpoint);
    if (!address) {
        return std::unexpected(address.error());
    }
    auto resolved = resolve_endpoint(*address, true);
    if (!resolved) {
        return std::unexpected(resolved.error());
    }
    const int attempts = address->port == 0 ? 8 : 1;
    TransportError last_error = error("listen_failed", "TCP listen failed");
    for (int attempt = 0; attempt < attempts; ++attempt) {
        SocketHandle socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (!socket.valid()) {
            return std::unexpected(errno_error("socket_failed", "TCP socket creation failed"));
        }
        configure_sigpipe_policy(socket.get());
        const int reuse = 1;
        (void)::setsockopt(socket.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        if (::bind(socket.get(), reinterpret_cast<sockaddr*>(&*resolved), sizeof(sockaddr_in)) !=
            0) {
            last_error = errno_error("bind_failed", "TCP bind failed");
            continue;
        }
        if (::listen(socket.get(), 1) != 0) {
            last_error = errno_error("listen_failed", "TCP listen failed");
            continue;
        }
        if (auto nonblocking = set_nonblocking(socket.get(), config.nonblocking); !nonblocking) {
            return std::unexpected(nonblocking.error());
        }
        const SocketAddress bound = bound_address_for(socket.get(), address->host);
        TcpServerTransportHandle handle{
            .server = std::make_unique<TcpServerTransport>(std::move(socket), config),
            .bound_address = bound,
        };
        return handle;
    }
    return std::unexpected(std::move(last_error));
#else
    (void)bind_endpoint;
    (void)config;
    return std::unexpected(error("unsupported_platform", "TCP socket transport requires POSIX sockets"));
#endif
}

std::expected<std::unique_ptr<ServerTransport>, TransportError> listen_tcp_server_once(
    std::string_view bind_endpoint,
    SocketTransportConfig config) {
    auto handle = listen_tcp_server_once_with_bound_address(bind_endpoint, config);
    if (!handle) {
        return std::unexpected(handle.error());
    }
    return std::move(handle->server);
}

} // namespace stellar::network
