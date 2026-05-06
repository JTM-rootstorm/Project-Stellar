#pragma once

#include <expected>
#include <memory>
#include <span>

#include "stellar/assets/MeshAsset.hpp"
#include "stellar/graphics/GraphicsDevice.hpp"
#include "stellar/graphics/GraphicsHandles.hpp"
#include "stellar/graphics/MaterialUpload.hpp"
#include "stellar/platform/Error.hpp"
#include "stellar/platform/Window.hpp"

namespace stellar::graphics::metal {

/** @brief Apple Metal graphics device implementation for SDL Metal windows. */
class MetalGraphicsDevice final : public stellar::graphics::GraphicsDevice {
public:
    /** @brief Construct an empty Metal graphics device. */
    MetalGraphicsDevice();

    /** @brief Release Metal and SDL Metal view resources. */
    ~MetalGraphicsDevice() noexcept override;

    MetalGraphicsDevice(const MetalGraphicsDevice&) = delete;
    MetalGraphicsDevice& operator=(const MetalGraphicsDevice&) = delete;

    /** @brief Initialize the Metal device, layer, SDL Metal view, and command queue. */
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& window) override;

    /** @brief Upload a static mesh into Metal buffers. */
    [[nodiscard]] std::expected<MeshHandle, stellar::platform::Error>
    create_mesh(const stellar::assets::MeshAsset& mesh) override;

    /** @brief Upload a texture into Metal storage. */
    [[nodiscard]] std::expected<TextureHandle, stellar::platform::Error>
    create_texture(const TextureUpload& texture) override;

    /** @brief Register a resolved material upload for Metal rendering. */
    [[nodiscard]] std::expected<MaterialHandle, stellar::platform::Error>
    create_material(const MaterialUpload& material) override;

    /** @brief Begin a Metal frame when a drawable is available. */
    void begin_frame(int width, int height) noexcept override;

    /** @brief Draw an uploaded static mesh with Metal pipelines. */
    void draw_mesh(MeshHandle mesh,
                   std::span<const MeshPrimitiveDrawCommand> commands,
                   const MeshDrawTransforms& transforms) noexcept override;

    /** @brief Present and commit the current clear-only Metal frame. */
    void end_frame() noexcept override;

    /** @brief Destroy a registered mesh handle. */
    void destroy_mesh(MeshHandle mesh) noexcept override;

    /** @brief Destroy a registered texture handle. */
    void destroy_texture(TextureHandle texture) noexcept override;

    /** @brief Destroy a registered material handle. */
    void destroy_material(MaterialHandle material) noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Return the embedded Metal shader source used by MetalGraphicsDevice.
 */
[[nodiscard]] const char* metal_shader_source() noexcept;

} // namespace stellar::graphics::metal
