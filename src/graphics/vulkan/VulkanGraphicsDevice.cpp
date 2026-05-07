#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

#if defined(__APPLE__)
#error "VulkanGraphicsDevice is Linux-only in Project Stellar; use Metal on macOS"
#endif

#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace stellar::graphics::vulkan {
namespace {

constexpr std::uint32_t kMaxFramesInFlight = 1;
constexpr std::uint32_t kMaterialTextureBindingCount = 7;

[[nodiscard]] stellar::platform::Error error(std::string message) {
    return stellar::platform::Error(std::move(message));
}

[[nodiscard]] std::string vk_result_message(std::string_view operation, VkResult result) {
    std::string message(operation);
    message += " failed with VkResult ";
    message += std::to_string(static_cast<int>(result));
    return message;
}

[[nodiscard]] stellar::platform::Error vk_error(std::string_view operation, VkResult result) {
    return error(vk_result_message(operation, result));
}

template <typename T>
void destroy_vector(T& values) noexcept {
    values.clear();
    values.shrink_to_fit();
}

struct BufferRecord {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};

struct TextureRecord {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct MeshPrimitiveRecord {
    BufferRecord vertex_buffer;
    BufferRecord index_buffer;
    std::uint32_t index_count = 0;
    bool has_tangents = false;
    bool has_colors = false;
};

struct MeshRecord {
    std::vector<MeshPrimitiveRecord> primitives;
};

struct SamplerKey {
    stellar::assets::TextureFilter min_filter = stellar::assets::TextureFilter::kUnspecified;
    stellar::assets::TextureFilter mag_filter = stellar::assets::TextureFilter::kUnspecified;
    stellar::assets::TextureWrapMode wrap_s = stellar::assets::TextureWrapMode::kRepeat;
    stellar::assets::TextureWrapMode wrap_t = stellar::assets::TextureWrapMode::kRepeat;

    [[nodiscard]] friend bool operator<(const SamplerKey& lhs,
                                        const SamplerKey& rhs) noexcept {
        return std::tie(lhs.min_filter, lhs.mag_filter, lhs.wrap_s, lhs.wrap_t) <
               std::tie(rhs.min_filter, rhs.mag_filter, rhs.wrap_s, rhs.wrap_t);
    }
};

struct MaterialRecord {
    MaterialUpload upload;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
};

[[nodiscard]] VkFilter to_vk_filter(stellar::assets::TextureFilter filter) noexcept {
    switch (filter) {
        case stellar::assets::TextureFilter::kNearest:
        case stellar::assets::TextureFilter::kNearestMipmapNearest:
        case stellar::assets::TextureFilter::kNearestMipmapLinear:
            return VK_FILTER_NEAREST;
        case stellar::assets::TextureFilter::kLinear:
        case stellar::assets::TextureFilter::kLinearMipmapNearest:
        case stellar::assets::TextureFilter::kLinearMipmapLinear:
        case stellar::assets::TextureFilter::kUnspecified:
            return VK_FILTER_LINEAR;
    }
    return VK_FILTER_LINEAR;
}

[[nodiscard]] VkSamplerMipmapMode to_vk_mipmap_mode(
    stellar::assets::TextureFilter filter) noexcept {
    switch (filter) {
        case stellar::assets::TextureFilter::kNearestMipmapNearest:
        case stellar::assets::TextureFilter::kLinearMipmapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case stellar::assets::TextureFilter::kNearestMipmapLinear:
        case stellar::assets::TextureFilter::kLinearMipmapLinear:
        case stellar::assets::TextureFilter::kNearest:
        case stellar::assets::TextureFilter::kLinear:
        case stellar::assets::TextureFilter::kUnspecified:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

[[nodiscard]] VkSamplerAddressMode to_vk_wrap(
    stellar::assets::TextureWrapMode mode) noexcept {
    switch (mode) {
        case stellar::assets::TextureWrapMode::kClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case stellar::assets::TextureWrapMode::kMirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case stellar::assets::TextureWrapMode::kRepeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

[[nodiscard]] std::expected<std::uint32_t, stellar::platform::Error>
find_memory_type(VkPhysicalDevice physical_device,
                 std::uint32_t type_filter,
                 VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    for (std::uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
        if ((type_filter & (1U << index)) != 0U &&
            (memory_properties.memoryTypes[index].propertyFlags & properties) == properties) {
            return index;
        }
    }
    return std::unexpected(error("Vulkan memory allocation failed: no suitable memory type"));
}

[[nodiscard]] std::expected<BufferRecord, stellar::platform::Error>
create_buffer(VkPhysicalDevice physical_device,
              VkDevice device,
              VkDeviceSize size,
              VkBufferUsageFlags usage,
              VkMemoryPropertyFlags properties) {
    const VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    BufferRecord record;
    VkResult result = vkCreateBuffer(device, &buffer_info, nullptr, &record.buffer);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan buffer creation", result));
    }

    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(device, record.buffer, &memory_requirements);
    auto memory_type = find_memory_type(
        physical_device, memory_requirements.memoryTypeBits, properties);
    if (!memory_type) {
        vkDestroyBuffer(device, record.buffer, nullptr);
        return std::unexpected(memory_type.error());
    }

    const VkMemoryAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = *memory_type,
    };
    result = vkAllocateMemory(device, &allocate_info, nullptr, &record.memory);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(device, record.buffer, nullptr);
        return std::unexpected(vk_error("Vulkan buffer memory allocation", result));
    }
    result = vkBindBufferMemory(device, record.buffer, record.memory, 0);
    if (result != VK_SUCCESS) {
        vkFreeMemory(device, record.memory, nullptr);
        vkDestroyBuffer(device, record.buffer, nullptr);
        return std::unexpected(vk_error("Vulkan buffer memory bind", result));
    }
    record.size = size;
    return record;
}

void destroy_buffer(VkDevice device, BufferRecord& record) noexcept {
    if (record.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, record.buffer, nullptr);
    }
    if (record.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, record.memory, nullptr);
    }
    record = {};
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
write_buffer(VkDevice device, const BufferRecord& record, const void* data, VkDeviceSize size) {
    void* mapped = nullptr;
    const VkResult result = vkMapMemory(device, record.memory, 0, size, 0, &mapped);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan buffer map", result));
    }
    std::memcpy(mapped, data, static_cast<std::size_t>(size));
    vkUnmapMemory(device, record.memory);
    return {};
}

[[nodiscard]] std::expected<VkCommandBuffer, stellar::platform::Error>
begin_single_time_commands(VkDevice device, VkCommandPool command_pool) {
    const VkCommandBufferAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkResult result = vkAllocateCommandBuffers(device, &allocate_info, &command_buffer);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan transient command-buffer allocation", result));
    }

    const VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
        return std::unexpected(vk_error("Vulkan transient command-buffer begin", result));
    }
    return command_buffer;
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
end_single_time_commands(VkDevice device,
                         VkCommandPool command_pool,
                         VkQueue queue,
                         VkCommandBuffer command_buffer) {
    VkResult result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
        return std::unexpected(vk_error("Vulkan transient command-buffer end", result));
    }
    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };
    result = vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
        return std::unexpected(vk_error("Vulkan transient queue submit", result));
    }
    result = vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan transient queue wait", result));
    }
    return {};
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
copy_buffer(VkDevice device,
            VkCommandPool command_pool,
            VkQueue queue,
            VkBuffer source,
            VkBuffer destination,
            VkDeviceSize size) {
    auto command_buffer = begin_single_time_commands(device, command_pool);
    if (!command_buffer) {
        return std::unexpected(command_buffer.error());
    }
    const VkBufferCopy copy_region{
        .size = size,
    };
    vkCmdCopyBuffer(*command_buffer, source, destination, 1, &copy_region);
    return end_single_time_commands(device, command_pool, queue, *command_buffer);
}

[[nodiscard]] std::expected<TextureRecord, stellar::platform::Error>
create_image(VkPhysicalDevice physical_device,
             VkDevice device,
             std::uint32_t width,
             std::uint32_t height,
             VkFormat format,
             VkImageUsageFlags usage) {
    const VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    TextureRecord record{.format = format, .width = width, .height = height};
    VkResult result = vkCreateImage(device, &image_info, nullptr, &record.image);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan image creation", result));
    }

    VkMemoryRequirements memory_requirements{};
    vkGetImageMemoryRequirements(device, record.image, &memory_requirements);
    auto memory_type = find_memory_type(
        physical_device, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memory_type) {
        vkDestroyImage(device, record.image, nullptr);
        return std::unexpected(memory_type.error());
    }

    const VkMemoryAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = *memory_type,
    };
    result = vkAllocateMemory(device, &allocate_info, nullptr, &record.memory);
    if (result != VK_SUCCESS) {
        vkDestroyImage(device, record.image, nullptr);
        return std::unexpected(vk_error("Vulkan image memory allocation", result));
    }
    result = vkBindImageMemory(device, record.image, record.memory, 0);
    if (result != VK_SUCCESS) {
        vkFreeMemory(device, record.memory, nullptr);
        vkDestroyImage(device, record.image, nullptr);
        return std::unexpected(vk_error("Vulkan image memory bind", result));
    }
    return record;
}

void destroy_texture_record(VkDevice device, TextureRecord& record) noexcept {
    if (record.image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, record.image_view, nullptr);
    }
    if (record.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, record.image, nullptr);
    }
    if (record.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, record.memory, nullptr);
    }
    record = {};
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
transition_image_layout(VkDevice device,
                        VkCommandPool command_pool,
                        VkQueue queue,
                        VkImage image,
                        VkImageLayout old_layout,
                        VkImageLayout new_layout) {
    auto command_buffer = begin_single_time_commands(device, command_pool);
    if (!command_buffer) {
        return std::unexpected(command_buffer.error());
    }

    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        vkFreeCommandBuffers(device, command_pool, 1, &*command_buffer);
        return std::unexpected(error("Vulkan image layout transition is unsupported"));
    }
    vkCmdPipelineBarrier(*command_buffer, source_stage, destination_stage, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
    return end_single_time_commands(device, command_pool, queue, *command_buffer);
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
copy_buffer_to_image(VkDevice device,
                     VkCommandPool command_pool,
                     VkQueue queue,
                     VkBuffer buffer,
                     VkImage image,
                     std::uint32_t width,
                     std::uint32_t height) {
    auto command_buffer = begin_single_time_commands(device, command_pool);
    if (!command_buffer) {
        return std::unexpected(command_buffer.error());
    }
    const VkBufferImageCopy region{
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {width, height, 1},
    };
    vkCmdCopyBufferToImage(*command_buffer, buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    return end_single_time_commands(device, command_pool, queue, *command_buffer);
}

[[nodiscard]] std::expected<VkImageView, stellar::platform::Error>
create_image_view(VkDevice device, VkImage image, VkFormat format) {
    const VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    VkImageView image_view = VK_NULL_HANDLE;
    const VkResult result = vkCreateImageView(device, &view_info, nullptr, &image_view);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan image-view creation", result));
    }
    return image_view;
}

[[nodiscard]] std::vector<std::uint8_t> expand_to_rgba8(
    const stellar::assets::ImageAsset& image) {
    if (image.format == stellar::assets::ImageFormat::kR8G8B8A8) {
        return image.pixels;
    }

    std::vector<std::uint8_t> rgba;
    rgba.reserve(static_cast<std::size_t>(image.width) * image.height * 4U);
    for (std::size_t index = 0; index + 2 < image.pixels.size(); index += 3) {
        rgba.push_back(image.pixels[index]);
        rgba.push_back(image.pixels[index + 1]);
        rgba.push_back(image.pixels[index + 2]);
        rgba.push_back(255);
    }
    return rgba;
}

struct QueueFamilyIndices {
    std::optional<std::uint32_t> graphics;
    std::optional<std::uint32_t> present;

    [[nodiscard]] bool complete() const noexcept {
        return graphics.has_value() && present.has_value();
    }
};

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

[[nodiscard]] QueueFamilyIndices find_queue_families(VkPhysicalDevice device,
                                                     VkSurfaceKHR surface) {
    QueueFamilyIndices indices;
    std::uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families.data());

    for (std::uint32_t index = 0; index < family_count; ++index) {
        if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            indices.graphics = index;
        }

        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &present_supported);
        if (present_supported == VK_TRUE) {
            indices.present = index;
        }

        if (indices.complete()) {
            break;
        }
    }
    return indices;
}

[[nodiscard]] SwapchainSupport query_swapchain_support(VkPhysicalDevice device,
                                                       VkSurfaceKHR surface) {
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities);

    std::uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
    support.formats.resize(format_count);
    if (format_count > 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device, surface, &format_count, support.formats.data());
    }

    std::uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
    support.present_modes.resize(present_mode_count);
    if (present_mode_count > 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &present_mode_count, support.present_modes.data());
    }
    return support;
}

[[nodiscard]] bool device_supports_swapchain(VkPhysicalDevice device) {
    std::uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extensions.data());

    for (const auto& extension : extensions) {
        if (std::string_view(extension.extensionName) == VK_KHR_SWAPCHAIN_EXTENSION_NAME) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] VkSurfaceFormatKHR choose_surface_format(
    const std::vector<VkSurfaceFormatKHR>& formats) noexcept {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats.front();
}

[[nodiscard]] VkPresentModeKHR choose_present_mode(
    const std::vector<VkPresentModeKHR>& present_modes) noexcept {
    for (const auto mode : present_modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

[[nodiscard]] VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities,
                                       SDL_Window* window) noexcept {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
    VkExtent2D extent{static_cast<std::uint32_t>(std::max(width, 1)),
                      static_cast<std::uint32_t>(std::max(height, 1))};
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);
    return extent;
}

} // namespace

struct VulkanGraphicsDevice::Impl {
    SDL_Window* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;
    std::uint32_t graphics_family = 0;
    std::uint32_t present_family = 0;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchain_format = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchain_extent{};
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers;
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore render_finished = VK_NULL_HANDLE;
    VkFence in_flight = VK_NULL_HANDLE;
    VkDescriptorSetLayout material_descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorPool material_descriptor_pool = VK_NULL_HANDLE;
    std::map<std::uint64_t, MeshRecord> meshes;
    std::map<std::uint64_t, TextureRecord> textures;
    std::map<std::uint64_t, MaterialRecord> materials;
    std::map<SamplerKey, VkSampler> samplers;
    TextureHandle fallback_white;
    TextureHandle fallback_black;
    TextureHandle fallback_normal;
    std::uint64_t next_handle = 1;
    std::uint32_t image_index = 0;
    bool frame_started = false;
    bool initialized = false;

    [[nodiscard]] std::uint64_t allocate_handle() noexcept {
        return next_handle++;
    }

    void destroy_mesh_record(MeshRecord& record) noexcept {
        for (auto& primitive : record.primitives) {
            destroy_buffer(device, primitive.vertex_buffer);
            destroy_buffer(device, primitive.index_buffer);
        }
        record.primitives.clear();
    }

    [[nodiscard]] std::expected<VkSampler, stellar::platform::Error>
    sampler_for(const stellar::assets::SamplerAsset& sampler) {
        const SamplerKey key{.min_filter = sampler.min_filter,
                             .mag_filter = sampler.mag_filter,
                             .wrap_s = sampler.wrap_s,
                             .wrap_t = sampler.wrap_t};
        if (auto found = samplers.find(key); found != samplers.end()) {
            return found->second;
        }

        const VkSamplerCreateInfo sampler_info{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = to_vk_filter(sampler.mag_filter),
            .minFilter = to_vk_filter(sampler.min_filter),
            .mipmapMode = to_vk_mipmap_mode(sampler.min_filter),
            .addressModeU = to_vk_wrap(sampler.wrap_s),
            .addressModeV = to_vk_wrap(sampler.wrap_t),
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = 0.0F,
            .anisotropyEnable = VK_FALSE,
            .compareEnable = VK_FALSE,
            .minLod = 0.0F,
            .maxLod = 0.0F,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };
        VkSampler vk_sampler = VK_NULL_HANDLE;
        const VkResult result =
            vkCreateSampler(device, &sampler_info, nullptr, &vk_sampler);
        if (result != VK_SUCCESS) {
            return std::unexpected(vk_error("Vulkan sampler creation", result));
        }
        samplers.emplace(key, vk_sampler);
        return vk_sampler;
    }

    [[nodiscard]] std::expected<TextureHandle, stellar::platform::Error>
    upload_rgba_texture(std::string_view name,
                        std::uint32_t width,
                        std::uint32_t height,
                        std::span<const std::uint8_t> pixels,
                        VkFormat format) {
        if (width == 0 || height == 0 || pixels.size() !=
                static_cast<std::size_t>(width) * height * 4U) {
            std::string message = "Vulkan texture upload failed for ";
            message += name;
            message += ": invalid RGBA8 payload";
            return std::unexpected(error(std::move(message)));
        }

        const VkDeviceSize image_size = static_cast<VkDeviceSize>(pixels.size());
        auto staging = create_buffer(physical_device, device, image_size,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!staging) {
            return std::unexpected(staging.error());
        }
        if (auto write = write_buffer(device, *staging, pixels.data(), image_size); !write) {
            destroy_buffer(device, *staging);
            return std::unexpected(write.error());
        }

        auto texture = create_image(physical_device, device, width, height, format,
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                        VK_IMAGE_USAGE_SAMPLED_BIT);
        if (!texture) {
            destroy_buffer(device, *staging);
            return std::unexpected(texture.error());
        }
        if (auto transition = transition_image_layout(
                device, command_pool, graphics_queue, texture->image, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            !transition) {
            destroy_texture_record(device, *texture);
            destroy_buffer(device, *staging);
            return std::unexpected(transition.error());
        }
        if (auto copy = copy_buffer_to_image(device, command_pool, graphics_queue,
                                             staging->buffer, texture->image, width, height);
            !copy) {
            destroy_texture_record(device, *texture);
            destroy_buffer(device, *staging);
            return std::unexpected(copy.error());
        }
        if (auto transition = transition_image_layout(
                device, command_pool, graphics_queue, texture->image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            !transition) {
            destroy_texture_record(device, *texture);
            destroy_buffer(device, *staging);
            return std::unexpected(transition.error());
        }
        destroy_buffer(device, *staging);

        auto image_view = create_image_view(device, texture->image, format);
        if (!image_view) {
            destroy_texture_record(device, *texture);
            return std::unexpected(image_view.error());
        }
        texture->image_view = *image_view;
        const TextureHandle handle{allocate_handle()};
        textures.emplace(handle.value, *texture);
        return handle;
    }

    [[nodiscard]] std::expected<void, stellar::platform::Error> create_fallback_textures() {
        const std::array<std::uint8_t, 4> white{255, 255, 255, 255};
        const std::array<std::uint8_t, 4> black{0, 0, 0, 255};
        const std::array<std::uint8_t, 4> flat_normal{128, 128, 255, 255};

        auto white_texture =
            upload_rgba_texture("vulkan-fallback-white", 1, 1, white,
                                VK_FORMAT_R8G8B8A8_UNORM);
        if (!white_texture) {
            return std::unexpected(white_texture.error());
        }
        auto black_texture =
            upload_rgba_texture("vulkan-fallback-black", 1, 1, black,
                                VK_FORMAT_R8G8B8A8_UNORM);
        if (!black_texture) {
            return std::unexpected(black_texture.error());
        }
        auto normal_texture =
            upload_rgba_texture("vulkan-fallback-normal", 1, 1, flat_normal,
                                VK_FORMAT_R8G8B8A8_UNORM);
        if (!normal_texture) {
            return std::unexpected(normal_texture.error());
        }

        fallback_white = *white_texture;
        fallback_black = *black_texture;
        fallback_normal = *normal_texture;
        return {};
    }

    [[nodiscard]] std::expected<TextureRecord*, stellar::platform::Error>
    texture_record(TextureHandle handle) {
        auto found = textures.find(handle.value);
        if (found == textures.end()) {
            std::string message = "Vulkan material upload failed: unknown texture handle ";
            message += std::to_string(handle.value);
            return std::unexpected(error(std::move(message)));
        }
        return &found->second;
    }

    [[nodiscard]] TextureHandle fallback_for_binding(std::uint32_t binding) const noexcept {
        switch (binding) {
            case 1:
                return fallback_normal;
            case 4:
            case 6:
                return fallback_black;
            default:
                return fallback_white;
        }
    }

    void cleanup_swapchain() noexcept {
        if (device == VK_NULL_HANDLE) {
            return;
        }
        for (auto framebuffer : framebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        framebuffers.clear();
        if (command_pool != VK_NULL_HANDLE && !command_buffers.empty()) {
            vkFreeCommandBuffers(device, command_pool,
                                 static_cast<std::uint32_t>(command_buffers.size()),
                                 command_buffers.data());
        }
        command_buffers.clear();
        if (render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, render_pass, nullptr);
            render_pass = VK_NULL_HANDLE;
        }
        for (auto image_view : swapchain_image_views) {
            vkDestroyImageView(device, image_view, nullptr);
        }
        swapchain_image_views.clear();
        destroy_vector(swapchain_images);
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
    }

    void cleanup() noexcept {
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
            materials.clear();
            if (material_descriptor_pool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, material_descriptor_pool, nullptr);
                material_descriptor_pool = VK_NULL_HANDLE;
            }
            for (auto& [handle, texture] : textures) {
                destroy_texture_record(device, texture);
            }
            textures.clear();
            for (auto& [key, sampler] : samplers) {
                vkDestroySampler(device, sampler, nullptr);
            }
            samplers.clear();
            for (auto& [handle, mesh] : meshes) {
                destroy_mesh_record(mesh);
            }
            meshes.clear();
            if (material_descriptor_set_layout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, material_descriptor_set_layout, nullptr);
            }
            cleanup_swapchain();
            if (image_available != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, image_available, nullptr);
            }
            if (render_finished != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, render_finished, nullptr);
            }
            if (in_flight != VK_NULL_HANDLE) {
                vkDestroyFence(device, in_flight, nullptr);
            }
            if (command_pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device, command_pool, nullptr);
            }
            vkDestroyDevice(device, nullptr);
        }
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
        }
        *this = Impl{};
    }
};

VulkanGraphicsDevice::VulkanGraphicsDevice() : impl_(std::make_unique<Impl>()) {}

VulkanGraphicsDevice::~VulkanGraphicsDevice() noexcept {
    impl_->cleanup();
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::initialize(stellar::platform::Window& window) {
    if (impl_->initialized) {
        return {};
    }
    impl_->window = window.native_handle();
    if (impl_->window == nullptr) {
        return std::unexpected(error("Vulkan initialization failed: SDL window is null"));
    }

    std::uint32_t extension_count = 0;
    if (SDL_Vulkan_GetInstanceExtensions(impl_->window, &extension_count, nullptr) != SDL_TRUE) {
        return std::unexpected(error("Vulkan initialization failed to query SDL extensions: " +
                                     std::string(SDL_GetError())));
    }
    std::vector<const char*> extensions(extension_count);
    if (SDL_Vulkan_GetInstanceExtensions(
            impl_->window, &extension_count, extensions.data()) != SDL_TRUE) {
        return std::unexpected(error("Vulkan initialization failed to read SDL extensions: " +
                                     std::string(SDL_GetError())));
    }

    const VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Stellar Engine",
        .applicationVersion = VK_MAKE_VERSION(0, 3, 2),
        .pEngineName = "Stellar Engine",
        .engineVersion = VK_MAKE_VERSION(0, 3, 2),
        .apiVersion = VK_API_VERSION_1_0,
    };
    const VkInstanceCreateInfo instance_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions.data(),
    };
    VkResult result = vkCreateInstance(&instance_info, nullptr, &impl_->instance);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan instance creation", result));
    }

    if (SDL_Vulkan_CreateSurface(impl_->window, impl_->instance, &impl_->surface) != SDL_TRUE) {
        return std::unexpected(error("Vulkan surface creation failed: " +
                                     std::string(SDL_GetError())));
    }

    std::uint32_t device_count = 0;
    result = vkEnumeratePhysicalDevices(impl_->instance, &device_count, nullptr);
    if (result != VK_SUCCESS || device_count == 0) {
        return std::unexpected(error("Vulkan initialization failed: no physical device found"));
    }
    std::vector<VkPhysicalDevice> physical_devices(device_count);
    result = vkEnumeratePhysicalDevices(
        impl_->instance, &device_count, physical_devices.data());
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan physical-device enumeration", result));
    }

    for (auto physical_device : physical_devices) {
        const QueueFamilyIndices indices =
            find_queue_families(physical_device, impl_->surface);
        if (!indices.complete() || !device_supports_swapchain(physical_device)) {
            continue;
        }
        const SwapchainSupport support =
            query_swapchain_support(physical_device, impl_->surface);
        if (support.formats.empty() || support.present_modes.empty()) {
            continue;
        }
        impl_->physical_device = physical_device;
        impl_->graphics_family = *indices.graphics;
        impl_->present_family = *indices.present;
        break;
    }
    if (impl_->physical_device == VK_NULL_HANDLE) {
        return std::unexpected(
            error("Vulkan initialization failed: no device supports graphics and present"));
    }

    const float queue_priority = 1.0F;
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    const std::set<std::uint32_t> unique_families = {
        impl_->graphics_family, impl_->present_family};
    for (const auto family : unique_families) {
        queue_infos.push_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = family,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        });
    }
    const std::vector<const char*> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const VkDeviceCreateInfo device_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = static_cast<std::uint32_t>(queue_infos.size()),
        .pQueueCreateInfos = queue_infos.data(),
        .enabledExtensionCount = static_cast<std::uint32_t>(device_extensions.size()),
        .ppEnabledExtensionNames = device_extensions.data(),
    };
    result = vkCreateDevice(impl_->physical_device, &device_info, nullptr, &impl_->device);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan logical-device creation", result));
    }
    vkGetDeviceQueue(impl_->device, impl_->graphics_family, 0, &impl_->graphics_queue);
    vkGetDeviceQueue(impl_->device, impl_->present_family, 0, &impl_->present_queue);

    const VkCommandPoolCreateInfo command_pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = impl_->graphics_family,
    };
    result = vkCreateCommandPool(
        impl_->device, &command_pool_info, nullptr, &impl_->command_pool);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan command-pool creation", result));
    }

    const VkSemaphoreCreateInfo semaphore_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    result = vkCreateSemaphore(
        impl_->device, &semaphore_info, nullptr, &impl_->image_available);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan image-available semaphore creation", result));
    }
    result = vkCreateSemaphore(
        impl_->device, &semaphore_info, nullptr, &impl_->render_finished);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan render-finished semaphore creation", result));
    }
    const VkFenceCreateInfo fence_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    result = vkCreateFence(impl_->device, &fence_info, nullptr, &impl_->in_flight);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan frame fence creation", result));
    }

    std::array<VkDescriptorSetLayoutBinding, kMaterialTextureBindingCount> bindings{};
    for (std::uint32_t binding = 0; binding < kMaterialTextureBindingCount; ++binding) {
        bindings[binding] = VkDescriptorSetLayoutBinding{
            .binding = binding,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        };
    }
    const VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    result = vkCreateDescriptorSetLayout(
        impl_->device, &layout_info, nullptr, &impl_->material_descriptor_set_layout);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan material descriptor-set layout creation",
                                        result));
    }

    constexpr std::uint32_t kMaterialDescriptorSetCapacity = 512;
    const VkDescriptorPoolSize pool_size{
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = kMaterialDescriptorSetCapacity * kMaterialTextureBindingCount,
    };
    const VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = kMaterialDescriptorSetCapacity,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };
    result = vkCreateDescriptorPool(
        impl_->device, &pool_info, nullptr, &impl_->material_descriptor_pool);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan material descriptor-pool creation", result));
    }

    if (auto fallbacks = impl_->create_fallback_textures(); !fallbacks) {
        return std::unexpected(fallbacks.error());
    }

    const SwapchainSupport support =
        query_swapchain_support(impl_->physical_device, impl_->surface);
    const VkSurfaceFormatKHR surface_format = choose_surface_format(support.formats);
    const VkPresentModeKHR present_mode = choose_present_mode(support.present_modes);
    const VkExtent2D extent = choose_extent(support.capabilities, impl_->window);
    std::uint32_t image_count = support.capabilities.minImageCount + 1U;
    if (support.capabilities.maxImageCount > 0U) {
        image_count = std::min(image_count, support.capabilities.maxImageCount);
    }

    const std::uint32_t queue_family_indices[] = {
        impl_->graphics_family, impl_->present_family};
    const bool shared_queue = impl_->graphics_family == impl_->present_family;
    const VkSwapchainCreateInfoKHR swapchain_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = impl_->surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode =
            shared_queue ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = shared_queue ? 0U : 2U,
        .pQueueFamilyIndices = shared_queue ? nullptr : queue_family_indices,
        .preTransform = support.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };
    result = vkCreateSwapchainKHR(impl_->device, &swapchain_info, nullptr, &impl_->swapchain);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan swapchain creation", result));
    }
    impl_->swapchain_format = surface_format.format;
    impl_->swapchain_extent = extent;

    std::uint32_t swapchain_image_count = 0;
    vkGetSwapchainImagesKHR(
        impl_->device, impl_->swapchain, &swapchain_image_count, nullptr);
    impl_->swapchain_images.resize(swapchain_image_count);
    vkGetSwapchainImagesKHR(
        impl_->device, impl_->swapchain, &swapchain_image_count, impl_->swapchain_images.data());

    impl_->swapchain_image_views.reserve(impl_->swapchain_images.size());
    for (auto image : impl_->swapchain_images) {
        const VkImageViewCreateInfo view_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = impl_->swapchain_format,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        VkImageView view = VK_NULL_HANDLE;
        result = vkCreateImageView(impl_->device, &view_info, nullptr, &view);
        if (result != VK_SUCCESS) {
            return std::unexpected(vk_error("Vulkan swapchain image-view creation", result));
        }
        impl_->swapchain_image_views.push_back(view);
    }

    const VkAttachmentDescription color_attachment{
        .format = impl_->swapchain_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    const VkAttachmentReference color_attachment_ref{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    const VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
    };
    const VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };
    const VkRenderPassCreateInfo render_pass_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };
    result = vkCreateRenderPass(
        impl_->device, &render_pass_info, nullptr, &impl_->render_pass);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan render-pass creation", result));
    }

    impl_->framebuffers.reserve(impl_->swapchain_image_views.size());
    for (auto image_view : impl_->swapchain_image_views) {
        const VkFramebufferCreateInfo framebuffer_info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = impl_->render_pass,
            .attachmentCount = 1,
            .pAttachments = &image_view,
            .width = impl_->swapchain_extent.width,
            .height = impl_->swapchain_extent.height,
            .layers = 1,
        };
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        result = vkCreateFramebuffer(
            impl_->device, &framebuffer_info, nullptr, &framebuffer);
        if (result != VK_SUCCESS) {
            return std::unexpected(vk_error("Vulkan framebuffer creation", result));
        }
        impl_->framebuffers.push_back(framebuffer);
    }

    impl_->command_buffers.resize(impl_->framebuffers.size());
    const VkCommandBufferAllocateInfo command_buffer_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = impl_->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<std::uint32_t>(impl_->command_buffers.size()),
    };
    result = vkAllocateCommandBuffers(
        impl_->device, &command_buffer_info, impl_->command_buffers.data());
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan command-buffer allocation", result));
    }

    impl_->initialized = true;
    return {};
}

std::expected<MeshHandle, stellar::platform::Error>
VulkanGraphicsDevice::create_mesh(const stellar::assets::MeshAsset& mesh) {
    if (!impl_->initialized) {
        return std::unexpected(error("Vulkan mesh upload failed: device is not initialized"));
    }
    if (mesh.primitives.empty()) {
        return std::unexpected(error("Vulkan mesh upload failed: mesh has no primitives"));
    }

    MeshRecord mesh_record;
    mesh_record.primitives.reserve(mesh.primitives.size());
    for (const auto& primitive : mesh.primitives) {
        if (primitive.topology != stellar::assets::PrimitiveTopology::kTriangles) {
            return std::unexpected(
                error("Vulkan mesh upload failed: only triangle topology is supported"));
        }
        if (primitive.vertices.empty() || primitive.indices.empty()) {
            return std::unexpected(
                error("Vulkan mesh upload failed: primitive has empty vertices or indices"));
        }

        const VkDeviceSize vertex_size =
            static_cast<VkDeviceSize>(primitive.vertices.size() *
                                      sizeof(stellar::assets::StaticVertex));
        const VkDeviceSize index_size =
            static_cast<VkDeviceSize>(primitive.indices.size() * sizeof(std::uint32_t));
        auto vertex_staging = create_buffer(
            impl_->physical_device, impl_->device, vertex_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!vertex_staging) {
            impl_->destroy_mesh_record(mesh_record);
            return std::unexpected(vertex_staging.error());
        }
        auto index_staging = create_buffer(
            impl_->physical_device, impl_->device, index_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!index_staging) {
            destroy_buffer(impl_->device, *vertex_staging);
            impl_->destroy_mesh_record(mesh_record);
            return std::unexpected(index_staging.error());
        }
        if (auto write = write_buffer(impl_->device, *vertex_staging,
                                      primitive.vertices.data(), vertex_size);
            !write) {
            destroy_buffer(impl_->device, *index_staging);
            destroy_buffer(impl_->device, *vertex_staging);
            impl_->destroy_mesh_record(mesh_record);
            return std::unexpected(write.error());
        }
        if (auto write = write_buffer(impl_->device, *index_staging,
                                      primitive.indices.data(), index_size);
            !write) {
            destroy_buffer(impl_->device, *index_staging);
            destroy_buffer(impl_->device, *vertex_staging);
            impl_->destroy_mesh_record(mesh_record);
            return std::unexpected(write.error());
        }

        MeshPrimitiveRecord primitive_record;
        auto vertex_buffer = create_buffer(
            impl_->physical_device, impl_->device, vertex_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (!vertex_buffer) {
            destroy_buffer(impl_->device, *index_staging);
            destroy_buffer(impl_->device, *vertex_staging);
            impl_->destroy_mesh_record(mesh_record);
            return std::unexpected(vertex_buffer.error());
        }
        auto index_buffer = create_buffer(
            impl_->physical_device, impl_->device, index_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (!index_buffer) {
            destroy_buffer(impl_->device, *vertex_buffer);
            destroy_buffer(impl_->device, *index_staging);
            destroy_buffer(impl_->device, *vertex_staging);
            impl_->destroy_mesh_record(mesh_record);
            return std::unexpected(index_buffer.error());
        }
        if (auto copy = copy_buffer(impl_->device, impl_->command_pool, impl_->graphics_queue,
                                    vertex_staging->buffer, vertex_buffer->buffer,
                                    vertex_size);
            !copy) {
            destroy_buffer(impl_->device, *index_buffer);
            destroy_buffer(impl_->device, *vertex_buffer);
            destroy_buffer(impl_->device, *index_staging);
            destroy_buffer(impl_->device, *vertex_staging);
            impl_->destroy_mesh_record(mesh_record);
            return std::unexpected(copy.error());
        }
        if (auto copy = copy_buffer(impl_->device, impl_->command_pool, impl_->graphics_queue,
                                    index_staging->buffer, index_buffer->buffer, index_size);
            !copy) {
            destroy_buffer(impl_->device, *index_buffer);
            destroy_buffer(impl_->device, *vertex_buffer);
            destroy_buffer(impl_->device, *index_staging);
            destroy_buffer(impl_->device, *vertex_staging);
            impl_->destroy_mesh_record(mesh_record);
            return std::unexpected(copy.error());
        }
        destroy_buffer(impl_->device, *index_staging);
        destroy_buffer(impl_->device, *vertex_staging);

        primitive_record.vertex_buffer = *vertex_buffer;
        primitive_record.index_buffer = *index_buffer;
        primitive_record.index_count = static_cast<std::uint32_t>(primitive.indices.size());
        primitive_record.has_tangents = primitive.has_tangents;
        primitive_record.has_colors = primitive.has_colors;
        mesh_record.primitives.push_back(primitive_record);
    }

    const MeshHandle handle{impl_->allocate_handle()};
    impl_->meshes.emplace(handle.value, std::move(mesh_record));
    return handle;
}

std::expected<TextureHandle, stellar::platform::Error>
VulkanGraphicsDevice::create_texture(const TextureUpload& texture) {
    if (!impl_->initialized) {
        return std::unexpected(error("Vulkan texture upload failed: device is not initialized"));
    }
    const auto& image = texture.image;
    if (image.width == 0 || image.height == 0) {
        return std::unexpected(error("Vulkan texture upload failed: image dimensions are empty"));
    }

    std::size_t expected_size = 0;
    switch (image.format) {
        case stellar::assets::ImageFormat::kR8G8B8:
            expected_size = static_cast<std::size_t>(image.width) * image.height * 3U;
            break;
        case stellar::assets::ImageFormat::kR8G8B8A8:
            expected_size = static_cast<std::size_t>(image.width) * image.height * 4U;
            break;
        case stellar::assets::ImageFormat::kUnknown:
            return std::unexpected(error("Vulkan texture upload failed: unsupported image format"));
    }
    if (image.pixels.size() != expected_size) {
        return std::unexpected(error("Vulkan texture upload failed: image byte count mismatch"));
    }

    const std::vector<std::uint8_t> rgba = expand_to_rgba8(image);
    const VkFormat format =
        texture.color_space == stellar::assets::TextureColorSpace::kSrgb
            ? VK_FORMAT_R8G8B8A8_SRGB
            : VK_FORMAT_R8G8B8A8_UNORM;
    return impl_->upload_rgba_texture(image.name, image.width, image.height, rgba, format);
}

std::expected<MaterialHandle, stellar::platform::Error>
VulkanGraphicsDevice::create_material(const MaterialUpload& material) {
    if (!impl_->initialized) {
        return std::unexpected(error("Vulkan material upload failed: device is not initialized"));
    }

    const VkDescriptorSetAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = impl_->material_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &impl_->material_descriptor_set_layout,
    };
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    VkResult result = vkAllocateDescriptorSets(impl_->device, &allocate_info, &descriptor_set);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan material descriptor-set allocation", result));
    }

    const std::array<const std::optional<MaterialTextureBinding>*,
                     kMaterialTextureBindingCount>
        bindings{&material.base_color_texture, &material.normal_texture,
                 &material.metallic_roughness_texture, &material.occlusion_texture,
                 &material.emissive_texture, &material.lightmap_texture,
                 &material.specular_texture};
    stellar::assets::SamplerAsset default_sampler;
    default_sampler.mag_filter = stellar::assets::TextureFilter::kLinear;
    default_sampler.min_filter = stellar::assets::TextureFilter::kLinear;
    default_sampler.wrap_s = stellar::assets::TextureWrapMode::kRepeat;
    default_sampler.wrap_t = stellar::assets::TextureWrapMode::kRepeat;

    std::array<VkDescriptorImageInfo, kMaterialTextureBindingCount> image_infos{};
    for (std::uint32_t binding = 0; binding < kMaterialTextureBindingCount; ++binding) {
        TextureHandle texture_handle = impl_->fallback_for_binding(binding);
        const stellar::assets::SamplerAsset* sampler = &default_sampler;
        if (bindings[binding]->has_value()) {
            texture_handle = (*bindings[binding])->texture;
            sampler = &(*bindings[binding])->sampler;
        }

        auto texture_record = impl_->texture_record(texture_handle);
        if (!texture_record) {
            vkFreeDescriptorSets(impl_->device, impl_->material_descriptor_pool, 1,
                                 &descriptor_set);
            return std::unexpected(texture_record.error());
        }
        auto vk_sampler = impl_->sampler_for(*sampler);
        if (!vk_sampler) {
            vkFreeDescriptorSets(impl_->device, impl_->material_descriptor_pool, 1,
                                 &descriptor_set);
            return std::unexpected(vk_sampler.error());
        }
        image_infos[binding] = VkDescriptorImageInfo{
            .sampler = *vk_sampler,
            .imageView = (*texture_record)->image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }

    std::array<VkWriteDescriptorSet, kMaterialTextureBindingCount> writes{};
    for (std::uint32_t binding = 0; binding < kMaterialTextureBindingCount; ++binding) {
        writes[binding] = VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = binding,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_infos[binding],
        };
    }
    vkUpdateDescriptorSets(impl_->device, static_cast<std::uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);

    const MaterialHandle handle{impl_->allocate_handle()};
    impl_->materials.emplace(handle.value,
                             MaterialRecord{.upload = material,
                                            .descriptor_set = descriptor_set});
    return handle;
}

void VulkanGraphicsDevice::begin_frame(int, int) noexcept {
    if (!impl_->initialized || impl_->frame_started) {
        return;
    }
    if (vkWaitForFences(
            impl_->device, kMaxFramesInFlight, &impl_->in_flight, VK_TRUE, UINT64_MAX) !=
        VK_SUCCESS) {
        return;
    }
    if (vkResetFences(impl_->device, kMaxFramesInFlight, &impl_->in_flight) != VK_SUCCESS) {
        return;
    }

    const VkResult acquire_result =
        vkAcquireNextImageKHR(impl_->device, impl_->swapchain, UINT64_MAX,
                              impl_->image_available, VK_NULL_HANDLE, &impl_->image_index);
    if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
        return;
    }

    const VkCommandBuffer command_buffer = impl_->command_buffers[impl_->image_index];
    vkResetCommandBuffer(command_buffer, 0);
    const VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        return;
    }

    const VkClearValue clear_color{{{0.0F, 0.0F, 0.0F, 1.0F}}};
    const VkRenderPassBeginInfo render_pass_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = impl_->render_pass,
        .framebuffer = impl_->framebuffers[impl_->image_index],
        .renderArea = {{0, 0}, impl_->swapchain_extent},
        .clearValueCount = 1,
        .pClearValues = &clear_color,
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    impl_->frame_started = true;
}

void VulkanGraphicsDevice::draw_mesh(MeshHandle,
                                     std::span<const MeshPrimitiveDrawCommand>,
                                     const MeshDrawTransforms&) noexcept {}

void VulkanGraphicsDevice::end_frame() noexcept {
    if (!impl_->initialized || !impl_->frame_started) {
        return;
    }
    const VkCommandBuffer command_buffer = impl_->command_buffers[impl_->image_index];
    vkCmdEndRenderPass(command_buffer);
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        impl_->frame_started = false;
        return;
    }

    const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &impl_->image_available,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &impl_->render_finished,
    };
    if (vkQueueSubmit(
            impl_->graphics_queue, 1, &submit_info, impl_->in_flight) != VK_SUCCESS) {
        impl_->frame_started = false;
        return;
    }

    const VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &impl_->render_finished,
        .swapchainCount = 1,
        .pSwapchains = &impl_->swapchain,
        .pImageIndices = &impl_->image_index,
    };
    vkQueuePresentKHR(impl_->present_queue, &present_info);
    impl_->frame_started = false;
}

void VulkanGraphicsDevice::destroy_mesh(MeshHandle mesh) noexcept {
    if (!impl_->initialized) {
        return;
    }
    auto found = impl_->meshes.find(mesh.value);
    if (found == impl_->meshes.end()) {
        return;
    }
    impl_->destroy_mesh_record(found->second);
    impl_->meshes.erase(found);
}

void VulkanGraphicsDevice::destroy_texture(TextureHandle texture) noexcept {
    if (!impl_->initialized) {
        return;
    }
    if (texture == impl_->fallback_white || texture == impl_->fallback_black ||
        texture == impl_->fallback_normal) {
        return;
    }
    auto found = impl_->textures.find(texture.value);
    if (found == impl_->textures.end()) {
        return;
    }
    destroy_texture_record(impl_->device, found->second);
    impl_->textures.erase(found);
}

void VulkanGraphicsDevice::destroy_material(MaterialHandle material) noexcept {
    if (!impl_->initialized) {
        return;
    }
    auto found = impl_->materials.find(material.value);
    if (found == impl_->materials.end()) {
        return;
    }
    if (found->second.descriptor_set != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(impl_->device, impl_->material_descriptor_pool, 1,
                             &found->second.descriptor_set);
    }
    impl_->materials.erase(found);
}

} // namespace stellar::graphics::vulkan
