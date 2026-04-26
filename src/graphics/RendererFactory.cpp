#include "stellar/graphics/RendererFactory.hpp"

#include <memory>

#include "stellar/graphics/opengl/CubeRenderer.hpp"

namespace stellar::graphics {

std::unique_ptr<Renderer> create_renderer() {
    return std::make_unique<opengl::CubeRenderer>();
}

} // namespace stellar::graphics
