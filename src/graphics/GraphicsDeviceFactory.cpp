#include "stellar/graphics/GraphicsDeviceFactory.hpp"

#include <memory>

#if defined(STELLAR_ENABLE_OPENGL_BACKEND)
#include "stellar/graphics/opengl/OpenGLGraphicsDevice.hpp"
#endif

namespace stellar::graphics {

std::unique_ptr<GraphicsDevice> create_graphics_device() {
    return create_graphics_device(GraphicsBackend::kOpenGL);
}

std::unique_ptr<GraphicsDevice> create_graphics_device(GraphicsBackend backend) {
    switch (backend) {
        case GraphicsBackend::kOpenGL:
#if defined(STELLAR_ENABLE_OPENGL_BACKEND)
            return std::make_unique<opengl::OpenGLGraphicsDevice>();
#else
            return nullptr;
#endif
        default:
            return nullptr;
    }
}

} // namespace stellar::graphics
