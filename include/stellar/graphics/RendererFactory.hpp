#pragma once

#include <memory>
#include <optional>

#include "stellar/assets/SceneAsset.hpp"
#include "stellar/graphics/GraphicsBackend.hpp"
#include "stellar/graphics/Renderer.hpp"

namespace stellar::graphics {

/**
 * @brief Create a renderer using the OpenGL default backend.
 */
[[nodiscard]] std::unique_ptr<Renderer>
create_renderer(std::optional<stellar::assets::SceneAsset> scene = std::nullopt);

/**
 * @brief Create a renderer using a runtime-selected backend.
 *
 * The renderer remains backend-neutral. OpenGL is fully render-capable; Vulkan currently
 * initializes and stores uploads but frame/draw/present calls are no-ops until the full Vulkan
 * swapchain and pipeline are implemented.
 */
[[nodiscard]] std::unique_ptr<Renderer>
create_renderer(GraphicsBackend backend,
                std::optional<stellar::assets::SceneAsset> scene = std::nullopt);

} // namespace stellar::graphics
