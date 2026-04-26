#pragma once

#include <memory>

#include "stellar/graphics/GraphicsDevice.hpp"

namespace stellar::graphics {

/**
 * @brief Create the default graphics device for the current build.
 */
[[nodiscard]] std::unique_ptr<GraphicsDevice> create_graphics_device();

} // namespace stellar::graphics
