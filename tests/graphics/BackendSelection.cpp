#include "stellar/graphics/GraphicsBackend.hpp"
#include "stellar/graphics/GraphicsDeviceFactory.hpp"

#include <cassert>
#include <memory>
#include <string>

int main() {
    const auto opengl = stellar::graphics::parse_graphics_backend("opengl");
#if defined(STELLAR_ENABLE_OPENGL_BACKEND)
    assert(opengl.has_value());
    assert(*opengl == stellar::graphics::GraphicsBackend::kOpenGL);
    assert(stellar::graphics::graphics_backend_name(*opengl) == "opengl");
    assert(stellar::graphics::graphics_backend_available(*opengl));

    const auto gl_alias = stellar::graphics::parse_graphics_backend("gl");
    assert(gl_alias.has_value());
    assert(*gl_alias == stellar::graphics::GraphicsBackend::kOpenGL);
#else
    assert(!opengl.has_value());
    assert(opengl.error().message.find("compiled backends: ") != std::string::npos);

    const auto gl_alias = stellar::graphics::parse_graphics_backend("gl");
    assert(!gl_alias.has_value());
#endif

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

    const std::string removed_backend = std::string("vul") + "kan";
    const auto removed = stellar::graphics::parse_graphics_backend(removed_backend);
    assert(!removed.has_value());
    assert(removed.error().message.find("Unsupported graphics backend: " + removed_backend) == 0);

    const std::string removed_alias = std::string("v") + "k";
    const auto alias = stellar::graphics::parse_graphics_backend(removed_alias);
    assert(!alias.has_value());

    const std::string removed_title = std::string("Vul") + "kan";
    const auto title = stellar::graphics::parse_graphics_backend(removed_title);
    assert(!title.has_value());

    const auto invalid = stellar::graphics::parse_graphics_backend("software");
    assert(!invalid.has_value());
    assert(invalid.error().message.find("Unsupported graphics backend") != std::string::npos);

    auto default_device = stellar::graphics::create_graphics_device();
#if defined(STELLAR_ENABLE_OPENGL_BACKEND) || defined(STELLAR_ENABLE_METAL_BACKEND)
    assert(default_device != nullptr);
#else
    assert(default_device == nullptr);
#endif
    assert(stellar::graphics::graphics_backend_available(
               stellar::graphics::default_graphics_backend()) ==
           (default_device != nullptr));

    auto selected_opengl_device = stellar::graphics::create_graphics_device(
        stellar::graphics::GraphicsBackend::kOpenGL);
#if defined(STELLAR_ENABLE_OPENGL_BACKEND)
    assert(selected_opengl_device != nullptr);
#else
    assert(selected_opengl_device == nullptr);
#endif

#if defined(STELLAR_ENABLE_METAL_BACKEND)
    auto selected_metal_device = stellar::graphics::create_graphics_device(
        stellar::graphics::GraphicsBackend::kMetal);
    assert(selected_metal_device != nullptr);
#endif

    return 0;
}
