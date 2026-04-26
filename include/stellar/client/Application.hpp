#pragma once

#include <expected>

#include "stellar/client/ApplicationConfig.hpp"
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

} // namespace stellar::client
