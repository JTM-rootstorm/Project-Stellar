#pragma once

#include <expected>
#include <cstdint>

#include "stellar/graphics/GraphicsDevice.hpp"

namespace stellar::graphics {

/**
 * @brief RAII wrapper for an OpenGL 2D texture.
 */
class Texture2D {
public:
    Texture2D() = default;

    /**
     * @brief Construct from an existing OpenGL texture ID.
     * @param id OpenGL texture object name.
     */
    explicit Texture2D(unsigned int id);

    /**
     * @brief Destructor deletes the OpenGL texture object.
     */
    ~Texture2D();

    // Non-copyable
    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    // Movable
    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;

    /**
     * @brief Bind the texture to a texture unit.
     * @param slot Texture unit index (default 0).
     */
    void bind(unsigned int slot = 0) const;

    /**
     * @brief Get the raw OpenGL texture ID.
     * @return OpenGL texture object ID.
     */
    unsigned int id() const { return texture_id_; }

private:
    unsigned int texture_id_ = 0;
};

/**
 * @brief Create a 2D texture from raw RGBA pixel data.
 * @param width Texture width in pixels.
 * @param height Texture height in pixels.
 * @param data Pointer to RGBA8 pixel data (width * height * 4 bytes).
 * @return Texture2D on success, Error on failure.
 */
std::expected<Texture2D, Error> create_texture_from_data(
    int width, int height, const uint8_t* data);

} // namespace stellar::graphics
