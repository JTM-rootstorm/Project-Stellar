#include "stellar/graphics/GraphicsDevice.hpp"

namespace stellar::graphics {

// Forward declaration; defined in OpenGLDevice.cpp.
std::unique_ptr<GraphicsDevice> make_opengl_device();

std::unique_ptr<GraphicsDevice> create_opengl_device() {
    return make_opengl_device();
}

} // namespace stellar::graphics
