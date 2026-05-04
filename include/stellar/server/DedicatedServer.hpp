#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>

#include "stellar/network/Session.hpp"
#include "stellar/platform/Error.hpp"

namespace stellar::server {

/** @brief Runtime configuration for the single-client dedicated server entry point. */
struct DedicatedServerConfig {
    /** @brief Canonical BSP map path loaded by the authoritative server. */
    std::string map_path;

    /** @brief Optional root used for sandboxed authoritative Lua script source lookup. */
    std::optional<std::string> script_root;

    /** @brief Host:port endpoint for the TCP server listener. */
    std::string listen_endpoint = "127.0.0.1:29070";

    /** @brief Fixed authoritative simulation tick rate in hertz. */
    int tick_rate = 60;

    /** @brief Authoritative snapshot send rate in hertz. */
    int snapshot_rate = 20;

    /** @brief Maximum accepted clients. ST-5 supports exactly one active client. */
    int max_clients = 1;

    /** @brief Validate map/script configuration and skip socket listener creation. */
    bool validate_only = false;
};

/** @brief Observable dedicated server runtime status for tests and CLI diagnostics. */
struct DedicatedServerStatus {
    /** @brief True once the server configuration and authoritative runtime are initialized. */
    bool running = false;

    /** @brief Stable server map compatibility identifier. */
    std::string map_id;

    /** @brief Number of completed authoritative simulation ticks. */
    std::uint64_t tick = 0;

    /** @brief Count of currently accepted client sessions, zero or one for ST-5. */
    std::uint32_t connected_clients = 0;

    /** @brief Concrete host:port endpoint bound by the socket listener. */
    std::string bound_endpoint;
};

/** @brief Result of one bounded dedicated server pump. */
struct DedicatedServerPumpResult {
    /** @brief Number of authoritative fixed ticks run by this pump. */
    int ticks_run = 0;

    /** @brief Number of malformed, premature, rejected, or failed packets. */
    std::uint32_t rejected_packets = 0;

    /** @brief Current single-client session lifecycle state. */
    stellar::network::SessionState session_state = stellar::network::SessionState::kDisconnected;
};

/**
 * @brief Headless authoritative dedicated server runtime over BSP maps and socket transport.
 *
 * The server owns map loading, script loading, player assignment, authoritative ticks, snapshots,
 * deltas, and gameplay event emission. ST-5 intentionally supports one active client only.
 */
class DedicatedServer {
public:
    /** @brief Parse CLI arguments into a deterministic dedicated server configuration. */
    [[nodiscard]] static std::expected<DedicatedServerConfig, stellar::platform::Error>
    parse_cli(int argc, const char* const argv[]);

    /** @brief Create and validate an authoritative dedicated server runtime. */
    [[nodiscard]] static std::expected<DedicatedServer, stellar::platform::Error> create(
        DedicatedServerConfig config);

    /** @brief Copying is disabled because the server owns authoritative socket/runtime state. */
    DedicatedServer(const DedicatedServer&) = delete;
    /** @brief Copy assignment is disabled because authoritative runtime state is single-owned. */
    DedicatedServer& operator=(const DedicatedServer&) = delete;
    /** @brief Move a dedicated server runtime and its socket/session ownership. */
    DedicatedServer(DedicatedServer&& other) noexcept;
    /** @brief Move-assign a dedicated server runtime and its socket/session ownership. */
    DedicatedServer& operator=(DedicatedServer&& other) noexcept;
    /** @brief Stop and release the owned authoritative server runtime implementation. */
    ~DedicatedServer();

    /** @brief Run the server forever with bounded internal pump steps. */
    [[nodiscard]] std::expected<void, stellar::platform::Error> run();

    /** @brief Pump socket IO and authoritative simulation for a bounded testable duration. */
    [[nodiscard]] std::expected<DedicatedServerPumpResult, stellar::platform::Error> pump_once(
        float delta_seconds) noexcept;

    /** @brief Return current server status without advancing simulation. */
    [[nodiscard]] DedicatedServerStatus status() const noexcept;

private:
    class Impl;

    explicit DedicatedServer(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

} // namespace stellar::server
