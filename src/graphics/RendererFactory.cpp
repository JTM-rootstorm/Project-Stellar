#include "stellar/graphics/RendererFactory.hpp"

#include <memory>
#include <optional>
#include <utility>

#include "stellar/graphics/LevelRenderer.hpp"

namespace stellar::graphics {

std::unique_ptr<Renderer>
create_renderer(std::optional<stellar::assets::LevelAsset> level) {
  return create_renderer(default_graphics_backend(), std::move(level));
}

std::unique_ptr<Renderer>
create_renderer(GraphicsBackend backend,
                std::optional<stellar::assets::LevelAsset> level) {
  return std::make_unique<LevelRenderer>(backend, std::move(level));
}

} // namespace stellar::graphics
