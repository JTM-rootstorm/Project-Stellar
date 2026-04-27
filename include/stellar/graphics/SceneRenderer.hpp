#pragma once

#include <array>
#include <expected>
#include <optional>

#include "stellar/assets/MeshAsset.hpp"
#include "stellar/assets/SceneAsset.hpp"
#include "stellar/graphics/GraphicsBackend.hpp"
#include "stellar/graphics/RenderScene.hpp"
#include "stellar/graphics/Renderer.hpp"

namespace stellar::graphics {

/**
 * @brief Generic static scene renderer with a debug cube fallback.
 */
class SceneRenderer final : public Renderer {
public:
    /**
     * @brief Construct a renderer from an optional CPU-side scene asset using OpenGL.
     */
    explicit SceneRenderer(std::optional<stellar::assets::SceneAsset> scene = std::nullopt) noexcept;

    /**
     * @brief Construct a renderer for a specific runtime backend.
     */
    explicit SceneRenderer(GraphicsBackend backend,
                           std::optional<stellar::assets::SceneAsset> scene = std::nullopt) noexcept;

    /**
     * @brief Release renderer-owned scene resources.
     */
    ~SceneRenderer() noexcept;

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    /**
     * @brief Initialize GPU resources for the provided scene or fallback cube.
     */
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& window) override;

    /**
     * @brief Render the loaded scene, rotating only the debug cube fallback.
     */
    void render(float rotation_degrees, int width, int height) noexcept override;

private:
    [[nodiscard]] static std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
    create_cube_mesh();

    [[nodiscard]] static stellar::assets::SceneAsset create_cube_scene();

    GraphicsBackend backend_ = GraphicsBackend::kOpenGL;
    std::optional<stellar::assets::SceneAsset> source_scene_;
    RenderScene scene_;
    bool using_debug_cube_ = false;
};

} // namespace stellar::graphics
