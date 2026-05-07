#include "stellar/graphics/GraphicsBackend.hpp"

#include <string>

namespace stellar::graphics {
namespace {

[[nodiscard]] std::string compiled_backend_list() {
    std::string names;
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
    names += "vulkan";
#endif
#if defined(STELLAR_ENABLE_OPENGL_BACKEND)
    if (!names.empty()) {
        names += ", ";
    }
    names += "opengl";
#endif
#if defined(STELLAR_ENABLE_METAL_BACKEND)
    if (!names.empty()) {
        names += ", ";
    }
    names += "metal";
#endif
    if (names.empty()) {
        names = "none";
    }
    return names;
}

[[nodiscard]] stellar::platform::Error unsupported_backend_error(std::string_view name) {
    std::string message = "Unsupported graphics backend: ";
    message.append(name.data(), name.size());
    message += " (compiled backends: ";
    message += compiled_backend_list();
    message += "; suggested backend: ";
    if (graphics_backend_available(default_graphics_backend())) {
        message += graphics_backend_name(default_graphics_backend());
    } else {
        message += "none";
    }
    message += ")";
    return stellar::platform::Error(std::move(message));
}

} // namespace

std::expected<GraphicsBackend, stellar::platform::Error>
parse_graphics_backend(std::string_view name) {
    if (name == "vulkan" || name == "vk" || name == "Vulkan") {
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
        return GraphicsBackend::kVulkan;
#else
        return std::unexpected(unsupported_backend_error(name));
#endif
    }
    if (name == "opengl" || name == "gl" || name == "OpenGL") {
#if defined(STELLAR_ENABLE_OPENGL_BACKEND)
        return GraphicsBackend::kOpenGL;
#else
        return std::unexpected(unsupported_backend_error(name));
#endif
    }
#if defined(STELLAR_ENABLE_METAL_BACKEND)
    if (name == "metal" || name == "mtl") {
        return GraphicsBackend::kMetal;
    }
#else
    if (name == "metal" || name == "mtl") {
        return std::unexpected(unsupported_backend_error(name));
    }
#endif

    return std::unexpected(unsupported_backend_error(name));
}

GraphicsBackend default_graphics_backend() noexcept {
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
    return GraphicsBackend::kVulkan;
#elif defined(STELLAR_ENABLE_OPENGL_BACKEND)
    return GraphicsBackend::kOpenGL;
#elif defined(STELLAR_ENABLE_METAL_BACKEND)
    return GraphicsBackend::kMetal;
#else
    return GraphicsBackend::kOpenGL;
#endif
}

bool graphics_backend_available(GraphicsBackend backend) noexcept {
    switch (backend) {
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
        case GraphicsBackend::kVulkan:
            return true;
#endif
        case GraphicsBackend::kOpenGL:
#if defined(STELLAR_ENABLE_OPENGL_BACKEND)
            return true;
#else
            return false;
#endif
#if defined(STELLAR_ENABLE_METAL_BACKEND)
        case GraphicsBackend::kMetal:
            return true;
#endif
        default:
            return false;
    }
}

std::string_view graphics_backend_name(GraphicsBackend backend) noexcept {
    switch (backend) {
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
        case GraphicsBackend::kVulkan:
            return "vulkan";
#endif
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
