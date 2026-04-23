#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "stellar/graphics/Shader.hpp"
#include "stellar/graphics/Texture.hpp"

namespace stellar::graphics {

/**
 * @brief Simple 2D quad renderer for drawing text.
 *
 * Uses a procedurally-generated 8x8 monospace font atlas.
 * Each character is rendered as a textured quad.
 */
class Renderer {
public:
    Renderer();
    ~Renderer();

    // Non-copyable
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Movable
    Renderer(Renderer&& other) noexcept;
    Renderer& operator=(Renderer&& other) noexcept;

    /**
     * @brief Initialize the renderer (shaders, font atlas, geometry).
     * @return void on success, Error on failure.
     */
    std::expected<void, Error> initialize();

    /**
     * @brief Render a string of text using the built-in font atlas.
     * @param text The string to render.
     * @param x Screen-space X coordinate (left).
     * @param y Screen-space Y coordinate (top).
     * @param scale Size multiplier for each glyph.
     * @param color Packed RGBA color (0xRRGGBBAA).
     */
    void render_text(const std::string& text, float x, float y,
                     float scale, uint32_t color);

private:
    std::expected<void, Error> build_font_atlas();

    std::unique_ptr<ShaderProgram> text_shader_;
    std::unique_ptr<Texture2D> font_atlas_;

    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;

    // Orthographic projection matrix for 800x600
    float projection_[16] = {};
};

} // namespace stellar::graphics
