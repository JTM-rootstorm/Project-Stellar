#include "stellar/graphics/opengl/OpenGLGraphicsDevice.hpp"

#include <cstddef>

#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>

namespace stellar::graphics::opengl {

namespace {

constexpr const char* kVertexShader = R"(
#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv0;
layout(location = 3) in vec4 a_tangent;

uniform mat4 u_mvp;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)";

constexpr const char* kFragmentShader = R"(
#version 330 core

uniform vec4 u_base_color;

out vec4 frag_color;

void main() {
    frag_color = u_base_color;
}
)";

GLuint compile_shader_impl(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

GLuint create_shader_program_impl(const char* vertex_src, const char* fragment_src) {
    GLuint vs = compile_shader_impl(GL_VERTEX_SHADER, vertex_src);
    GLuint fs = compile_shader_impl(GL_FRAGMENT_SHADER, fragment_src);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

constexpr GLenum texture_format_from_image(stellar::assets::ImageFormat format,
                                           GLint& internal_format,
                                           GLenum& external_format) {
    switch (format) {
        case stellar::assets::ImageFormat::kR8G8B8:
            internal_format = GL_RGB8;
            external_format = GL_RGB;
            return GL_RGB;
        case stellar::assets::ImageFormat::kR8G8B8A8:
            internal_format = GL_RGBA8;
            external_format = GL_RGBA;
            return GL_RGBA;
        case stellar::assets::ImageFormat::kUnknown:
        default:
            internal_format = GL_NONE;
            external_format = GL_NONE;
            return GL_NONE;
    }
}

} // namespace

OpenGLGraphicsDevice::~OpenGLGraphicsDevice() noexcept {
    if (shader_program_ != 0) {
        glDeleteProgram(shader_program_);
        shader_program_ = 0;
        mvp_loc_ = -1;
    }

    for (auto& [handle, record] : meshes_) {
        (void)handle;
        destroy_mesh_record(record);
    }
    meshes_.clear();

    for (auto& [handle, record] : textures_) {
        (void)handle;
        destroy_texture_record(record);
    }
    textures_.clear();

    materials_.clear();

    if (context_) {
        SDL_GL_DeleteContext(context_);
        context_ = nullptr;
    }

    window_ = nullptr;
}

std::expected<void, stellar::platform::Error>
OpenGLGraphicsDevice::initialize(stellar::platform::Window& window) {
    window_ = window.native_handle();
    if (!window_) {
        return std::unexpected(stellar::platform::Error("Window is not initialized"));
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    context_ = SDL_GL_CreateContext(window_);
    if (!context_) {
        return std::unexpected(stellar::platform::Error("Failed to create OpenGL context"));
    }

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        SDL_GL_DeleteContext(context_);
        context_ = nullptr;
        return std::unexpected(stellar::platform::Error("Failed to initialize GLEW"));
    }

    shader_program_ = create_shader_program_impl(kVertexShader, kFragmentShader);
    glUseProgram(shader_program_);
    mvp_loc_ = glGetUniformLocation(shader_program_, "u_mvp");
    glUseProgram(0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    return {};
}

std::expected<MeshHandle, stellar::platform::Error>
OpenGLGraphicsDevice::create_mesh(const stellar::assets::MeshAsset& mesh) {
    if (!context_) {
        return std::unexpected(stellar::platform::Error("Graphics device is not initialized"));
    }

    if (mesh.primitives.empty()) {
        return std::unexpected(stellar::platform::Error("Mesh has no primitives"));
    }

    MeshRecord record;
    record.primitives.reserve(mesh.primitives.size());

    for (const auto& primitive : mesh.primitives) {
        if (primitive.topology != stellar::assets::PrimitiveTopology::kTriangles) {
            return std::unexpected(stellar::platform::Error("Unsupported mesh topology"));
        }

        if (primitive.vertices.empty() || primitive.indices.empty()) {
            return std::unexpected(stellar::platform::Error("Mesh primitive is empty"));
        }

        MeshPrimitiveGpu gpu_primitive;
        glGenVertexArrays(1, &gpu_primitive.vao);
        glGenBuffers(1, &gpu_primitive.vbo);
        glGenBuffers(1, &gpu_primitive.ebo);

        glBindVertexArray(gpu_primitive.vao);

        glBindBuffer(GL_ARRAY_BUFFER, gpu_primitive.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(primitive.vertices.size() *
                                             sizeof(stellar::assets::StaticVertex)),
                     primitive.vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu_primitive.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(primitive.indices.size() *
                                             sizeof(std::uint32_t)),
                     primitive.indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              sizeof(stellar::assets::StaticVertex),
                              reinterpret_cast<void*>(offsetof(stellar::assets::StaticVertex,
                                                               position)));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                              sizeof(stellar::assets::StaticVertex),
                              reinterpret_cast<void*>(offsetof(stellar::assets::StaticVertex,
                                                               normal)));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                              sizeof(stellar::assets::StaticVertex),
                              reinterpret_cast<void*>(offsetof(stellar::assets::StaticVertex,
                                                               uv0)));
        glEnableVertexAttribArray(2);

        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE,
                              sizeof(stellar::assets::StaticVertex),
                              reinterpret_cast<void*>(offsetof(stellar::assets::StaticVertex,
                                                               tangent)));
        glEnableVertexAttribArray(3);

        glBindVertexArray(0);

        gpu_primitive.index_count = static_cast<int>(primitive.indices.size());
        record.primitives.push_back(gpu_primitive);
    }

    const std::uint64_t handle_value = allocate_handle();
    meshes_.emplace(handle_value, std::move(record));
    return MeshHandle{handle_value};
}

std::expected<TextureHandle, stellar::platform::Error>
OpenGLGraphicsDevice::create_texture(const stellar::assets::ImageAsset& image) {
    if (!context_) {
        return std::unexpected(stellar::platform::Error("Graphics device is not initialized"));
    }

    if (image.width == 0 || image.height == 0 || image.pixels.empty()) {
        return std::unexpected(stellar::platform::Error("Image asset is empty"));
    }

    GLint internal_format = GL_NONE;
    GLenum external_format = GL_NONE;
    if (texture_format_from_image(image.format, internal_format, external_format) == GL_NONE) {
        return std::unexpected(stellar::platform::Error("Unsupported image format"));
    }

    TextureRecord record;
    glGenTextures(1, &record.texture);
    glBindTexture(GL_TEXTURE_2D, record.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, static_cast<GLsizei>(image.width),
                 static_cast<GLsizei>(image.height), 0, external_format, GL_UNSIGNED_BYTE,
                 image.pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    record.width = static_cast<int>(image.width);
    record.height = static_cast<int>(image.height);

    const std::uint64_t handle_value = allocate_handle();
    textures_.emplace(handle_value, std::move(record));
    return TextureHandle{handle_value};
}

std::expected<MaterialHandle, stellar::platform::Error>
OpenGLGraphicsDevice::create_material(const stellar::assets::MaterialAsset& material) {
    if (!context_) {
        return std::unexpected(stellar::platform::Error("Graphics device is not initialized"));
    }

    const std::uint64_t handle_value = allocate_handle();
    materials_.emplace(handle_value, MaterialRecord{material});
    return MaterialHandle{handle_value};
}

void OpenGLGraphicsDevice::begin_frame(int width, int height) noexcept {
    if (!context_) {
        return;
    }

    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLGraphicsDevice::draw_mesh(MeshHandle mesh,
                                     std::span<const MaterialHandle> materials,
                                     const std::array<float, 16>& mvp) noexcept {
    if (!context_ || shader_program_ == 0 || mvp_loc_ < 0) {
        return;
    }

    auto it = meshes_.find(mesh.value);
    if (it == meshes_.end()) {
        return;
    }

    glUseProgram(shader_program_);
    glUniformMatrix4fv(mvp_loc_, 1, GL_FALSE, mvp.data());

    for (std::size_t primitive_index = 0; primitive_index < it->second.primitives.size();
         ++primitive_index) {
        const auto& primitive = it->second.primitives[primitive_index];
        const MaterialHandle material_handle = primitive_index < materials.size()
                                                   ? materials[primitive_index]
                                                   : MaterialHandle{};

        const auto material_it = materials_.find(material_handle.value);
        const std::array<float, 4> base_color =
            material_it != materials_.end()
                ? material_it->second.material.base_color_factor
                : std::array<float, 4>{0.85f, 0.9f, 1.0f, 1.0f};

        const GLint base_color_loc = glGetUniformLocation(shader_program_, "u_base_color");
        glUniform4fv(base_color_loc, 1, base_color.data());
        glBindVertexArray(primitive.vao);
        glDrawElements(GL_TRIANGLES, primitive.index_count, GL_UNSIGNED_INT, nullptr);
    }

    glBindVertexArray(0);
}

void OpenGLGraphicsDevice::end_frame() noexcept {
    if (window_) {
        SDL_GL_SwapWindow(window_);
    }
}

void OpenGLGraphicsDevice::destroy_mesh(MeshHandle mesh) noexcept {
    auto it = meshes_.find(mesh.value);
    if (it == meshes_.end()) {
        return;
    }

    destroy_mesh_record(it->second);
    meshes_.erase(it);
}

void OpenGLGraphicsDevice::destroy_texture(TextureHandle texture) noexcept {
    auto it = textures_.find(texture.value);
    if (it == textures_.end()) {
        return;
    }

    destroy_texture_record(it->second);
    textures_.erase(it);
}

void OpenGLGraphicsDevice::destroy_material(MaterialHandle material) noexcept {
    materials_.erase(material.value);
}

std::uint64_t OpenGLGraphicsDevice::allocate_handle() noexcept {
    return next_handle_++;
}

void OpenGLGraphicsDevice::destroy_mesh_record(MeshRecord& record) noexcept {
    for (auto& primitive : record.primitives) {
        if (primitive.ebo != 0) {
            glDeleteBuffers(1, &primitive.ebo);
            primitive.ebo = 0;
        }

        if (primitive.vbo != 0) {
            glDeleteBuffers(1, &primitive.vbo);
            primitive.vbo = 0;
        }

        if (primitive.vao != 0) {
            glDeleteVertexArrays(1, &primitive.vao);
            primitive.vao = 0;
        }
    }

    record.primitives.clear();
}

void OpenGLGraphicsDevice::destroy_texture_record(TextureRecord& record) noexcept {
    if (record.texture != 0) {
        glDeleteTextures(1, &record.texture);
        record.texture = 0;
    }
}

} // namespace stellar::graphics::opengl
