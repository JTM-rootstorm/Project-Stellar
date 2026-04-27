#include "stellar/graphics/GraphicsDeviceFactory.hpp"

#include <memory>

#include "stellar/graphics/opengl/OpenGLGraphicsDevice.hpp"
#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

namespace stellar::graphics {

std::unique_ptr<GraphicsDevice> create_graphics_device() {
    return create_graphics_device(GraphicsBackend::kOpenGL);
}

std::unique_ptr<GraphicsDevice> create_graphics_device(GraphicsBackend backend) {
    switch (backend) {
        case GraphicsBackend::kVulkan:
            return std::make_unique<vulkan::VulkanGraphicsDevice>();
        case GraphicsBackend::kOpenGL:
        default:
            return std::make_unique<opengl::OpenGLGraphicsDevice>();
    }
}

} // namespace stellar::graphics
