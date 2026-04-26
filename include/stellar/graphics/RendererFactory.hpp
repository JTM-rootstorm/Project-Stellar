#pragma once

#include <memory>

#include "stellar/graphics/Renderer.hpp"

namespace stellar::graphics {

/**
 * @brief Create the default renderer for the current build.
 *
 * The current prototype selects the OpenGL backend, but callers only receive
 * the backend-neutral interface.
 */
[[nodiscard]] std::unique_ptr<Renderer> create_renderer();

} // namespace stellar::graphics
