#pragma once

#include <expected>
#include <memory>

#include "stellar/authority/AuthorityBootstrap.hpp"
#include "stellar/client/ApplicationConfig.hpp"
#include "stellar/client/LocalLoopbackRuntime.hpp"
#include "stellar/client/NetworkedClientRuntime.hpp"
#include "stellar/client/SinglePlayerRuntime.hpp"
#include "stellar/platform/Window.hpp"

namespace stellar::client {

/**
 * @brief Top-level client application runner.
 *
 * Owns the client loop, coordinates platform input/windowing, and delegates
 * rendering work to the graphics subsystem through an abstract interface.
 */
class Application {
public:
    Application() noexcept = default;

    /**
     * @brief Construct the application with startup configuration.
     */
    explicit Application(ApplicationConfig config) noexcept;

    /**
     * @brief Run the client application.
     * @return std::expected<void, stellar::platform::Error> on failure.
     */
    [[nodiscard]] std::expected<void, stellar::platform::Error> run();

private:
    ApplicationConfig config_;
};

/**
 * @brief Display-free prepared runtime state for client application startup.
 *
 * Owns the validated startup data plus any runtime world and local loopback
 * runtime assembled from a configured BSP map. Heap ownership keeps the
 * LevelAsset, RuntimeWorld, and WorldSession reference chain stable if this
 * aggregate is moved by callers or std::expected.
 */
struct PreparedApplicationRuntime {
    /** @brief Validated startup data, including an optional loaded BSP level. */
    std::unique_ptr<ApplicationValidation> validation;

    /** @brief Runtime world built once from validation->level when a map exists. */
    std::unique_ptr<stellar::world::RuntimeWorld> runtime_world;

    /** @brief Optional authoritative in-process runtime for mapped local play. */
    std::unique_ptr<LocalLoopbackRuntime> local_loopback_runtime;

    /** @brief Optional transport-backed authoritative runtime for mapped local play. */
    std::unique_ptr<NetworkedClientRuntime> networked_runtime;

    /** @brief Optional true in-process single-player runtime for mapped local play. */
    std::unique_ptr<SinglePlayerRuntime> single_player_runtime;

    /** @brief Optional socket-backed presentation-only runtime for remote server play. */
    std::unique_ptr<RemoteClientRuntime> remote_runtime;

    /** @brief Active non-authoritative client runtime consumed by the application loop. */
    IClientRuntime* active_client_runtime = nullptr;

    /** @brief Optional authority bootstrap storage that backs scripted local runtime references. */
    std::unique_ptr<stellar::authority::PreparedAuthority> authority;
};

/**
 * @brief Validate startup inputs and prepare display-free runtime state.
 */
[[nodiscard]] std::expected<PreparedApplicationRuntime, stellar::platform::Error>
prepare_application_runtime(const ApplicationConfig& config);

} // namespace stellar::client
