#include "stellar/graphics/GraphicsBackend.hpp"

#include <string>

namespace stellar::graphics {

std::expected<GraphicsBackend, stellar::platform::Error>
parse_graphics_backend(std::string_view name) {
    if (name == "opengl" || name == "gl" || name == "OpenGL") {
        return GraphicsBackend::kOpenGL;
    }
    if (name == "vulkan" || name == "vk" || name == "Vulkan") {
        return GraphicsBackend::kVulkan;
    }

    std::string message = "Unsupported graphics backend: ";
    message.append(name.data(), name.size());
    message += " (expected opengl or vulkan)";
    return std::unexpected(stellar::platform::Error(std::move(message)));
}

std::string_view graphics_backend_name(GraphicsBackend backend) noexcept {
    switch (backend) {
        case GraphicsBackend::kVulkan:
            return "vulkan";
        case GraphicsBackend::kOpenGL:
        default:
            return "opengl";
    }
}

} // namespace stellar::graphics
