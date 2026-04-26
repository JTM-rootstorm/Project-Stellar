#include "stellar/graphics/RendererFactory.hpp"

#include <memory>
#include <optional>
#include <utility>

#include "stellar/graphics/SceneRenderer.hpp"

namespace stellar::graphics {

std::unique_ptr<Renderer> create_renderer(std::optional<stellar::assets::SceneAsset> scene) {
    return std::make_unique<SceneRenderer>(std::move(scene));
}

} // namespace stellar::graphics
