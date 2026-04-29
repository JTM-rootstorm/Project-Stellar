#include "stellar/graphics/RendererFactory.hpp"

#include <memory>
#include <optional>
#include <utility>

#include "stellar/graphics/SceneRenderer.hpp"

namespace stellar::graphics {

std::unique_ptr<Renderer> create_renderer(std::optional<stellar::assets::SceneAsset> scene) {
    return create_renderer(GraphicsBackend::kOpenGL, std::move(scene), {});
}

std::unique_ptr<Renderer> create_renderer(GraphicsBackend backend,
                                          std::optional<stellar::assets::SceneAsset> scene,
                                          SceneRendererAnimationOptions animation_options) {
    return std::make_unique<SceneRenderer>(backend, std::move(scene), std::move(animation_options));
}

} // namespace stellar::graphics
