#pragma once

#include <expected>
#include <string>

#include "stellar/graphics/GraphicsDevice.hpp"

namespace stellar::graphics {

/**
 * @brief RAII wrapper for an OpenGL shader program.
 */
class ShaderProgram {
public:
    ShaderProgram() = default;

    /**
     * @brief Construct from an existing OpenGL program ID.
     * @param id OpenGL program object name.
     */
    explicit ShaderProgram(unsigned int id);

    /**
     * @brief Destructor deletes the OpenGL program object.
     */
    ~ShaderProgram();

    // Non-copyable
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    // Movable
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    /**
     * @brief Activate this shader program.
     */
    void use() const;

    /**
     * @brief Set a 4x4 matrix uniform.
     * @param name Uniform variable name.
     * @param value Pointer to 16 floats (column-major).
     */
    void set_uniform_mat4(const std::string& name, const float* value) const;

    /**
     * @brief Set an integer uniform.
     * @param name Uniform variable name.
     * @param value Integer value.
     */
    void set_uniform_int(const std::string& name, int value) const;

    /**
     * @brief Set a 4-component vector uniform.
     * @param name Uniform variable name.
     * @param r Red component.
     * @param g Green component.
     * @param b Blue component.
     * @param a Alpha component.
     */
    void set_uniform_vec4(const std::string& name, float r, float g,
                          float b, float a) const;

    /**
     * @brief Get the raw OpenGL program ID.
     * @return OpenGL program object ID.
     */
    unsigned int id() const { return program_id_; }

private:
    unsigned int program_id_ = 0;
};

/**
 * @brief Compile and link a shader program from source strings.
 * @param vertex_source GLSL vertex shader source.
 * @param fragment_source GLSL fragment shader source.
 * @return ShaderProgram on success, Error on failure.
 */
std::expected<ShaderProgram, Error> create_shader_from_source(
    const std::string& vertex_source,
    const std::string& fragment_source);

} // namespace stellar::graphics
