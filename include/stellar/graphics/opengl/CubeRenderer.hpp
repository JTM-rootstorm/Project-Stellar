#pragma once

#include <array>
#include <expected>

#include "stellar/assets/MeshAsset.hpp"
#include "stellar/graphics/RenderScene.hpp"
#include "stellar/graphics/Renderer.hpp"

namespace stellar::graphics::opengl {

/**
 * @brief OpenGL renderer for the sample rotating cube.
 *
 * Encapsulates shader creation, buffer setup, and frame submission for the
 * current prototype scene.
 */
class CubeRenderer final : public stellar::graphics::Renderer {
public:
    CubeRenderer() noexcept = default;
    ~CubeRenderer() noexcept;

    CubeRenderer(const CubeRenderer&) = delete;
    CubeRenderer& operator=(const CubeRenderer&) = delete;

    /**
     * @brief Initialize OpenGL resources and bind to the window.
     * @param window Platform window used to create the OpenGL context.
     * @return std::expected<void, stellar::platform::Error> on failure.
     */
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& window) override;

    /**
     * @brief Render the cube for the current frame.
     * @param rotation_degrees Rotation angle in degrees.
     * @param width Viewport width in pixels.
     * @param height Viewport height in pixels.
     */
    void render(float rotation_degrees, int width, int height) noexcept override;

private:
    [[nodiscard]] static std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
    create_cube_mesh();

    [[nodiscard]] static stellar::assets::SceneAsset create_cube_scene();

    stellar::graphics::RenderScene scene_;
};

} // namespace stellar::graphics::opengl
