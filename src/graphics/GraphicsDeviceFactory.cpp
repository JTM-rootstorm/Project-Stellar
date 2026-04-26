#include "stellar/graphics/GraphicsDeviceFactory.hpp"

#include <memory>

#include "stellar/graphics/opengl/OpenGLGraphicsDevice.hpp"

namespace stellar::graphics {

std::unique_ptr<GraphicsDevice> create_graphics_device() {
    return std::make_unique<opengl::OpenGLGraphicsDevice>();
}

} // namespace stellar::graphics
