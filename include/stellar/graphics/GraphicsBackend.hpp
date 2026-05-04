#pragma once

#include <expected>
#include <string_view>

#include "stellar/graphics/GraphicsDevice.hpp"

namespace stellar::graphics {

/**
 * @brief Runtime-selectable graphics backend.
 *
 * Currently only OpenGL is implemented. Future native backends such as
 * DirectX or Metal should be added here only when their implementations exist.
 */
enum class GraphicsBackend {
    /** @brief OpenGL backend. */
    kOpenGL,
};

/**
 * @brief Parse a user-supplied graphics backend name.
 * @param name Backend name such as "opengl" or "gl".
 * @return Parsed backend or an Error describing the unsupported value.
 */
[[nodiscard]] std::expected<GraphicsBackend, stellar::platform::Error>
parse_graphics_backend(std::string_view name);

/**
 * @brief Convert a graphics backend to a stable lowercase name.
 */
[[nodiscard]] std::string_view graphics_backend_name(GraphicsBackend backend) noexcept;

} // namespace stellar::graphics
