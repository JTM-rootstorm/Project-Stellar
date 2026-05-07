#include "stellar/graphics/GraphicsBackend.hpp"
#include "stellar/graphics/GraphicsDeviceFactory.hpp"

#include <cassert>
#include <memory>
#include <string>

int main() {
    const auto vulkan = stellar::graphics::parse_graphics_backend("vulkan");
#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
    assert(vulkan.has_value());
    assert(*vulkan == stellar::graphics::GraphicsBackend::kVulkan);
    assert(stellar::graphics::graphics_backend_name(*vulkan) == "vulkan");
    assert(stellar::graphics::graphics_backend_available(*vulkan));

    const auto vk_alias = stellar::graphics::parse_graphics_backend("vk");
    assert(vk_alias.has_value());
    assert(*vk_alias == stellar::graphics::GraphicsBackend::kVulkan);

    const auto vulkan_title = stellar::graphics::parse_graphics_backend("Vulkan");
    assert(vulkan_title.has_value());
    assert(*vulkan_title == stellar::graphics::GraphicsBackend::kVulkan);
#else
    assert(!vulkan.has_value());
    assert(vulkan.error().message.find("Unsupported graphics backend: vulkan") == 0);

    const auto vk_alias = stellar::graphics::parse_graphics_backend("vk");
    assert(!vk_alias.has_value());

    const auto vulkan_title = stellar::graphics::parse_graphics_backend("Vulkan");
    assert(!vulkan_title.has_value());
#endif

    const auto opengl = stellar::graphics::parse_graphics_backend("opengl");
    assert(!opengl.has_value());
    assert(opengl.error().message.find("compiled backends: ") != std::string::npos);

    const auto gl_alias = stellar::graphics::parse_graphics_backend("gl");
    assert(!gl_alias.has_value());

    const auto metal = stellar::graphics::parse_graphics_backend("metal");
#if defined(STELLAR_ENABLE_METAL_BACKEND)
    assert(metal.has_value());
    assert(*metal == stellar::graphics::GraphicsBackend::kMetal);
    assert(stellar::graphics::graphics_backend_name(*metal) == "metal");
    assert(stellar::graphics::graphics_backend_available(*metal));

    const auto metal_alias = stellar::graphics::parse_graphics_backend("mtl");
    assert(metal_alias.has_value());
    assert(*metal_alias == stellar::graphics::GraphicsBackend::kMetal);
#else
    assert(!metal.has_value());
    assert(metal.error().message.find("compiled backends: ") != std::string::npos);
#endif

    const auto invalid = stellar::graphics::parse_graphics_backend("software");
    assert(!invalid.has_value());
    assert(invalid.error().message.find("Unsupported graphics backend") != std::string::npos);

    auto default_device = stellar::graphics::create_graphics_device();
#if defined(STELLAR_ENABLE_VULKAN_BACKEND) || defined(STELLAR_ENABLE_METAL_BACKEND)
    assert(default_device != nullptr);
#else
    assert(default_device == nullptr);
#endif
    assert(stellar::graphics::graphics_backend_available(
               stellar::graphics::default_graphics_backend()) ==
           (default_device != nullptr));

#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
    assert(stellar::graphics::default_graphics_backend() ==
           stellar::graphics::GraphicsBackend::kVulkan);

    auto selected_vulkan_device = stellar::graphics::create_graphics_device(
        stellar::graphics::GraphicsBackend::kVulkan);
    assert(selected_vulkan_device != nullptr);
#endif

#if defined(STELLAR_ENABLE_METAL_BACKEND)
    auto selected_metal_device = stellar::graphics::create_graphics_device(
        stellar::graphics::GraphicsBackend::kMetal);
    assert(selected_metal_device != nullptr);
#endif

    return 0;
}
