#include "stellar/graphics/Renderer.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include <algorithm>
#include <vector>

namespace stellar::graphics {

namespace {

constexpr const char* kTextVertexShader = R"(
#version 450 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;

out vec2 v_uv;

uniform mat4 u_projection;

void main() {
    gl_Position = u_projection * vec4(a_pos, 0.0, 1.0);
    v_uv = a_uv;
}
)";

constexpr const char* kTextFragmentShader = R"(
#version 450 core
in vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_texture;
uniform vec4 u_color;

void main() {
    vec4 texel = texture(u_texture, v_uv);
    frag_color = texel * u_color;
}
)";

struct Vertex {
    float x;
    float y;
    float u;
    float v;
};

} // anonymous namespace

Renderer::Renderer() = default;

Renderer::~Renderer() {
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
    }
}

Renderer::Renderer(Renderer&& other) noexcept
    : text_shader_(std::move(other.text_shader_)),
      font_atlas_(std::move(other.font_atlas_)),
      vao_(other.vao_),
      vbo_(other.vbo_) {
    std::copy(std::begin(other.projection_), std::end(other.projection_),
              std::begin(projection_));
    other.vao_ = 0;
    other.vbo_ = 0;
}

Renderer& Renderer::operator=(Renderer&& other) noexcept {
    if (this != &other) {
        if (vbo_ != 0) {
            glDeleteBuffers(1, &vbo_);
        }
        if (vao_ != 0) {
            glDeleteVertexArrays(1, &vao_);
        }

        text_shader_ = std::move(other.text_shader_);
        font_atlas_ = std::move(other.font_atlas_);
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        std::copy(std::begin(other.projection_), std::end(other.projection_),
                  std::begin(projection_));

        other.vao_ = 0;
        other.vbo_ = 0;
    }
    return *this;
}

std::expected<void, Error> Renderer::initialize() {
    auto shader_res = create_shader_from_source(kTextVertexShader,
                                                kTextFragmentShader);
    if (!shader_res) {
        return std::unexpected<Error>(shader_res.error());
    }
    text_shader_ = std::make_unique<ShaderProgram>(
        std::move(shader_res.value()));

    auto atlas_res = build_font_atlas();
    if (!atlas_res) {
        return std::unexpected<Error>(atlas_res.error());
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, u)));

    glBindVertexArray(0);

    // Orthographic projection: left=0, right=800, bottom=600, top=0.
    constexpr float l = 0.0f;
    constexpr float r = 800.0f;
    constexpr float b = 600.0f;
    constexpr float t = 0.0f;

    projection_[0]  = 2.0f / (r - l);
    projection_[1]  = 0.0f;
    projection_[2]  = 0.0f;
    projection_[3]  = 0.0f;

    projection_[4]  = 0.0f;
    projection_[5]  = 2.0f / (t - b);
    projection_[6]  = 0.0f;
    projection_[7]  = 0.0f;

    projection_[8]  = 0.0f;
    projection_[9]  = 0.0f;
    projection_[10] = 1.0f;
    projection_[11] = 0.0f;

    projection_[12] = -(r + l) / (r - l);
    projection_[13] = -(t + b) / (t - b);
    projection_[14] = 0.0f;
    projection_[15] = 1.0f;

    return {};
}

std::expected<void, Error> Renderer::build_font_atlas() {
    constexpr int atlas_w = 128;
    constexpr int atlas_h = 64;
    constexpr int cell_w = 8;
    constexpr int cell_h = 8;
    constexpr int cols = 16;

    std::vector<uint8_t> pixels(
        static_cast<size_t>(atlas_w * atlas_h * 4), 0);

    auto draw_char = [&](char c, const uint8_t bitmap[7]) {
        int idx = static_cast<int>(c);
        int cell_x = (idx % cols) * cell_w;
        int cell_y = (idx / cols) * cell_h;

        for (int row = 0; row < 7; ++row) {
            uint8_t bits = bitmap[row];
            for (int col = 0; col < 5; ++col) {
                if ((bits >> (4 - col)) & 1) {
                    int px = cell_x + 1 + col;
                    int py = cell_y + row;
                    int offset = (py * atlas_w + px) * 4;
                    pixels[static_cast<size_t>(offset) + 0] = 255;
                    pixels[static_cast<size_t>(offset) + 1] = 255;
                    pixels[static_cast<size_t>(offset) + 2] = 255;
                    pixels[static_cast<size_t>(offset) + 3] = 255;
                }
            }
        }
    };

    const uint8_t w_bm[7] = {0x00, 0x15, 0x15, 0x15, 0x15, 0x1F, 0x00};
    const uint8_t o_bm[7] = {0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00};
    const uint8_t r_bm[7] = {0x00, 0x0E, 0x09, 0x08, 0x08, 0x08, 0x00};
    const uint8_t k_bm[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    const uint8_t i_bm[7] = {0x04, 0x00, 0x04, 0x04, 0x04, 0x04, 0x00};
    const uint8_t n_bm[7] = {0x00, 0x0E, 0x09, 0x09, 0x09, 0x09, 0x00};
    const uint8_t g_bm[7] = {0x00, 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x0E};

    draw_char('w', w_bm);
    draw_char('o', o_bm);
    draw_char('r', r_bm);
    draw_char('k', k_bm);
    draw_char('i', i_bm);
    draw_char('n', n_bm);
    draw_char('g', g_bm);

    auto tex = create_texture_from_data(atlas_w, atlas_h, pixels.data());
    if (!tex) {
        return std::unexpected<Error>(tex.error());
    }
    font_atlas_ = std::make_unique<Texture2D>(std::move(tex.value()));
    return {};
}

void Renderer::render_text(const std::string& text, float x, float y,
                           float scale, uint32_t color) {
    if (!text_shader_ || !font_atlas_ || vao_ == 0) {
        return;
    }

    float r = ((color >> 24) & 0xFF) / 255.0f;
    float g = ((color >> 16) & 0xFF) / 255.0f;
    float b = ((color >> 8) & 0xFF) / 255.0f;
    float a = ((color >> 0) & 0xFF) / 255.0f;

    text_shader_->use();
    text_shader_->set_uniform_mat4("u_projection", projection_);
    text_shader_->set_uniform_vec4("u_color", r, g, b, a);

    font_atlas_->bind(0);
    text_shader_->set_uniform_int("u_texture", 0);

    constexpr float cell_size = 8.0f;
    float quad_w = cell_size * scale;
    float quad_h = cell_size * scale;

    std::vector<Vertex> vertices;
    vertices.reserve(text.size() * 6);

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        int idx = static_cast<int>(c);
        int cell_x = idx % 16;
        int cell_y = idx / 16;

        float u0 = static_cast<float>(cell_x) / 16.0f;
        float u1 = static_cast<float>(cell_x + 1) / 16.0f;
        float v0 = 1.0f - static_cast<float>(cell_y + 1) / 8.0f;
        float v1 = 1.0f - static_cast<float>(cell_y) / 8.0f;

        float qx = x + static_cast<float>(i) * quad_w;
        float qy = y;

        // Triangle 1
        vertices.push_back({qx, qy, u0, v1});          // top-left
        vertices.push_back({qx + quad_w, qy, u1, v1}); // top-right
        vertices.push_back({qx, qy + quad_h, u0, v0}); // bottom-left

        // Triangle 2
        vertices.push_back({qx, qy + quad_h, u0, v0});          // bottom-left
        vertices.push_back({qx + quad_w, qy, u1, v1});          // top-right
        vertices.push_back({qx + quad_w, qy + quad_h, u1, v0}); // bottom-right
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_STREAM_DRAW);

    glDrawArrays(GL_TRIANGLES, 0,
                 static_cast<int>(vertices.size()));

    glBindVertexArray(0);
}

} // namespace stellar::graphics
