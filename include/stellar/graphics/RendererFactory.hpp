#pragma once

#include <memory>
#include <optional>

#include "stellar/assets/LevelAsset.hpp"
#include "stellar/graphics/GraphicsBackend.hpp"
#include "stellar/graphics/LevelRenderer.hpp"
#include "stellar/graphics/Renderer.hpp"

namespace stellar::graphics {

/**
 * @brief Create a renderer using the OpenGL default backend.
 */
[[nodiscard]] std::unique_ptr<Renderer> create_renderer(
    std::optional<stellar::assets::LevelAsset> level = std::nullopt);

/**
 * @brief Create a renderer using a runtime-selected backend.
 *
 * The renderer remains backend-neutral. OpenGL is the current implemented
 * backend; future native backends should be added only with real device
 * implementations.
 */
[[nodiscard]] std::unique_ptr<Renderer> create_renderer(
    GraphicsBackend backend,
    std::optional<stellar::assets::LevelAsset> level = std::nullopt);

} // namespace stellar::graphics
