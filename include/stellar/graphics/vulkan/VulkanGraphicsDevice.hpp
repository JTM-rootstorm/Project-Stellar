#pragma once

#include "stellar/graphics/FrameReadback.hpp"
#include "stellar/graphics/GraphicsDevice.hpp"

#include <memory>

namespace stellar::graphics::vulkan {

/**
 * @brief Linux Vulkan backend for the shared graphics device abstraction.
 */
class VulkanGraphicsDevice final : public stellar::graphics::GraphicsDevice,
                                   public stellar::graphics::FrameReadbackDevice {
public:
    /** @brief Construct an uninitialized Vulkan graphics device. */
    VulkanGraphicsDevice();

    /** @brief Destroy Vulkan resources owned by the device. */
    ~VulkanGraphicsDevice() noexcept override;

    /** @brief Vulkan devices own backend state and cannot be copied. */
    VulkanGraphicsDevice(const VulkanGraphicsDevice&) = delete;

    /** @brief Vulkan devices own backend state and cannot be copy-assigned. */
    VulkanGraphicsDevice& operator=(const VulkanGraphicsDevice&) = delete;

    /** @brief Initialize the Vulkan device against an SDL Vulkan window. */
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& window) override;

    /** @brief Upload a mesh into Vulkan buffers. */
    [[nodiscard]] std::expected<MeshHandle, stellar::platform::Error>
    create_mesh(const stellar::assets::MeshAsset& mesh) override;

    /** @brief Upload an image into a Vulkan texture. */
    [[nodiscard]] std::expected<TextureHandle, stellar::platform::Error>
    create_texture(const TextureUpload& texture) override;

    /** @brief Store backend-neutral material bindings for Vulkan draw submission. */
    [[nodiscard]] std::expected<MaterialHandle, stellar::platform::Error>
    create_material(const MaterialUpload& material) override;

    /** @brief Begin a Vulkan frame. */
    void begin_frame(int width, int height) noexcept override;

    /** @brief Draw an uploaded mesh with Vulkan. */
    void draw_mesh(MeshHandle mesh,
                   std::span<const MeshPrimitiveDrawCommand> commands,
                   const MeshDrawTransforms& transforms) noexcept override;

    /** @brief Present the current Vulkan frame. */
    void end_frame() noexcept override;

    /** @brief Destroy a Vulkan mesh resource. */
    void destroy_mesh(MeshHandle mesh) noexcept override;

    /** @brief Destroy a Vulkan texture resource. */
    void destroy_texture(TextureHandle texture) noexcept override;

    /** @brief Destroy a Vulkan material resource. */
    void destroy_material(MaterialHandle material) noexcept override;

    /** @brief Request CPU-readable readback for the next completed Vulkan frame. */
    void request_frame_readback() noexcept override;

    /** @brief Take the most recently completed Vulkan frame readback. */
    [[nodiscard]] std::expected<std::optional<stellar::graphics::FrameReadback>,
                                stellar::platform::Error>
    take_frame_readback() noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace stellar::graphics::vulkan
