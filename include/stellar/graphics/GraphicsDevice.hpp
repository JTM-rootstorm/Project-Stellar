#pragma once

#include <array>
#include <expected>
#include <span>

#include "stellar/assets/ImageAsset.hpp"
#include "stellar/assets/MeshAsset.hpp"
#include "stellar/assets/TextureAsset.hpp"
#include "stellar/graphics/GraphicsHandles.hpp"
#include "stellar/graphics/MaterialUpload.hpp"
#include "stellar/platform/Window.hpp"

namespace stellar::graphics {

/**
 * @brief Backend-neutral per-draw transforms for static mesh rendering.
 */
struct MeshDrawTransforms {
    /** @brief Column-major model-view-projection transform matrix. */
    std::array<float, 16> mvp{};

    /** @brief Column-major model/world transform matrix. */
    std::array<float, 16> world{};

    /** @brief Column-major inverse-transpose world-space normal transform matrix. */
    std::array<float, 9> normal{};
};

/**
 * @brief Backend-neutral image upload payload with color-space metadata.
 */
struct TextureUpload {
    stellar::assets::ImageAsset image;
    stellar::assets::TextureColorSpace color_space = stellar::assets::TextureColorSpace::kLinear;
};

/**
 * @brief One primitive draw within an uploaded mesh.
 */
struct MeshPrimitiveDrawCommand {
    /** @brief Primitive index inside the uploaded mesh. */
    std::size_t primitive_index = 0;

    /** @brief Material handle to use for this primitive draw. */
    MaterialHandle material;

    /**
     * @brief Final skin joint matrices for this draw, in skin joint order.
     *
     * An empty span means the primitive should use the static mesh path. Matrix data is
     * backend-neutral and must remain valid for the duration of the draw_mesh call.
     */
    std::span<const std::array<float, 16>> skin_joint_matrices;
};

/**
 * @brief Backend-neutral GPU resource upload interface.
 *
 * The device owns backend-specific resources and returns opaque handles so the
 * importer and higher-level asset systems never see raw OpenGL/Vulkan state.
 */
class GraphicsDevice {
public:
    virtual ~GraphicsDevice() = default;

    /**
     * @brief Initialize the device against an existing platform window.
     * @param window Platform window to bind the backend to.
     * @return std::expected<void, stellar::platform::Error> on failure.
     */
    [[nodiscard]] virtual std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& window) = 0;

    /**
     * @brief Upload a mesh to GPU memory.
     * @param mesh CPU-side mesh asset.
     * @return Opaque mesh handle on success.
     */
    [[nodiscard]] virtual std::expected<MeshHandle, stellar::platform::Error>
    create_mesh(const stellar::assets::MeshAsset& mesh) = 0;

    /**
     * @brief Upload an image as a GPU texture.
     * @param texture CPU-side image asset and upload metadata.
     * @return Opaque texture handle on success.
     */
    [[nodiscard]] virtual std::expected<TextureHandle, stellar::platform::Error>
    create_texture(const TextureUpload& texture) = 0;

    /**
     * @brief Register a material for later rendering use.
     * @param material Backend-neutral material description with resolved textures.
     * @return Opaque material handle on success.
     */
    [[nodiscard]] virtual std::expected<MaterialHandle, stellar::platform::Error>
    create_material(const MaterialUpload& material) = 0;

    /**
     * @brief Begin a frame and clear the target.
     * @param width Viewport width in pixels.
     * @param height Viewport height in pixels.
     */
    virtual void begin_frame(int width, int height) noexcept = 0;

    /**
     * @brief Draw a mesh using backend-neutral world and projection transforms.
     * @param mesh Opaque mesh handle.
     * @param commands Primitive draw commands in submission order.
     * @param transforms Column-major MVP, world, and normal transform matrices.
     */
    virtual void draw_mesh(MeshHandle mesh,
                           std::span<const MeshPrimitiveDrawCommand> commands,
                           const MeshDrawTransforms& transforms) noexcept = 0;

    /**
     * @brief Present the current frame.
     */
    virtual void end_frame() noexcept = 0;

    /**
     * @brief Destroy a mesh created by this device.
     */
    virtual void destroy_mesh(MeshHandle mesh) noexcept = 0;

    /**
     * @brief Destroy a texture created by this device.
     */
    virtual void destroy_texture(TextureHandle texture) noexcept = 0;

    /**
     * @brief Destroy a material created by this device.
     */
    virtual void destroy_material(MaterialHandle material) noexcept = 0;
};

} // namespace stellar::graphics
