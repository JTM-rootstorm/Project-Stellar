#pragma once

#include <expected>
#include <optional>
#include <string>
#include <cstddef>

#include "stellar/assets/SceneAsset.hpp"
#include "stellar/graphics/GraphicsBackend.hpp"
#include "stellar/platform/Error.hpp"

namespace stellar::client {

/**
 * @brief Configuration supplied to the client application at startup.
 */
struct ApplicationConfig {
    /** @brief Optional glTF asset path to load instead of the debug cube fallback. */
    std::optional<std::string> asset_path;

    /** @brief Graphics backend selected at startup. OpenGL remains the default. */
    stellar::graphics::GraphicsBackend graphics_backend =
        stellar::graphics::GraphicsBackend::kOpenGL;

    /** @brief Validate startup inputs and return before creating a window or graphics context. */
    bool validate_only = false;

    /** @brief Optional imported animation index to play when a loaded asset has animations. */
    std::optional<std::size_t> animation_index;

    /** @brief Optional imported animation name to play when a loaded asset has animations. */
    std::optional<std::string> animation_name;

    /** @brief Whether selected imported animation playback should loop. */
    bool animation_loop = true;
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
