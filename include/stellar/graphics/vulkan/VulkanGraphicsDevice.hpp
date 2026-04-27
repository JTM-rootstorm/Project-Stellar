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
     * swapchain, render pass, graphics pipeline, and graphics queue when initialized. Mesh buffers
     * and texture images are uploaded to Vulkan resources, and the initial draw path renders opaque
     * static geometry with base-color material factors.
 */
class VulkanGraphicsDevice final : public stellar::graphics::GraphicsDevice {
public:
    /** @brief Construct an uninitialized Vulkan graphics device. */
    VulkanGraphicsDevice() noexcept = default;

    /** @brief Destroy Vulkan resources owned by this graphics device. */
    ~VulkanGraphicsDevice() noexcept override;

    /** @brief Copy construction is disabled because Vulkan handles have unique ownership. */
    VulkanGraphicsDevice(const VulkanGraphicsDevice&) = delete;

    /** @brief Copy assignment is disabled because Vulkan handles have unique ownership. */
    VulkanGraphicsDevice& operator=(const VulkanGraphicsDevice&) = delete;

    /**
     * @brief Initialize a Vulkan instance, SDL surface, physical device, logical device, and queue.
     */
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& window) override;

    /**
     * @brief Validate mesh data and upload static vertex/index buffers to Vulkan memory.
     */
    [[nodiscard]] std::expected<MeshHandle, stellar::platform::Error>
    create_mesh(const stellar::assets::MeshAsset& mesh) override;

    /**
     * @brief Validate image data and upload a sampled Vulkan texture image.
     */
    [[nodiscard]] std::expected<TextureHandle, stellar::platform::Error>
    create_texture(const TextureUpload& texture) override;

    /**
     * @brief Record a backend-neutral material upload.
     */
    [[nodiscard]] std::expected<MaterialHandle, stellar::platform::Error>
    create_material(const MaterialUpload& material) override;

    /**
     * @brief Begin a Vulkan frame by acquiring and clearing the swapchain image.
     */
    void begin_frame(int width, int height) noexcept override;

    /**
     * @brief Record opaque static mesh draw commands into the active Vulkan frame.
     */
    void draw_mesh(MeshHandle mesh,
                   std::span<const MeshPrimitiveDrawCommand> commands,
                   const MeshDrawTransforms& transforms) noexcept override;

    /**
     * @brief End a Vulkan frame by submitting the clear pass and presenting the swapchain image.
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
        VkBuffer vertex_buffer = VK_NULL_HANDLE;
        VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
        VkDeviceSize vertex_buffer_size = 0;
        VkBuffer index_buffer = VK_NULL_HANDLE;
        VkDeviceMemory index_memory = VK_NULL_HANDLE;
        VkDeviceSize index_buffer_size = 0;
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
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t mip_levels = 1;
        VkFormat format = VK_FORMAT_UNDEFINED;
        stellar::assets::TextureColorSpace color_space = stellar::assets::TextureColorSpace::kLinear;
    };

    struct MaterialRecord {
        MaterialUpload upload;
        std::vector<VkSampler> samplers;
    };

    struct FrameResources {
        VkCommandPool command_pool = VK_NULL_HANDLE;
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        VkSemaphore image_available_semaphore = VK_NULL_HANDLE;
        VkSemaphore render_finished_semaphore = VK_NULL_HANDLE;
        VkFence in_flight_fence = VK_NULL_HANDLE;
    };

    [[nodiscard]] std::expected<void, stellar::platform::Error>
    create_instance(stellar::platform::Window& window);
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    create_surface(stellar::platform::Window& window);
    [[nodiscard]] std::expected<void, stellar::platform::Error> select_physical_device();
    [[nodiscard]] std::expected<void, stellar::platform::Error> create_logical_device();
    [[nodiscard]] std::expected<void, stellar::platform::Error> create_command_resources();
    [[nodiscard]] std::expected<void, stellar::platform::Error> create_sync_objects();
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    create_swapchain_resources(std::uint32_t preferred_width, std::uint32_t preferred_height);
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    recreate_swapchain_resources(std::uint32_t preferred_width, std::uint32_t preferred_height);
    [[nodiscard]] std::expected<void, stellar::platform::Error> create_swapchain(
        std::uint32_t preferred_width, std::uint32_t preferred_height);
    [[nodiscard]] std::expected<void, stellar::platform::Error> create_swapchain_image_views();
    [[nodiscard]] std::expected<void, stellar::platform::Error> create_render_pass();
    [[nodiscard]] std::expected<void, stellar::platform::Error> create_pipeline_layout();
    [[nodiscard]] std::expected<void, stellar::platform::Error> create_graphics_pipeline();
    [[nodiscard]] std::expected<void, stellar::platform::Error> create_depth_resources();
    [[nodiscard]] std::expected<void, stellar::platform::Error> create_framebuffers();
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    create_buffer(VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties,
                  VkBuffer& buffer,
                  VkDeviceMemory& memory);
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    upload_to_buffer(VkDeviceMemory memory, const void* data, VkDeviceSize size);
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    create_image(std::uint32_t width,
                 std::uint32_t height,
                 std::uint32_t mip_levels,
                 VkFormat format,
                 VkImageTiling tiling,
                 VkImageUsageFlags usage,
                 VkMemoryPropertyFlags properties,
                 VkImage& image,
                 VkDeviceMemory& memory);
    [[nodiscard]] std::expected<VkCommandBuffer, stellar::platform::Error>
    begin_single_time_commands();
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    end_single_time_commands(VkCommandBuffer command_buffer);
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    transition_image_layout(VkImage image,
                            VkFormat format,
                            std::uint32_t mip_levels,
                            VkImageLayout old_layout,
                            VkImageLayout new_layout);
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    copy_buffer_to_image(VkBuffer buffer, VkImage image, std::uint32_t width, std::uint32_t height);
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    generate_texture_mipmaps(VkImage image,
                             VkFormat format,
                             std::uint32_t width,
                             std::uint32_t height,
                             std::uint32_t mip_levels);
    [[nodiscard]] std::expected<VkImageView, stellar::platform::Error>
    create_texture_image_view(VkImage image, VkFormat format, std::uint32_t mip_levels);
    [[nodiscard]] std::expected<VkSampler, stellar::platform::Error>
    create_sampler(const stellar::assets::SamplerAsset& sampler, std::uint32_t mip_levels);
    [[nodiscard]] std::expected<std::uint32_t, stellar::platform::Error>
    find_memory_type(std::uint32_t type_filter, VkMemoryPropertyFlags properties) const;
    [[nodiscard]] std::expected<VkFormat, stellar::platform::Error> find_depth_format() const;
    void destroy_buffer(VkBuffer& buffer, VkDeviceMemory& memory) noexcept;
    void destroy_mesh_record(MeshRecord& record) noexcept;
    void destroy_texture_record(TextureRecord& record) noexcept;
    void destroy_material_record(MaterialRecord& record) noexcept;
    void destroy_swapchain_resources() noexcept;
    void destroy_command_resources() noexcept;
    void destroy_sync_objects() noexcept;
    void destroy_vulkan_objects() noexcept;
    [[nodiscard]] std::uint64_t allocate_handle() noexcept;

    SDL_Window* window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline_ = VK_NULL_HANDLE;
    VkImage depth_image_ = VK_NULL_HANDLE;
    VkDeviceMemory depth_image_memory_ = VK_NULL_HANDLE;
    VkImageView depth_image_view_ = VK_NULL_HANDLE;
    VkFormat swapchain_image_format_ = VK_FORMAT_UNDEFINED;
    VkFormat depth_format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchain_extent_{0, 0};
    std::uint32_t graphics_queue_family_ = 0;
    std::uint32_t current_frame_index_ = 0;
    std::uint32_t current_swapchain_image_index_ = 0;
    bool initialized_ = false;
    bool frame_in_progress_ = false;
    bool swapchain_needs_rebuild_ = false;
    std::uint64_t next_handle_ = 1;
    std::vector<FrameResources> frames_;
    std::vector<VkImage> swapchain_images_;
    std::vector<VkFence> swapchain_image_fences_;
    std::vector<VkImageView> swapchain_image_views_;
    std::vector<VkFramebuffer> swapchain_framebuffers_;
    std::map<std::uint64_t, MeshRecord> meshes_;
    std::map<std::uint64_t, TextureRecord> textures_;
    std::map<std::uint64_t, MaterialRecord> materials_;
};

} // namespace stellar::graphics::vulkan
