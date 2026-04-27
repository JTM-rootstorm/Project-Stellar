#include "stellar/graphics/GraphicsBackend.hpp"
#include "stellar/graphics/GraphicsDeviceFactory.hpp"
#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

#include <cassert>
#include <memory>
#include <string>

int main() {
    const auto opengl = stellar::graphics::parse_graphics_backend("opengl");
    assert(opengl.has_value());
    assert(*opengl == stellar::graphics::GraphicsBackend::kOpenGL);
    assert(stellar::graphics::graphics_backend_name(*opengl) == "opengl");

    const auto gl_alias = stellar::graphics::parse_graphics_backend("gl");
    assert(gl_alias.has_value());
    assert(*gl_alias == stellar::graphics::GraphicsBackend::kOpenGL);

    const auto vulkan = stellar::graphics::parse_graphics_backend("vulkan");
    assert(vulkan.has_value());
    assert(*vulkan == stellar::graphics::GraphicsBackend::kVulkan);
    assert(stellar::graphics::graphics_backend_name(*vulkan) == "vulkan");

    const auto vk_alias = stellar::graphics::parse_graphics_backend("vk");
    assert(vk_alias.has_value());
    assert(*vk_alias == stellar::graphics::GraphicsBackend::kVulkan);

    const auto invalid = stellar::graphics::parse_graphics_backend("software");
    assert(!invalid.has_value());
    assert(invalid.error().message.find("Unsupported graphics backend") != std::string::npos);

    auto default_device = stellar::graphics::create_graphics_device();
    assert(default_device != nullptr);

    auto selected_vulkan_device = stellar::graphics::create_graphics_device(
        stellar::graphics::GraphicsBackend::kVulkan);
    assert(selected_vulkan_device != nullptr);
    assert(dynamic_cast<stellar::graphics::vulkan::VulkanGraphicsDevice*>(
               selected_vulkan_device.get()) != nullptr);

    return 0;
}
