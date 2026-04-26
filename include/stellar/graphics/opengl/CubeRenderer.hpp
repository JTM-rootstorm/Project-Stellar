#pragma once

#include <expected>

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
    void destroy() noexcept;

    SDL_Window* window_ = nullptr;
    SDL_GLContext context_ = nullptr;
    unsigned int shader_program_ = 0;
    int mvp_loc_ = -1;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
};

} // namespace stellar::graphics::opengl
