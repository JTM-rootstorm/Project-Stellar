#pragma once

#include <array>
#include <cstdint>
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
    /** @brief Construct an uninitialized OpenGL graphics device. */
    OpenGLGraphicsDevice() noexcept = default;

    /** @brief Destroy OpenGL resources and the owned GL context. */
    ~OpenGLGraphicsDevice() noexcept override;

    /** @brief OpenGL devices own GL object state and cannot be copied. */
    OpenGLGraphicsDevice(const OpenGLGraphicsDevice&) = delete;

    /** @brief OpenGL devices own GL object state and cannot be copy-assigned. */
    OpenGLGraphicsDevice& operator=(const OpenGLGraphicsDevice&) = delete;

    /** @brief Initialize SDL/OpenGL state for the supplied platform window. */
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& window) override;

    /** @brief Upload mesh primitives into OpenGL vertex/index buffers. */
    [[nodiscard]] std::expected<MeshHandle, stellar::platform::Error>
    create_mesh(const stellar::assets::MeshAsset& mesh) override;

    /** @brief Upload an image into an OpenGL texture object. */
    [[nodiscard]] std::expected<TextureHandle, stellar::platform::Error>
    create_texture(const TextureUpload& texture) override;

    /** @brief Store backend-neutral material bindings for OpenGL draw submission. */
    [[nodiscard]] std::expected<MaterialHandle, stellar::platform::Error>
    create_material(const MaterialUpload& material) override;

    /** @brief Begin an OpenGL frame and configure the viewport and clear state. */
    void begin_frame(int width, int height) noexcept override;

    /** @brief Draw uploaded mesh primitives with OpenGL shader and texture bindings. */
    void draw_mesh(MeshHandle mesh,
                   std::span<const MeshPrimitiveDrawCommand> commands,
                   const MeshDrawTransforms& transforms) noexcept override;

    /** @brief Present the OpenGL back buffer for the bound SDL window. */
    void end_frame() noexcept override;

    /** @brief Delete OpenGL buffers and vertex arrays for an uploaded mesh. */
    void destroy_mesh(MeshHandle mesh) noexcept override;

    /** @brief Delete an OpenGL texture object created by this device. */
    void destroy_texture(TextureHandle texture) noexcept override;

    /** @brief Drop a material record created by this device. */
    void destroy_material(MaterialHandle material) noexcept override;

private:
    struct MeshPrimitiveGpu {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        int index_count = 0;
        bool has_tangents = false;
        bool has_colors = false;
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
        MaterialUpload upload;
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
    int model_loc_ = -1;
    int normal_matrix_loc_ = -1;
    std::uint64_t next_handle_ = 1;
    std::map<std::uint64_t, MeshRecord> meshes_;
    std::map<std::uint64_t, TextureRecord> textures_;
    std::map<std::uint64_t, MaterialRecord> materials_;
    std::map<std::uint64_t, DrawBinding> draw_bindings_;
};

} // namespace stellar::graphics::opengl
