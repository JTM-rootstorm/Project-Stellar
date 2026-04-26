#pragma once

#include <array>
#include <map>
#include <span>
#include <vector>

#include <expected>

#include "stellar/graphics/GraphicsDevice.hpp"

namespace stellar::graphics::opengl {

/**
 * @brief OpenGL backend for CPU-side resource uploads.
 */
class OpenGLGraphicsDevice final : public stellar::graphics::GraphicsDevice {
public:
    OpenGLGraphicsDevice() noexcept = default;
    ~OpenGLGraphicsDevice() noexcept override;

    OpenGLGraphicsDevice(const OpenGLGraphicsDevice&) = delete;
    OpenGLGraphicsDevice& operator=(const OpenGLGraphicsDevice&) = delete;

    [[nodiscard]] std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& window) override;

    [[nodiscard]] std::expected<MeshHandle, stellar::platform::Error>
    create_mesh(const stellar::assets::MeshAsset& mesh) override;

    [[nodiscard]] std::expected<TextureHandle, stellar::platform::Error>
    create_texture(const stellar::assets::ImageAsset& image) override;

    [[nodiscard]] std::expected<MaterialHandle, stellar::platform::Error>
    create_material(const stellar::assets::MaterialAsset& material) override;

    void begin_frame(int width, int height) noexcept override;

    void draw_mesh(MeshHandle mesh,
                   std::span<const MaterialHandle> materials,
                   const std::array<float, 16>& mvp) noexcept override;

    void end_frame() noexcept override;

    void destroy_mesh(MeshHandle mesh) noexcept override;
    void destroy_texture(TextureHandle texture) noexcept override;
    void destroy_material(MaterialHandle material) noexcept override;

private:
    struct MeshPrimitiveGpu {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        int index_count = 0;
    };

    struct MeshRecord {
        std::vector<MeshPrimitiveGpu> primitives;
    };

    struct TextureRecord {
        unsigned int texture = 0;
        int width = 0;
        int height = 0;
    };

    struct MaterialRecord {
        stellar::assets::MaterialAsset material;
    };

    struct DrawBinding {
        MeshHandle mesh;
        std::vector<MaterialHandle> materials;
    };

    [[nodiscard]] std::uint64_t allocate_handle() noexcept;
    void destroy_mesh_record(MeshRecord& record) noexcept;
    void destroy_texture_record(TextureRecord& record) noexcept;

    SDL_Window* window_ = nullptr;
    SDL_GLContext context_ = nullptr;
    unsigned int shader_program_ = 0;
    int mvp_loc_ = -1;
    std::uint64_t next_handle_ = 1;
    std::map<std::uint64_t, MeshRecord> meshes_;
    std::map<std::uint64_t, TextureRecord> textures_;
    std::map<std::uint64_t, MaterialRecord> materials_;
    std::map<std::uint64_t, DrawBinding> draw_bindings_;
};

} // namespace stellar::graphics::opengl
