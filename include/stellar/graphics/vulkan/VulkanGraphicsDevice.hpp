#pragma once

#include <expected>
#include <map>
#include <vector>

#include <vulkan/vulkan.h>

#include "stellar/graphics/GraphicsDevice.hpp"

namespace stellar::graphics::vulkan {

/**
 * @brief Initial Vulkan backend for the backend-neutral GraphicsDevice interface.
 *
 * This implementation creates a Vulkan instance, SDL surface, physical device, logical device,
 * and graphics queue when initialized. Meshes, textures, samplers, and materials are recorded in
 * backend-neutral upload records so RenderScene and glTF resource paths can exercise the same
 * public API as OpenGL. Full Vulkan swapchain, pipeline, GPU memory uploads, and drawing are
 * intentionally deferred; frame and draw calls are explicit no-ops until that renderer exists.
 */
class VulkanGraphicsDevice final : public stellar::graphics::GraphicsDevice {
public:
    VulkanGraphicsDevice() noexcept = default;
    ~VulkanGraphicsDevice() noexcept override;

    VulkanGraphicsDevice(const VulkanGraphicsDevice&) = delete;
    VulkanGraphicsDevice& operator=(const VulkanGraphicsDevice&) = delete;

    /**
     * @brief Initialize a Vulkan instance, SDL surface, physical device, logical device, and queue.
     */
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& window) override;

    /**
     * @brief Validate and record backend-neutral mesh upload data.
     */
    [[nodiscard]] std::expected<MeshHandle, stellar::platform::Error>
    create_mesh(const stellar::assets::MeshAsset& mesh) override;

    /**
     * @brief Validate and record backend-neutral texture upload data.
     */
    [[nodiscard]] std::expected<TextureHandle, stellar::platform::Error>
    create_texture(const TextureUpload& texture) override;

    /**
     * @brief Record a backend-neutral material upload.
     */
    [[nodiscard]] std::expected<MaterialHandle, stellar::platform::Error>
    create_material(const MaterialUpload& material) override;

    /**
     * @brief Begin a Vulkan frame; currently a no-op until swapchain support is implemented.
     */
    void begin_frame(int width, int height) noexcept override;

    /**
     * @brief Submit a mesh draw; currently a no-op until Vulkan pipelines are implemented.
     */
    void draw_mesh(MeshHandle mesh,
                   std::span<const MeshPrimitiveDrawCommand> commands,
                   const MeshDrawTransforms& transforms) noexcept override;

    /**
     * @brief End a Vulkan frame; currently a no-op until presentation support is implemented.
     */
    void end_frame() noexcept override;

    /** @brief Destroy a recorded mesh upload. */
    void destroy_mesh(MeshHandle mesh) noexcept override;

    /** @brief Destroy a recorded texture upload. */
    void destroy_texture(TextureHandle texture) noexcept override;

    /** @brief Destroy a recorded material upload. */
    void destroy_material(MaterialHandle material) noexcept override;

    /**
     * @brief Return whether Vulkan instance/device initialization succeeded.
     */
    [[nodiscard]] bool is_initialized() const noexcept;

private:
    struct MeshPrimitiveRecord {
        std::size_t vertex_count = 0;
        std::size_t index_count = 0;
        bool has_tangents = false;
        bool has_colors = false;
        bool has_skinning = false;
    };

    struct MeshRecord {
        std::vector<MeshPrimitiveRecord> primitives;
    };

    struct TextureRecord {
        stellar::assets::ImageAsset image;
        stellar::assets::TextureColorSpace color_space = stellar::assets::TextureColorSpace::kLinear;
    };

    struct MaterialRecord {
        MaterialUpload upload;
    };

    [[nodiscard]] std::expected<void, stellar::platform::Error>
    create_instance(stellar::platform::Window& window);
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    create_surface(stellar::platform::Window& window);
    [[nodiscard]] std::expected<void, stellar::platform::Error> select_physical_device();
    [[nodiscard]] std::expected<void, stellar::platform::Error> create_logical_device();
    void destroy_vulkan_objects() noexcept;
    [[nodiscard]] std::uint64_t allocate_handle() noexcept;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    std::uint32_t graphics_queue_family_ = 0;
    bool initialized_ = false;
    std::uint64_t next_handle_ = 1;
    std::map<std::uint64_t, MeshRecord> meshes_;
    std::map<std::uint64_t, TextureRecord> textures_;
    std::map<std::uint64_t, MaterialRecord> materials_;
};

} // namespace stellar::graphics::vulkan
