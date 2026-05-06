#include "stellar/graphics/GraphicsBackend.hpp"

#include <string>

namespace stellar::graphics {

std::expected<GraphicsBackend, stellar::platform::Error>
parse_graphics_backend(std::string_view name) {
    if (name == "opengl" || name == "gl" || name == "OpenGL") {
        return GraphicsBackend::kOpenGL;
    }
#if defined(STELLAR_ENABLE_METAL_BACKEND)
    if (name == "metal" || name == "mtl") {
        return GraphicsBackend::kMetal;
    }
#else
    if (name == "metal" || name == "mtl") {
        std::string message = "Unsupported graphics backend: ";
        message.append(name.data(), name.size());
        message += " (Metal backend not built)";
        return std::unexpected(stellar::platform::Error(std::move(message)));
    }
#endif

    std::string message = "Unsupported graphics backend: ";
    message.append(name.data(), name.size());
    message += " (expected opengl";
#if defined(STELLAR_ENABLE_METAL_BACKEND)
    message += " or metal";
#endif
    message += ")";
    return std::unexpected(stellar::platform::Error(std::move(message)));
}

std::string_view graphics_backend_name(GraphicsBackend backend) noexcept {
    switch (backend) {
        case GraphicsBackend::kOpenGL:
            return "opengl";
#if defined(STELLAR_ENABLE_METAL_BACKEND)
        case GraphicsBackend::kMetal:
            return "metal";
#endif
        default:
            return "unknown";
    }
}

} // namespace stellar::graphics
