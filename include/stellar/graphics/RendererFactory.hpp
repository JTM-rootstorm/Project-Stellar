#pragma once

#include <memory>
#include <optional>

#include "stellar/assets/SceneAsset.hpp"
#include "stellar/graphics/Renderer.hpp"

namespace stellar::graphics {

/**
 * @brief Create the default renderer for the current build.
 *
 * The current prototype selects the OpenGL device through RenderScene, but
 * callers only receive the backend-neutral interface. Vulkan static scene
 * parity is intentionally deferred until the Vulkan backend exists.
 */
[[nodiscard]] std::unique_ptr<Renderer>
create_renderer(std::optional<stellar::assets::SceneAsset> scene = std::nullopt);

} // namespace stellar::graphics
