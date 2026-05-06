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
#if defined(STELLAR_ENABLE_METAL_BACKEND)
    /** @brief Apple Metal backend. */
    kMetal,
#endif
};

/**
 * @brief Parse a user-supplied graphics backend name.
 * @param name Backend name such as "opengl" or "gl".
 * @return Parsed backend or an Error describing the unsupported value.
 */
[[nodiscard]] std::expected<GraphicsBackend, stellar::platform::Error>
parse_graphics_backend(std::string_view name);

/**
 * @brief Return the deterministic default backend for the current compiled build.
 */
[[nodiscard]] GraphicsBackend default_graphics_backend() noexcept;

/**
 * @brief Return whether a backend has a compiled device implementation in this build.
 */
[[nodiscard]] bool graphics_backend_available(GraphicsBackend backend) noexcept;

/**
 * @brief Convert a graphics backend to a stable lowercase name.
 */
[[nodiscard]] std::string_view graphics_backend_name(GraphicsBackend backend) noexcept;

} // namespace stellar::graphics
