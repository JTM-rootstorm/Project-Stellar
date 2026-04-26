#pragma once

#include <expected>
#include <optional>
#include <string>

#include "stellar/assets/SceneAsset.hpp"
#include "stellar/platform/Error.hpp"

namespace stellar::client {

/**
 * @brief Configuration supplied to the client application at startup.
 */
struct ApplicationConfig {
    /** @brief Optional glTF asset path to load instead of the debug cube fallback. */
    std::optional<std::string> asset_path;

    /** @brief Validate startup inputs and return before creating a window or graphics context. */
    bool validate_only = false;
};

/**
 * @brief Backend-neutral result of validating client startup configuration.
 */
struct ApplicationValidation {
    /** @brief Optional CPU-side scene loaded from the configured asset path. */
    std::optional<stellar::assets::SceneAsset> scene;
};

/**
 * @brief Validate client startup configuration without creating display or graphics resources.
 */
[[nodiscard]] std::expected<ApplicationValidation, stellar::platform::Error>
validate_application_config(const ApplicationConfig& config);

} // namespace stellar::client
