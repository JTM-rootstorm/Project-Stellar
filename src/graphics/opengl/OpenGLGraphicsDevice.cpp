#include "stellar/graphics/opengl/OpenGLGraphicsDevice.hpp"

#include <cstddef>
#include <expected>
#include <string>

#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>

namespace stellar::graphics::opengl {

namespace {

constexpr int kMaxSkinJoints = 96;

constexpr const char* kVertexShader = R"(
#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv0;
layout(location = 3) in vec4 a_tangent;
layout(location = 4) in vec2 a_uv1;
layout(location = 5) in vec4 a_color;
layout(location = 6) in uvec4 a_joints0;
layout(location = 7) in vec4 a_weights0;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform mat3 u_normal_matrix;
uniform bool u_has_skinning;
uniform mat4 u_joint_matrices[96];

out vec2 v_uv0;
out vec2 v_uv1;
out vec3 v_normal;
out vec4 v_tangent;
out vec4 v_color;

void main() {
    mat4 skin = mat4(1.0);
    if (u_has_skinning) {
        skin = a_weights0.x * u_joint_matrices[a_joints0.x] +
            a_weights0.y * u_joint_matrices[a_joints0.y] +
            a_weights0.z * u_joint_matrices[a_joints0.z] +
            a_weights0.w * u_joint_matrices[a_joints0.w];
    }
    vec4 local_position = skin * vec4(a_position, 1.0);
    mat3 skinned_normal_matrix = mat3(skin);
    v_uv0 = a_uv0;
    v_uv1 = a_uv1;
    v_normal = normalize(u_normal_matrix * skinned_normal_matrix * a_normal);
    v_tangent = vec4(mat3(u_model) * skinned_normal_matrix * a_tangent.xyz, a_tangent.w);
    v_color = a_color;
    gl_Position = u_mvp * local_position;
}
)";

constexpr const char* kFragmentShader = R"(
#version 330 core

uniform vec4 u_base_color;
uniform sampler2D u_base_color_texture;
uniform sampler2D u_normal_texture;
uniform sampler2D u_metallic_roughness_texture;
uniform sampler2D u_occlusion_texture;
uniform sampler2D u_emissive_texture;
uniform bool u_has_base_color_texture;
uniform bool u_has_normal_texture;
uniform bool u_has_metallic_roughness_texture;
uniform bool u_has_occlusion_texture;
uniform bool u_has_emissive_texture;
uniform bool u_has_vertex_color;
uniform int u_base_color_texcoord_set;
uniform int u_normal_texcoord_set;
uniform int u_metallic_roughness_texcoord_set;
uniform int u_occlusion_texcoord_set;
uniform int u_emissive_texcoord_set;
uniform float u_metallic_factor;
uniform float u_roughness_factor;
uniform float u_normal_scale;
uniform float u_occlusion_strength;
uniform vec3 u_emissive_factor;
uniform int u_alpha_mode;
uniform float u_alpha_cutoff;

in vec2 v_uv0;
in vec2 v_uv1;
in vec3 v_normal;
in vec4 v_tangent;
in vec4 v_color;

out vec4 frag_color;

vec2 uv_for_set(int texcoord_set) {
    return texcoord_set == 1 ? v_uv1 : v_uv0;
}

void main() {
    vec4 color = u_base_color;
    if (u_has_vertex_color) {
        color *= v_color;
    }
    if (u_has_base_color_texture) {
        color *= texture(u_base_color_texture, uv_for_set(u_base_color_texcoord_set));
    }
    if (u_alpha_mode == 1 && color.a < u_alpha_cutoff) {
        discard;
    }

    float metallic = u_metallic_factor;
    float roughness = u_roughness_factor;
    if (u_has_metallic_roughness_texture) {
        vec4 metallic_roughness = texture(u_metallic_roughness_texture,
            uv_for_set(u_metallic_roughness_texcoord_set));
        roughness *= metallic_roughness.g;
        metallic *= metallic_roughness.b;
    }

    vec3 normal = normalize(v_normal);
    if (u_has_normal_texture) {
        vec3 tangent = normalize(v_tangent.xyz - normal * dot(v_tangent.xyz, normal));
        vec3 bitangent = normalize(cross(normal, tangent) * v_tangent.w);
        mat3 tbn = mat3(tangent, bitangent, normal);
        vec3 sampled_normal = texture(u_normal_texture, uv_for_set(u_normal_texcoord_set)).xyz *
            2.0 - 1.0;
        sampled_normal.xy *= u_normal_scale;
        normal = normalize(tbn * sampled_normal);
    }

    vec3 light_dir = normalize(vec3(0.35, 0.8, 0.45));
    float diffuse = max(dot(normal, light_dir), 0.0);
    float perceptual_roughness = clamp(roughness, 0.04, 1.0);
    float metal_attenuation = mix(1.0, 0.45, clamp(metallic, 0.0, 1.0));
    float lit = 0.18 + diffuse * metal_attenuation *
        mix(1.0, 0.65, perceptual_roughness);
    float occlusion = 1.0;
    if (u_has_occlusion_texture) {
        occlusion = mix(1.0, texture(u_occlusion_texture, uv_for_set(u_occlusion_texcoord_set)).r,
            u_occlusion_strength);
    }
    vec3 emissive = u_emissive_factor;
    if (u_has_emissive_texture) {
        emissive *= texture(u_emissive_texture, uv_for_set(u_emissive_texcoord_set)).rgb;
    }
    frag_color = vec4(color.rgb * lit * occlusion + emissive, color.a);
}
)";

std::string shader_info_log(GLuint shader) {
    GLint log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length <= 1) {
        return {};
    }

    std::string log(static_cast<std::size_t>(log_length), '\0');
    GLsizei written = 0;
    glGetShaderInfoLog(shader, log_length, &written, log.data());
    log.resize(static_cast<std::size_t>(written));
    return log;
}

std::string program_info_log(GLuint program) {
    GLint log_length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length <= 1) {
        return {};
    }

    std::string log(static_cast<std::size_t>(log_length), '\0');
    GLsizei written = 0;
    glGetProgramInfoLog(program, log_length, &written, log.data());
    log.resize(static_cast<std::size_t>(written));
    return log;
}

std::expected<GLuint, stellar::platform::Error>
compile_shader_impl(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compile_status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status != GL_TRUE) {
        std::string message = "Failed to compile OpenGL shader";
        const std::string log = shader_info_log(shader);
        if (!log.empty()) {
            message += ": ";
            message += log;
        }
        glDeleteShader(shader);
        return std::unexpected(stellar::platform::Error(message));
    }

    return shader;
}

std::expected<GLuint, stellar::platform::Error>
create_shader_program_impl(const char* vertex_src, const char* fragment_src) {
    auto vs = compile_shader_impl(GL_VERTEX_SHADER, vertex_src);
    if (!vs) {
        return std::unexpected(vs.error());
    }

    auto fs = compile_shader_impl(GL_FRAGMENT_SHADER, fragment_src);
    if (!fs) {
        glDeleteShader(*vs);
        return std::unexpected(fs.error());
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, *vs);
    glAttachShader(program, *fs);
    glLinkProgram(program);

    glDeleteShader(*vs);
    glDeleteShader(*fs);

    GLint link_status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    if (link_status != GL_TRUE) {
        std::string message = "Failed to link OpenGL shader program";
        const std::string log = program_info_log(program);
        if (!log.empty()) {
            message += ": ";
            message += log;
        }
        glDeleteProgram(program);
        return std::unexpected(stellar::platform::Error(message));
    }

    return program;
}

constexpr GLenum texture_format_from_image(stellar::assets::ImageFormat format,
                                            stellar::assets::TextureColorSpace color_space,
                                            GLint& internal_format,
                                            GLenum& external_format) {
    switch (format) {
        case stellar::assets::ImageFormat::kR8G8B8:
            internal_format = color_space == stellar::assets::TextureColorSpace::kSrgb ? GL_SRGB8
                                                                                       : GL_RGB8;
            external_format = GL_RGB;
            return GL_RGB;
        case stellar::assets::ImageFormat::kR8G8B8A8:
            internal_format = color_space == stellar::assets::TextureColorSpace::kSrgb ? GL_SRGB8_ALPHA8
                                                                                       : GL_RGBA8;
            external_format = GL_RGBA;
            return GL_RGBA;
        case stellar::assets::ImageFormat::kUnknown:
        default:
            internal_format = GL_NONE;
            external_format = GL_NONE;
            return GL_NONE;
    }
}

GLint to_gl_filter(stellar::assets::TextureFilter filter, GLint fallback) noexcept {
    switch (filter) {
        case stellar::assets::TextureFilter::kNearest:
            return GL_NEAREST;
        case stellar::assets::TextureFilter::kLinear:
            return GL_LINEAR;
        case stellar::assets::TextureFilter::kNearestMipmapNearest:
            return GL_NEAREST_MIPMAP_NEAREST;
        case stellar::assets::TextureFilter::kLinearMipmapNearest:
            return GL_LINEAR_MIPMAP_NEAREST;
        case stellar::assets::TextureFilter::kNearestMipmapLinear:
            return GL_NEAREST_MIPMAP_LINEAR;
        case stellar::assets::TextureFilter::kLinearMipmapLinear:
            return GL_LINEAR_MIPMAP_LINEAR;
        case stellar::assets::TextureFilter::kUnspecified:
        default:
            return fallback;
    }
}

GLint to_gl_wrap(stellar::assets::TextureWrapMode mode) noexcept {
    switch (mode) {
        case stellar::assets::TextureWrapMode::kClampToEdge:
            return GL_CLAMP_TO_EDGE;
        case stellar::assets::TextureWrapMode::kMirroredRepeat:
            return GL_MIRRORED_REPEAT;
        case stellar::assets::TextureWrapMode::kRepeat:
        default:
            return GL_REPEAT;
    }
}

int to_alpha_mode(stellar::assets::AlphaMode mode) noexcept {
    switch (mode) {
        case stellar::assets::AlphaMode::kMask:
            return 1;
        case stellar::assets::AlphaMode::kBlend:
            return 2;
        case stellar::assets::AlphaMode::kOpaque:
        default:
            return 0;
    }
}

void apply_sampler_state(const stellar::assets::SamplerAsset& sampler) noexcept {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    to_gl_filter(sampler.min_filter, GL_LINEAR_MIPMAP_LINEAR));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    to_gl_filter(sampler.mag_filter, GL_LINEAR));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, to_gl_wrap(sampler.wrap_s));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, to_gl_wrap(sampler.wrap_t));
}

} // namespace

OpenGLGraphicsDevice::~OpenGLGraphicsDevice() noexcept {
    if (shader_program_ != 0) {
        glDeleteProgram(shader_program_);
        shader_program_ = 0;
        mvp_loc_ = -1;
        model_loc_ = -1;
        normal_matrix_loc_ = -1;
        has_skinning_loc_ = -1;
        joint_matrices_loc_ = -1;
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

    auto shader_program = create_shader_program_impl(kVertexShader, kFragmentShader);
    if (!shader_program) {
        SDL_GL_DeleteContext(context_);
        context_ = nullptr;
        return std::unexpected(shader_program.error());
    }

    shader_program_ = *shader_program;
    glUseProgram(shader_program_);
    mvp_loc_ = glGetUniformLocation(shader_program_, "u_mvp");
    model_loc_ = glGetUniformLocation(shader_program_, "u_model");
    normal_matrix_loc_ = glGetUniformLocation(shader_program_, "u_normal_matrix");
    has_skinning_loc_ = glGetUniformLocation(shader_program_, "u_has_skinning");
    joint_matrices_loc_ = glGetUniformLocation(shader_program_, "u_joint_matrices[0]");
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

        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE,
                              sizeof(stellar::assets::StaticVertex),
                              reinterpret_cast<void*>(offsetof(stellar::assets::StaticVertex,
                                                               uv1)));
        glEnableVertexAttribArray(4);

        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE,
                              sizeof(stellar::assets::StaticVertex),
                              reinterpret_cast<void*>(offsetof(stellar::assets::StaticVertex,
                                                                color)));
        glEnableVertexAttribArray(5);

        glVertexAttribIPointer(6, 4, GL_UNSIGNED_SHORT,
                               sizeof(stellar::assets::StaticVertex),
                               reinterpret_cast<void*>(offsetof(stellar::assets::StaticVertex,
                                                                joints0)));
        glEnableVertexAttribArray(6);

        glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE,
                              sizeof(stellar::assets::StaticVertex),
                              reinterpret_cast<void*>(offsetof(stellar::assets::StaticVertex,
                                                               weights0)));
        glEnableVertexAttribArray(7);

        glBindVertexArray(0);

        gpu_primitive.index_count = static_cast<int>(primitive.indices.size());
        gpu_primitive.has_tangents = primitive.has_tangents;
        gpu_primitive.has_colors = primitive.has_colors;
        gpu_primitive.has_skinning = primitive.has_skinning;
        record.primitives.push_back(gpu_primitive);
    }

    const std::uint64_t handle_value = allocate_handle();
    meshes_.emplace(handle_value, std::move(record));
    return MeshHandle{handle_value};
}

std::expected<TextureHandle, stellar::platform::Error>
OpenGLGraphicsDevice::create_texture(const TextureUpload& texture) {
    if (!context_) {
        return std::unexpected(stellar::platform::Error("Graphics device is not initialized"));
    }

    const auto& image = texture.image;
    if (image.width == 0 || image.height == 0 || image.pixels.empty()) {
        return std::unexpected(stellar::platform::Error("Image asset is empty"));
    }

    GLint internal_format = GL_NONE;
    GLenum external_format = GL_NONE;
    if (texture_format_from_image(image.format, texture.color_space, internal_format,
                                  external_format) == GL_NONE) {
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
OpenGLGraphicsDevice::create_material(const MaterialUpload& material) {
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
                                      std::span<const MeshPrimitiveDrawCommand> commands,
                                      const MeshDrawTransforms& transforms) noexcept {
    if (!context_ || shader_program_ == 0 || mvp_loc_ < 0 || model_loc_ < 0 ||
        normal_matrix_loc_ < 0) {
        return;
    }

    auto it = meshes_.find(mesh.value);
    if (it == meshes_.end()) {
        return;
    }

    glUseProgram(shader_program_);
    glUniformMatrix4fv(mvp_loc_, 1, GL_FALSE, transforms.mvp.data());
    glUniformMatrix4fv(model_loc_, 1, GL_FALSE, transforms.world.data());
    glUniformMatrix3fv(normal_matrix_loc_, 1, GL_FALSE, transforms.normal.data());

    auto bind_texture = [this](const std::optional<MaterialTextureBinding>& binding,
                               GLenum texture_unit) noexcept -> bool {
        if (!binding.has_value() || binding->texcoord_set > 1) {
            return false;
        }

        const auto texture_it = textures_.find(binding->texture.value);
        if (texture_it == textures_.end()) {
            return false;
        }

        glActiveTexture(texture_unit);
        glBindTexture(GL_TEXTURE_2D, texture_it->second.texture);
        apply_sampler_state(binding->sampler);
        return true;
    };

    for (const MeshPrimitiveDrawCommand& command : commands) {
        const std::size_t primitive_index = command.primitive_index;
        if (primitive_index >= it->second.primitives.size()) {
            continue;
        }
        const auto& primitive = it->second.primitives[primitive_index];
        const MaterialHandle material_handle = command.material;
        const std::size_t joint_count = command.skin_joint_matrices.size();
        const bool use_skinning = primitive.has_skinning && !command.skin_joint_matrices.empty() &&
            joint_count <= static_cast<std::size_t>(kMaxSkinJoints);
        if (has_skinning_loc_ >= 0) {
            glUniform1i(has_skinning_loc_, use_skinning ? 1 : 0);
        }
        if (use_skinning && joint_matrices_loc_ >= 0 && joint_count > 0) {
            glUniformMatrix4fv(joint_matrices_loc_, static_cast<GLsizei>(joint_count), GL_FALSE,
                               command.skin_joint_matrices.data()->data());
        }

        const auto material_it = materials_.find(material_handle.value);
        const MaterialRecord* material = material_it != materials_.end() ? &material_it->second
                                                                         : nullptr;
        const std::array<float, 4> fallback_color{0.85f, 0.9f, 1.0f, 1.0f};
        const std::array<float, 4> base_color = material != nullptr
                                                     ? material->upload.material.base_color_factor
                                                     : fallback_color;

        const auto alpha_mode = material != nullptr ? material->upload.material.alpha_mode
                                                    : stellar::assets::AlphaMode::kOpaque;
        if (alpha_mode == stellar::assets::AlphaMode::kBlend) {
            // RenderScene orders transparent primitives; direct device submissions should already
            // be preordered by the caller.
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }

        if (material != nullptr && material->upload.material.double_sided) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        }

        bool has_base_color_texture = false;
        if (material != nullptr && material->upload.base_color_texture.has_value()) {
            has_base_color_texture = bind_texture(material->upload.base_color_texture,
                                                  GL_TEXTURE0);
        }

        bool has_normal_texture = false;
        if (material != nullptr && primitive.has_tangents) {
            has_normal_texture = bind_texture(material->upload.normal_texture, GL_TEXTURE1);
        }

        bool has_metallic_roughness_texture = false;
        if (material != nullptr) {
            has_metallic_roughness_texture =
                bind_texture(material->upload.metallic_roughness_texture, GL_TEXTURE2);
        }

        bool has_occlusion_texture = false;
        if (material != nullptr) {
            has_occlusion_texture = bind_texture(material->upload.occlusion_texture, GL_TEXTURE3);
        }

        bool has_emissive_texture = false;
        if (material != nullptr) {
            has_emissive_texture = bind_texture(material->upload.emissive_texture, GL_TEXTURE4);
        }

        const GLint base_color_loc = glGetUniformLocation(shader_program_, "u_base_color");
        const GLint texture_loc = glGetUniformLocation(shader_program_, "u_base_color_texture");
        const GLint normal_texture_loc = glGetUniformLocation(shader_program_, "u_normal_texture");
        const GLint mr_texture_loc =
            glGetUniformLocation(shader_program_, "u_metallic_roughness_texture");
        const GLint occlusion_texture_loc = glGetUniformLocation(shader_program_,
                                                                 "u_occlusion_texture");
        const GLint emissive_texture_loc = glGetUniformLocation(shader_program_,
                                                                "u_emissive_texture");
        const GLint has_texture_loc = glGetUniformLocation(shader_program_,
                                                           "u_has_base_color_texture");
        const GLint has_normal_loc = glGetUniformLocation(shader_program_, "u_has_normal_texture");
        const GLint has_mr_loc =
            glGetUniformLocation(shader_program_, "u_has_metallic_roughness_texture");
        const GLint has_occlusion_loc = glGetUniformLocation(shader_program_,
                                                              "u_has_occlusion_texture");
        const GLint has_emissive_loc = glGetUniformLocation(shader_program_,
                                                             "u_has_emissive_texture");
        const GLint has_vertex_color_loc = glGetUniformLocation(shader_program_,
                                                                "u_has_vertex_color");
        const GLint base_color_texcoord_loc = glGetUniformLocation(shader_program_,
                                                                   "u_base_color_texcoord_set");
        const GLint normal_texcoord_loc = glGetUniformLocation(shader_program_,
                                                               "u_normal_texcoord_set");
        const GLint mr_texcoord_loc = glGetUniformLocation(shader_program_,
                                                           "u_metallic_roughness_texcoord_set");
        const GLint occlusion_texcoord_loc = glGetUniformLocation(shader_program_,
                                                                 "u_occlusion_texcoord_set");
        const GLint emissive_texcoord_loc = glGetUniformLocation(shader_program_,
                                                                "u_emissive_texcoord_set");
        const GLint metallic_loc = glGetUniformLocation(shader_program_, "u_metallic_factor");
        const GLint roughness_loc = glGetUniformLocation(shader_program_, "u_roughness_factor");
        const GLint normal_scale_loc = glGetUniformLocation(shader_program_, "u_normal_scale");
        const GLint occlusion_strength_loc = glGetUniformLocation(shader_program_,
                                                                  "u_occlusion_strength");
        const GLint emissive_factor_loc = glGetUniformLocation(shader_program_,
                                                               "u_emissive_factor");
        const GLint alpha_mode_loc = glGetUniformLocation(shader_program_, "u_alpha_mode");
        const GLint alpha_cutoff_loc = glGetUniformLocation(shader_program_, "u_alpha_cutoff");
        glUniform4fv(base_color_loc, 1, base_color.data());
        glUniform1i(texture_loc, 0);
        glUniform1i(normal_texture_loc, 1);
        glUniform1i(mr_texture_loc, 2);
        glUniform1i(occlusion_texture_loc, 3);
        glUniform1i(emissive_texture_loc, 4);
        glUniform1i(has_texture_loc, has_base_color_texture ? 1 : 0);
        glUniform1i(has_normal_loc, has_normal_texture ? 1 : 0);
        glUniform1i(has_mr_loc, has_metallic_roughness_texture ? 1 : 0);
        glUniform1i(has_occlusion_loc, has_occlusion_texture ? 1 : 0);
        glUniform1i(has_emissive_loc, has_emissive_texture ? 1 : 0);
        glUniform1i(has_vertex_color_loc, primitive.has_colors ? 1 : 0);
        glUniform1i(base_color_texcoord_loc,
                    has_base_color_texture && material->upload.base_color_texture.has_value()
                        ? static_cast<int>(material->upload.base_color_texture->texcoord_set)
                        : 0);
        glUniform1i(normal_texcoord_loc,
                    has_normal_texture && material->upload.normal_texture.has_value()
                        ? static_cast<int>(material->upload.normal_texture->texcoord_set)
                        : 0);
        glUniform1i(mr_texcoord_loc,
                    has_metallic_roughness_texture &&
                            material->upload.metallic_roughness_texture.has_value()
                        ? static_cast<int>(
                              material->upload.metallic_roughness_texture->texcoord_set)
                        : 0);
        glUniform1i(occlusion_texcoord_loc,
                    has_occlusion_texture && material->upload.occlusion_texture.has_value()
                        ? static_cast<int>(material->upload.occlusion_texture->texcoord_set)
                        : 0);
        glUniform1i(emissive_texcoord_loc,
                    has_emissive_texture && material->upload.emissive_texture.has_value()
                        ? static_cast<int>(material->upload.emissive_texture->texcoord_set)
                        : 0);
        glUniform1f(metallic_loc,
                    material != nullptr ? material->upload.material.metallic_factor : 0.0f);
        glUniform1f(roughness_loc,
                    material != nullptr ? material->upload.material.roughness_factor : 1.0f);
        glUniform1f(normal_scale_loc,
                    material != nullptr && material->upload.material.normal_texture.has_value()
                        ? material->upload.material.normal_texture->scale
                        : 1.0f);
        glUniform1f(occlusion_strength_loc,
                    material != nullptr ? material->upload.material.occlusion_strength : 1.0f);
        const std::array<float, 3> emissive_factor = material != nullptr
            ? material->upload.material.emissive_factor
            : std::array<float, 3>{0.0f, 0.0f, 0.0f};
        glUniform3fv(emissive_factor_loc, 1, emissive_factor.data());
        glUniform1i(alpha_mode_loc, to_alpha_mode(alpha_mode));
        glUniform1f(alpha_cutoff_loc,
                    material != nullptr ? material->upload.material.alpha_cutoff : 0.5f);
        glBindVertexArray(primitive.vao);
        glDrawElements(GL_TRIANGLES, primitive.index_count, GL_UNSIGNED_INT, nullptr);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
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
