#include "stellar/graphics/Shader.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include <array>
#include <string>

namespace stellar::graphics {

ShaderProgram::ShaderProgram(unsigned int id) : program_id_(id) {}

ShaderProgram::~ShaderProgram() {
    if (program_id_ != 0) {
        glDeleteProgram(program_id_);
    }
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : program_id_(other.program_id_) {
    other.program_id_ = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        if (program_id_ != 0) {
            glDeleteProgram(program_id_);
        }
        program_id_ = other.program_id_;
        other.program_id_ = 0;
    }
    return *this;
}

void ShaderProgram::use() const {
    glUseProgram(program_id_);
}

void ShaderProgram::set_uniform_mat4(const std::string& name,
                                     const float* value) const {
    int loc = glGetUniformLocation(program_id_, name.c_str());
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, value);
    }
}

void ShaderProgram::set_uniform_int(const std::string& name,
                                    int value) const {
    int loc = glGetUniformLocation(program_id_, name.c_str());
    if (loc >= 0) {
        glUniform1i(loc, value);
    }
}

void ShaderProgram::set_uniform_vec4(const std::string& name, float r,
                                     float g, float b, float a) const {
    int loc = glGetUniformLocation(program_id_, name.c_str());
    if (loc >= 0) {
        glUniform4f(loc, r, g, b, a);
    }
}

std::expected<ShaderProgram, Error> create_shader_from_source(
    const std::string& vertex_source,
    const std::string& fragment_source) {

    auto compile = [](unsigned int type,
                      const std::string& source)
        -> std::expected<unsigned int, Error> {
        unsigned int shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        int success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            std::array<char, 512> log{};
            glGetShaderInfoLog(shader, static_cast<int>(log.size()),
                               nullptr, log.data());
            glDeleteShader(shader);
            return std::unexpected<Error>(
                Error{-3, std::string("Shader compile error: ") + log.data()});
        }
        return shader;
    };

    auto vs = compile(GL_VERTEX_SHADER, vertex_source);
    if (!vs) {
        return std::unexpected<Error>(vs.error());
    }

    auto fs = compile(GL_FRAGMENT_SHADER, fragment_source);
    if (!fs) {
        glDeleteShader(vs.value());
        return std::unexpected<Error>(fs.error());
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs.value());
    glAttachShader(program, fs.value());
    glLinkProgram(program);

    glDeleteShader(vs.value());
    glDeleteShader(fs.value());

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        std::array<char, 512> log{};
        glGetProgramInfoLog(program, static_cast<int>(log.size()),
                            nullptr, log.data());
        glDeleteProgram(program);
        return std::unexpected<Error>(
            Error{-4, std::string("Program link error: ") + log.data()});
    }

    return ShaderProgram(program);
}

} // namespace stellar::graphics
