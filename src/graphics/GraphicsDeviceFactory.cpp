#include "stellar/graphics/GraphicsDeviceFactory.hpp"

#include <memory>

#if defined(STELLAR_ENABLE_METAL_BACKEND)
#include "stellar/graphics/metal/MetalGraphicsDevice.hpp"
#endif
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"
#endif

namespace stellar::graphics {

std::unique_ptr<GraphicsDevice> create_graphics_device() {
    return create_graphics_device(default_graphics_backend());
}

std::unique_ptr<GraphicsDevice> create_graphics_device(GraphicsBackend backend) {
    switch (backend) {
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
        case GraphicsBackend::kVulkan:
            return std::make_unique<vulkan::VulkanGraphicsDevice>();
#endif
#if defined(STELLAR_ENABLE_METAL_BACKEND)
        case GraphicsBackend::kMetal:
            return std::make_unique<metal::MetalGraphicsDevice>();
#endif
        default:
            return nullptr;
    }
}

} // namespace stellar::graphics
