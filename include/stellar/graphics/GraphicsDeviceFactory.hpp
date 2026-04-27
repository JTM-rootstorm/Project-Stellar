#pragma once

#include <memory>

#include "stellar/graphics/GraphicsBackend.hpp"
#include "stellar/graphics/GraphicsDevice.hpp"

namespace stellar::graphics {

/**
 * @brief Create the OpenGL graphics device used as the default backend.
 */
[[nodiscard]] std::unique_ptr<GraphicsDevice> create_graphics_device();

/**
 * @brief Create a graphics device for the requested runtime backend.
 * @param backend Backend selected by configuration or CLI.
 * @return Backend-neutral graphics device instance, or nullptr if unavailable in this build.
 */
[[nodiscard]] std::unique_ptr<GraphicsDevice> create_graphics_device(GraphicsBackend backend);

} // namespace stellar::graphics
