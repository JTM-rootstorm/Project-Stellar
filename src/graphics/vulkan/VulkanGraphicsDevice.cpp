#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

#if defined(__APPLE__)
#error "VulkanGraphicsDevice is Linux-only in Project Stellar; use Metal on macOS"
#endif

#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <expected>
#include <fstream>
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
constexpr std::uint32_t kDrawUniformBinding = kMaterialTextureBindingCount;
constexpr std::uint32_t kMaterialDescriptorBindingCount = kMaterialTextureBindingCount + 1;
constexpr std::uint32_t kDrawUniformCapacity = 4096;

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

struct PipelineKey {
    bool alpha_blend = false;
    bool double_sided = false;
    bool readback_target = false;

    [[nodiscard]] friend bool operator<(const PipelineKey& lhs,
                                        const PipelineKey& rhs) noexcept {
        return std::tie(lhs.alpha_blend, lhs.double_sided, lhs.readback_target) <
               std::tie(rhs.alpha_blend, rhs.double_sided, rhs.readback_target);
    }
};

struct PipelineRecord {
    VkPipeline pipeline = VK_NULL_HANDLE;
};

struct DrawUniforms {
    std::array<float, 16> mvp{};
    std::array<float, 16> world{};
    std::array<float, 4> normal_column0{1.0F, 0.0F, 0.0F, 0.0F};
    std::array<float, 4> normal_column1{0.0F, 1.0F, 0.0F, 0.0F};
    std::array<float, 4> normal_column2{0.0F, 0.0F, 1.0F, 0.0F};
    std::array<float, 4> base_color{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 4> emissive_factor{};
    std::array<float, 4> global_light_color{};
    std::array<float, 4> camera_world_position{};
    std::array<float, 4> key_light_direction{0.35F, 0.8F, 0.45F, 0.0F};
    std::array<std::array<float, 4>, 6> texture_transform0{};
    std::array<std::array<float, 4>, 6> texture_transform1{
        std::array<float, 4>{1.0F, 1.0F, 0.0F, 0.0F},
        std::array<float, 4>{1.0F, 1.0F, 0.0F, 0.0F},
        std::array<float, 4>{1.0F, 1.0F, 0.0F, 0.0F},
        std::array<float, 4>{1.0F, 1.0F, 0.0F, 0.0F},
        std::array<float, 4>{1.0F, 1.0F, 0.0F, 0.0F},
        std::array<float, 4>{1.0F, 1.0F, 0.0F, 0.0F},
    };
    std::array<float, 4> material_factors0{0.0F, 1.0F, 1.0F, 0.0F};
    std::array<float, 4> material_factors1{0.0F, 32.0F, 1.0F, 1.0F};
    std::array<float, 4> material_factors2{0.0F, 0.0F, 0.5F, 0.0F};
    std::array<std::uint32_t, 4> material_flags0{};
    std::array<std::uint32_t, 4> material_flags1{};
    std::array<std::uint32_t, 4> material_flags2{};
    std::array<std::uint32_t, 4> material_flags3{};
    std::array<std::uint32_t, 4> material_flags4{};
};

static_assert(sizeof(DrawUniforms) % 16 == 0,
              "Vulkan draw uniforms must stay std140-aligned");

[[nodiscard]] VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment) noexcept {
    if (alignment == 0) {
        return value;
    }
    const VkDeviceSize remainder = value % alignment;
    return remainder == 0 ? value : value + alignment - remainder;
}

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
create_image_view(VkDevice device,
                  VkImage image,
                  VkFormat format,
                  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) {
    const VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {aspect, 0, 1, 0, 1},
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

std::array<float, 4>
transform0_for(const std::optional<MaterialTextureBinding>& binding) noexcept {
    if (!binding.has_value() || !binding->transform.enabled) {
        return {0.0F, 0.0F, 0.0F, 0.0F};
    }

    return {binding->transform.offset[0], binding->transform.offset[1],
            binding->transform.rotation, 1.0F};
}

std::array<float, 4>
transform1_for(const std::optional<MaterialTextureBinding>& binding) noexcept {
    if (!binding.has_value() || !binding->transform.enabled) {
        return {1.0F, 1.0F, 0.0F, 0.0F};
    }

    return {binding->transform.scale[0], binding->transform.scale[1], 0.0F, 0.0F};
}

[[nodiscard]] std::uint32_t alpha_mode_value(stellar::assets::AlphaMode mode) noexcept {
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

[[nodiscard]] bool binding_uses_supported_texcoord(
    const std::optional<MaterialTextureBinding>& binding) noexcept {
    return binding.has_value() && binding->texcoord_set <= 1;
}

[[nodiscard]] std::expected<std::vector<std::uint32_t>, stellar::platform::Error>
read_spirv_words(const char* path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) {
        std::string message = "Vulkan shader load failed: ";
        message += path;
        return std::unexpected(error(std::move(message)));
    }
    const auto size = file.tellg();
    if (size <= 0 || size % static_cast<std::streamoff>(sizeof(std::uint32_t)) != 0) {
        std::string message = "Vulkan shader load failed: invalid SPIR-V size for ";
        message += path;
        return std::unexpected(error(std::move(message)));
    }
    std::vector<std::uint32_t> words(static_cast<std::size_t>(size) /
                                     sizeof(std::uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(words.data()), size);
    return words;
}

[[nodiscard]] std::expected<VkShaderModule, stellar::platform::Error>
create_shader_module(VkDevice device, std::span<const std::uint32_t> words) {
    const VkShaderModuleCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = words.size_bytes(),
        .pCode = words.data(),
    };
    VkShaderModule module = VK_NULL_HANDLE;
    const VkResult result = vkCreateShaderModule(device, &create_info, nullptr, &module);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan shader-module creation", result));
    }
    return module;
}

[[nodiscard]] std::expected<VkFormat, stellar::platform::Error>
choose_depth_format(VkPhysicalDevice device) {
    constexpr std::array<VkFormat, 3> candidates{
        VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
    for (VkFormat format : candidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(device, format, &properties);
        if ((properties.optimalTilingFeatures &
             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
            return format;
        }
    }
    return std::unexpected(error("Vulkan depth format selection failed"));
}

[[nodiscard]] bool supports_readback_format(VkFormat format) noexcept {
    switch (format) {
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
            return true;
        default:
            return false;
    }
}

void copy_swapchain_rgba(VkFormat format,
                         int width,
                         int height,
                         std::span<const std::uint8_t> source,
                         std::vector<std::uint8_t>& dest) {
    dest.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                 static_cast<std::size_t>(x)) *
                4U;
            if (format == VK_FORMAT_B8G8R8A8_UNORM ||
                format == VK_FORMAT_B8G8R8A8_SRGB) {
                dest[index + 0] = source[index + 2];
                dest[index + 1] = source[index + 1];
                dest[index + 2] = source[index + 0];
                dest[index + 3] = source[index + 3];
            } else {
                dest[index + 0] = source[index + 0];
                dest[index + 1] = source[index + 1];
                dest[index + 2] = source[index + 2];
                dest[index + 3] = source[index + 3];
            }
        }
    }
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
    bool swapchain_readback_supported = false;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    TextureRecord depth_buffer;
    TextureRecord readback_color;
    VkFormat readback_color_format = VK_FORMAT_UNDEFINED;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkRenderPass readback_render_pass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;
    VkFramebuffer readback_framebuffer = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers;
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore render_finished = VK_NULL_HANDLE;
    VkFence in_flight = VK_NULL_HANDLE;
    VkDescriptorSetLayout material_descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorPool material_descriptor_pool = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    BufferRecord draw_uniform_buffer;
    void* draw_uniform_mapped = nullptr;
    VkDeviceSize draw_uniform_stride = sizeof(DrawUniforms);
    std::uint32_t draw_uniform_index = 0;
    std::map<PipelineKey, PipelineRecord> pipelines;
    std::map<std::uint64_t, MeshRecord> meshes;
    std::map<std::uint64_t, TextureRecord> textures;
    std::map<std::uint64_t, MaterialRecord> materials;
    std::map<SamplerKey, VkSampler> samplers;
    TextureHandle fallback_white;
    TextureHandle fallback_black;
    TextureHandle fallback_normal;
    MaterialHandle fallback_material;
    std::uint64_t next_handle = 1;
    std::uint32_t image_index = 0;
    bool frame_started = false;
    bool initialized = false;
    bool rendering_to_readback_target = false;
    bool readback_requested = false;
    std::optional<stellar::graphics::FrameReadback> last_readback;
    std::optional<std::string> last_readback_error;

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

    [[nodiscard]] std::expected<MaterialHandle, stellar::platform::Error>
    create_material_record(const MaterialUpload& material) {
        const VkDescriptorSetAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = material_descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &material_descriptor_set_layout,
        };
        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        VkResult result = vkAllocateDescriptorSets(device, &allocate_info, &descriptor_set);
        if (result != VK_SUCCESS) {
            return std::unexpected(vk_error("Vulkan material descriptor-set allocation",
                                            result));
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
            TextureHandle texture_handle = fallback_for_binding(binding);
            const stellar::assets::SamplerAsset* sampler = &default_sampler;
            if (bindings[binding]->has_value()) {
                texture_handle = (*bindings[binding])->texture;
                sampler = &(*bindings[binding])->sampler;
            }

            auto texture = texture_record(texture_handle);
            if (!texture) {
                vkFreeDescriptorSets(device, material_descriptor_pool, 1, &descriptor_set);
                return std::unexpected(texture.error());
            }
            auto vk_sampler = sampler_for(*sampler);
            if (!vk_sampler) {
                vkFreeDescriptorSets(device, material_descriptor_pool, 1, &descriptor_set);
                return std::unexpected(vk_sampler.error());
            }
            image_infos[binding] = VkDescriptorImageInfo{
                .sampler = *vk_sampler,
                .imageView = (*texture)->image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
        }

        std::array<VkWriteDescriptorSet, kMaterialDescriptorBindingCount> writes{};
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
        const VkDescriptorBufferInfo uniform_info{
            .buffer = draw_uniform_buffer.buffer,
            .offset = 0,
            .range = sizeof(DrawUniforms),
        };
        writes[kDrawUniformBinding] = VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = kDrawUniformBinding,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .pBufferInfo = &uniform_info,
        };
        vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);

        const MaterialHandle handle{allocate_handle()};
        materials.emplace(handle.value,
                          MaterialRecord{.upload = material,
                                         .descriptor_set = descriptor_set});
        return handle;
    }

    [[nodiscard]] std::expected<void, stellar::platform::Error> create_fallback_material() {
        stellar::assets::MaterialAsset material;
        material.name = "vulkan-fallback-material";
        material.base_color_factor = {0.85F, 0.9F, 1.0F, 1.0F};
        material.unlit = true;
        auto handle = create_material_record(MaterialUpload{.material = material});
        if (!handle) {
            return std::unexpected(handle.error());
        }
        fallback_material = *handle;
        return {};
    }

    [[nodiscard]] std::optional<std::uint32_t>
    write_draw_uniforms(const DrawUniforms& uniforms) noexcept {
        if (draw_uniform_mapped == nullptr || draw_uniform_index >= kDrawUniformCapacity) {
            return std::nullopt;
        }
        const VkDeviceSize offset =
            static_cast<VkDeviceSize>(draw_uniform_index) * draw_uniform_stride;
        std::memcpy(static_cast<std::byte*>(draw_uniform_mapped) + offset, &uniforms,
                    sizeof(uniforms));
        ++draw_uniform_index;
        return static_cast<std::uint32_t>(offset);
    }

    [[nodiscard]] std::expected<VkPipeline, stellar::platform::Error>
    pipeline_for(PipelineKey key) {
        if (auto found = pipelines.find(key); found != pipelines.end()) {
            return found->second.pipeline;
        }

        auto vertex_words = read_spirv_words(STELLAR_VULKAN_VERTEX_SPV);
        if (!vertex_words) {
            return std::unexpected(vertex_words.error());
        }
        auto fragment_words = read_spirv_words(STELLAR_VULKAN_FRAGMENT_SPV);
        if (!fragment_words) {
            return std::unexpected(fragment_words.error());
        }
        auto vertex_module = create_shader_module(device, *vertex_words);
        if (!vertex_module) {
            return std::unexpected(vertex_module.error());
        }
        auto fragment_module = create_shader_module(device, *fragment_words);
        if (!fragment_module) {
            vkDestroyShaderModule(device, *vertex_module, nullptr);
            return std::unexpected(fragment_module.error());
        }

        const VkPipelineShaderStageCreateInfo shader_stages[] = {
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
             .stage = VK_SHADER_STAGE_VERTEX_BIT,
             .module = *vertex_module,
             .pName = "main"},
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
             .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
             .module = *fragment_module,
             .pName = "main"},
        };
        const VkVertexInputBindingDescription binding_description{
            .binding = 0,
            .stride = sizeof(stellar::assets::StaticVertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
        const std::array<VkVertexInputAttributeDescription, 6> attributes{{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT,
             offsetof(stellar::assets::StaticVertex, position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT,
             offsetof(stellar::assets::StaticVertex, normal)},
            {2, 0, VK_FORMAT_R32G32_SFLOAT,
             offsetof(stellar::assets::StaticVertex, uv0)},
            {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
             offsetof(stellar::assets::StaticVertex, tangent)},
            {4, 0, VK_FORMAT_R32G32_SFLOAT,
             offsetof(stellar::assets::StaticVertex, uv1)},
            {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
             offsetof(stellar::assets::StaticVertex, color)},
        }};
        const VkPipelineVertexInputStateCreateInfo vertex_input_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &binding_description,
            .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size()),
            .pVertexAttributeDescriptions = attributes.data(),
        };
        const VkPipelineInputAssemblyStateCreateInfo input_assembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };
        const VkViewport viewport{
            .x = 0.0F,
            .y = 0.0F,
            .width = static_cast<float>(swapchain_extent.width),
            .height = static_cast<float>(swapchain_extent.height),
            .minDepth = 0.0F,
            .maxDepth = 1.0F,
        };
        const VkRect2D scissor{{0, 0}, swapchain_extent};
        const VkPipelineViewportStateCreateInfo viewport_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor,
        };
        const VkPipelineRasterizationStateCreateInfo rasterizer{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = key.double_sided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.0F,
        };
        const VkPipelineMultisampleStateCreateInfo multisampling{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };
        const VkPipelineDepthStencilStateCreateInfo depth_stencil{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
        };
        VkPipelineColorBlendAttachmentState color_blend_attachment{
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        if (key.alpha_blend) {
            color_blend_attachment.blendEnable = VK_TRUE;
            color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
            color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }
        const VkPipelineColorBlendStateCreateInfo color_blending{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &color_blend_attachment,
        };
        const VkGraphicsPipelineCreateInfo pipeline_info{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = shader_stages,
            .pVertexInputState = &vertex_input_info,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depth_stencil,
            .pColorBlendState = &color_blending,
            .layout = pipeline_layout,
            .renderPass = key.readback_target ? readback_render_pass : render_pass,
            .subpass = 0,
        };
        VkPipeline pipeline = VK_NULL_HANDLE;
        const VkResult result = vkCreateGraphicsPipelines(
            device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
        vkDestroyShaderModule(device, *fragment_module, nullptr);
        vkDestroyShaderModule(device, *vertex_module, nullptr);
        if (result != VK_SUCCESS) {
            return std::unexpected(vk_error("Vulkan graphics-pipeline creation", result));
        }
        pipelines.emplace(key, PipelineRecord{.pipeline = pipeline});
        return pipeline;
    }

    void cleanup_swapchain() noexcept {
        if (device == VK_NULL_HANDLE) {
            return;
        }
        if (readback_framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, readback_framebuffer, nullptr);
            readback_framebuffer = VK_NULL_HANDLE;
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
        if (readback_render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, readback_render_pass, nullptr);
            readback_render_pass = VK_NULL_HANDLE;
        }
        if (render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, render_pass, nullptr);
            render_pass = VK_NULL_HANDLE;
        }
        destroy_texture_record(device, readback_color);
        for (auto image_view : swapchain_image_views) {
            vkDestroyImageView(device, image_view, nullptr);
        }
        swapchain_image_views.clear();
        destroy_texture_record(device, depth_buffer);
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
            for (auto& [key, pipeline] : pipelines) {
                if (pipeline.pipeline != VK_NULL_HANDLE) {
                    vkDestroyPipeline(device, pipeline.pipeline, nullptr);
                }
            }
            pipelines.clear();
            if (pipeline_layout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
            }
            if (draw_uniform_mapped != nullptr) {
                vkUnmapMemory(device, draw_uniform_buffer.memory);
                draw_uniform_mapped = nullptr;
            }
            destroy_buffer(device, draw_uniform_buffer);
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

    std::array<VkDescriptorSetLayoutBinding, kMaterialDescriptorBindingCount> bindings{};
    for (std::uint32_t binding = 0; binding < kMaterialTextureBindingCount; ++binding) {
        bindings[binding] = VkDescriptorSetLayoutBinding{
            .binding = binding,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        };
    }
    bindings[kDrawUniformBinding] = VkDescriptorSetLayoutBinding{
        .binding = kDrawUniformBinding,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    };
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
    const std::array<VkDescriptorPoolSize, 2> pool_sizes{{
        {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         .descriptorCount = kMaterialDescriptorSetCapacity * kMaterialTextureBindingCount},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
         .descriptorCount = kMaterialDescriptorSetCapacity},
    }};
    const VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = kMaterialDescriptorSetCapacity,
        .poolSizeCount = static_cast<std::uint32_t>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };
    result = vkCreateDescriptorPool(
        impl_->device, &pool_info, nullptr, &impl_->material_descriptor_pool);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan material descriptor-pool creation", result));
    }

    const VkPipelineLayoutCreateInfo pipeline_layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &impl_->material_descriptor_set_layout,
    };
    result = vkCreatePipelineLayout(
        impl_->device, &pipeline_layout_info, nullptr, &impl_->pipeline_layout);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan pipeline-layout creation", result));
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(impl_->physical_device, &properties);
    impl_->draw_uniform_stride = align_up(
        sizeof(DrawUniforms), properties.limits.minUniformBufferOffsetAlignment);
    auto draw_uniform_buffer = create_buffer(
        impl_->physical_device, impl_->device,
        impl_->draw_uniform_stride * kDrawUniformCapacity,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!draw_uniform_buffer) {
        return std::unexpected(draw_uniform_buffer.error());
    }
    impl_->draw_uniform_buffer = *draw_uniform_buffer;
    result = vkMapMemory(impl_->device, impl_->draw_uniform_buffer.memory, 0,
                         impl_->draw_uniform_buffer.size, 0,
                         &impl_->draw_uniform_mapped);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk_error("Vulkan draw-uniform buffer map", result));
    }

    if (auto fallbacks = impl_->create_fallback_textures(); !fallbacks) {
        return std::unexpected(fallbacks.error());
    }
    if (auto fallback_material = impl_->create_fallback_material(); !fallback_material) {
        return std::unexpected(fallback_material.error());
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
    impl_->swapchain_readback_supported =
        (support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0 &&
        supports_readback_format(surface_format.format);
    const bool swapchain_transfer_dst_supported =
        (support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0;
    const VkImageUsageFlags swapchain_usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        (impl_->swapchain_readback_supported ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0U) |
        (!impl_->swapchain_readback_supported && swapchain_transfer_dst_supported
             ? VK_IMAGE_USAGE_TRANSFER_DST_BIT
             : 0U);

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
        .imageUsage = swapchain_usage,
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

    auto depth_format = choose_depth_format(impl_->physical_device);
    if (!depth_format) {
        return std::unexpected(depth_format.error());
    }
    auto depth_buffer = create_image(
        impl_->physical_device, impl_->device, impl_->swapchain_extent.width,
        impl_->swapchain_extent.height, *depth_format,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    if (!depth_buffer) {
        return std::unexpected(depth_buffer.error());
    }
    auto depth_view = create_image_view(
        impl_->device, depth_buffer->image, *depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
    if (!depth_view) {
        destroy_texture_record(impl_->device, *depth_buffer);
        return std::unexpected(depth_view.error());
    }
    depth_buffer->image_view = *depth_view;
    impl_->depth_buffer = *depth_buffer;

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
    const VkAttachmentDescription depth_attachment{
        .format = *depth_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    const VkAttachmentReference color_attachment_ref{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    const VkAttachmentReference depth_attachment_ref{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    const VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
        .pDepthStencilAttachment = &depth_attachment_ref,
    };
    const VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };
    const std::array<VkAttachmentDescription, 2> attachments{
        color_attachment, depth_attachment};
    const VkRenderPassCreateInfo render_pass_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<std::uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
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

    if (!impl_->swapchain_readback_supported && swapchain_transfer_dst_supported &&
        supports_readback_format(impl_->swapchain_format)) {
        auto readback_color = create_image(
            impl_->physical_device, impl_->device, impl_->swapchain_extent.width,
            impl_->swapchain_extent.height, impl_->swapchain_format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        if (readback_color) {
            auto readback_view = create_image_view(
                impl_->device, readback_color->image, impl_->swapchain_format);
            if (readback_view) {
                readback_color->image_view = *readback_view;
                impl_->readback_color = *readback_color;
                impl_->readback_color_format = impl_->swapchain_format;

                const VkAttachmentDescription readback_color_attachment{
                    .format = impl_->readback_color_format,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                };
                const std::array<VkAttachmentDescription, 2> readback_attachments{
                    readback_color_attachment, depth_attachment};
                const VkRenderPassCreateInfo readback_render_pass_info{
                    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                    .attachmentCount =
                        static_cast<std::uint32_t>(readback_attachments.size()),
                    .pAttachments = readback_attachments.data(),
                    .subpassCount = 1,
                    .pSubpasses = &subpass,
                    .dependencyCount = 1,
                    .pDependencies = &dependency,
                };
                result = vkCreateRenderPass(
                    impl_->device, &readback_render_pass_info, nullptr,
                    &impl_->readback_render_pass);
                if (result == VK_SUCCESS) {
                    const std::array<VkImageView, 2> readback_framebuffer_attachments{
                        impl_->readback_color.image_view, impl_->depth_buffer.image_view};
                    const VkFramebufferCreateInfo readback_framebuffer_info{
                        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                        .renderPass = impl_->readback_render_pass,
                        .attachmentCount = static_cast<std::uint32_t>(
                            readback_framebuffer_attachments.size()),
                        .pAttachments = readback_framebuffer_attachments.data(),
                        .width = impl_->swapchain_extent.width,
                        .height = impl_->swapchain_extent.height,
                        .layers = 1,
                    };
                    result = vkCreateFramebuffer(
                        impl_->device, &readback_framebuffer_info, nullptr,
                        &impl_->readback_framebuffer);
                    if (result != VK_SUCCESS) {
                        vkDestroyRenderPass(
                            impl_->device, impl_->readback_render_pass, nullptr);
                        impl_->readback_render_pass = VK_NULL_HANDLE;
                    }
                }
            }
            if (impl_->readback_framebuffer == VK_NULL_HANDLE) {
                destroy_texture_record(impl_->device, *readback_color);
                impl_->readback_color = {};
                impl_->readback_color_format = VK_FORMAT_UNDEFINED;
            }
        }
    }

    impl_->framebuffers.reserve(impl_->swapchain_image_views.size());
    for (auto image_view : impl_->swapchain_image_views) {
        const std::array<VkImageView, 2> attachments{
            image_view, impl_->depth_buffer.image_view};
        const VkFramebufferCreateInfo framebuffer_info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = impl_->render_pass,
            .attachmentCount = static_cast<std::uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
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

    return impl_->create_material_record(material);
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

    impl_->draw_uniform_index = 0;
    impl_->rendering_to_readback_target =
        impl_->readback_requested && !impl_->swapchain_readback_supported &&
        impl_->readback_framebuffer != VK_NULL_HANDLE;
    const VkCommandBuffer command_buffer = impl_->command_buffers[impl_->image_index];
    vkResetCommandBuffer(command_buffer, 0);
    const VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        return;
    }

    const std::array<VkClearValue, 2> clear_values{{
        {.color = {{0.0F, 0.0F, 0.0F, 1.0F}}},
        {.depthStencil = {1.0F, 0}},
    }};
    const VkRenderPassBeginInfo render_pass_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = impl_->rendering_to_readback_target ? impl_->readback_render_pass
                                                          : impl_->render_pass,
        .framebuffer = impl_->rendering_to_readback_target
                           ? impl_->readback_framebuffer
                           : impl_->framebuffers[impl_->image_index],
        .renderArea = {{0, 0}, impl_->swapchain_extent},
        .clearValueCount = static_cast<std::uint32_t>(clear_values.size()),
        .pClearValues = clear_values.data(),
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    impl_->frame_started = true;
}

void VulkanGraphicsDevice::draw_mesh(MeshHandle mesh,
                                     std::span<const MeshPrimitiveDrawCommand> commands,
                                     const MeshDrawTransforms& transforms) noexcept {
    if (!impl_->initialized || !impl_->frame_started) {
        return;
    }
    auto mesh_record = impl_->meshes.find(mesh.value);
    if (mesh_record == impl_->meshes.end()) {
        return;
    }

    const VkCommandBuffer command_buffer = impl_->command_buffers[impl_->image_index];
    for (const auto& command : commands) {
        if (command.primitive_index >= mesh_record->second.primitives.size()) {
            continue;
        }
        auto material = impl_->materials.find(command.material.value);
        if (material == impl_->materials.end()) {
            material = impl_->materials.find(impl_->fallback_material.value);
        }
        if (material == impl_->materials.end()) {
            continue;
        }
        const auto& upload = material->second.upload;
        const bool alpha_blend =
            upload.material.alpha_mode == stellar::assets::AlphaMode::kBlend;
        const PipelineKey key{.alpha_blend = alpha_blend,
                              .double_sided = upload.material.double_sided,
                              .readback_target = impl_->rendering_to_readback_target};
        auto pipeline = impl_->pipeline_for(key);
        if (!pipeline) {
            continue;
        }

        const auto& primitive = mesh_record->second.primitives[command.primitive_index];
        const bool has_base_color = binding_uses_supported_texcoord(upload.base_color_texture);
        const bool has_lightmap = binding_uses_supported_texcoord(upload.lightmap_texture);
        const bool has_normal =
            primitive.has_tangents && binding_uses_supported_texcoord(upload.normal_texture);
        const bool has_metallic_roughness =
            binding_uses_supported_texcoord(upload.metallic_roughness_texture);
        const bool has_occlusion = binding_uses_supported_texcoord(upload.occlusion_texture);
        const bool has_emissive = binding_uses_supported_texcoord(upload.emissive_texture);
        const bool has_specular = binding_uses_supported_texcoord(upload.specular_texture);

        DrawUniforms uniforms{};
        uniforms.mvp = transforms.mvp;
        uniforms.world = transforms.world;
        uniforms.normal_column0 = {transforms.normal[0], transforms.normal[1],
                                   transforms.normal[2], 0.0F};
        uniforms.normal_column1 = {transforms.normal[3], transforms.normal[4],
                                   transforms.normal[5], 0.0F};
        uniforms.normal_column2 = {transforms.normal[6], transforms.normal[7],
                                   transforms.normal[8], 0.0F};
        uniforms.base_color = upload.material.base_color_factor;
        uniforms.emissive_factor = {upload.material.emissive_factor[0],
                                    upload.material.emissive_factor[1],
                                    upload.material.emissive_factor[2], 0.0F};
        uniforms.global_light_color = {upload.global_light_color[0],
                                       upload.global_light_color[1],
                                       upload.global_light_color[2], 0.0F};
        uniforms.camera_world_position = {
            transforms.camera_world_position[0], transforms.camera_world_position[1],
            transforms.camera_world_position[2], 0.0F};
        uniforms.texture_transform0 = {
            transform0_for(upload.base_color_texture),
            transform0_for(upload.normal_texture),
            transform0_for(upload.metallic_roughness_texture),
            transform0_for(upload.occlusion_texture),
            transform0_for(upload.emissive_texture),
            transform0_for(upload.specular_texture),
        };
        uniforms.texture_transform1 = {
            transform1_for(upload.base_color_texture),
            transform1_for(upload.normal_texture),
            transform1_for(upload.metallic_roughness_texture),
            transform1_for(upload.occlusion_texture),
            transform1_for(upload.emissive_texture),
            transform1_for(upload.specular_texture),
        };
        uniforms.material_factors0 = {
            upload.material.metallic_factor, upload.material.roughness_factor,
            upload.material.normal_scale, upload.material.normal_light_strength};
        uniforms.material_factors1 = {
            upload.material.specular_factor, upload.material.specular_power,
            upload.material.occlusion_strength, upload.lightmap_multiplier};
        uniforms.material_factors2 = {upload.lightmap_min, upload.global_light_intensity,
                                      upload.material.alpha_cutoff, 0.0F};
        uniforms.material_flags0 = {has_base_color ? 1U : 0U, has_lightmap ? 1U : 0U,
                                    has_normal ? 1U : 0U,
                                    has_metallic_roughness ? 1U : 0U};
        uniforms.material_flags1 = {has_occlusion ? 1U : 0U, has_emissive ? 1U : 0U,
                                    has_specular ? 1U : 0U,
                                    primitive.has_colors ? 1U : 0U};
        uniforms.material_flags2 = {
            transforms.has_camera_world_position ? 1U : 0U,
            has_base_color ? upload.base_color_texture->texcoord_set : 0U,
            has_lightmap ? upload.lightmap_texture->texcoord_set : 1U,
            has_normal ? upload.normal_texture->texcoord_set : 0U};
        uniforms.material_flags3 = {
            has_metallic_roughness ? upload.metallic_roughness_texture->texcoord_set : 0U,
            has_occlusion ? upload.occlusion_texture->texcoord_set : 0U,
            has_emissive ? upload.emissive_texture->texcoord_set : 0U,
            has_specular ? upload.specular_texture->texcoord_set : 0U};
        uniforms.material_flags4 = {
            alpha_mode_value(upload.material.alpha_mode),
            upload.material.unlit ? 1U : 0U, 0U, 0U};
        const auto dynamic_offset = impl_->write_draw_uniforms(uniforms);
        if (!dynamic_offset.has_value()) {
            continue;
        }

        const VkDeviceSize offset = 0;
        VkBuffer vertex_buffer = primitive.vertex_buffer.buffer;
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                impl_->pipeline_layout, 0, 1,
                                &material->second.descriptor_set, 1,
                                &*dynamic_offset);
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, &offset);
        vkCmdBindIndexBuffer(command_buffer, primitive.index_buffer.buffer, 0,
                             VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(command_buffer, primitive.index_count, 1, 0, 0, 0);
    }
}

void VulkanGraphicsDevice::end_frame() noexcept {
    if (!impl_->initialized || !impl_->frame_started) {
        return;
    }
    const VkCommandBuffer command_buffer = impl_->command_buffers[impl_->image_index];
    vkCmdEndRenderPass(command_buffer);

    const bool readback_requested = impl_->readback_requested;
    BufferRecord readback_buffer;
    bool has_readback_buffer = false;
    int readback_width = 0;
    int readback_height = 0;
    const bool readback_from_offscreen = impl_->rendering_to_readback_target;
    if (readback_requested) {
        readback_width = static_cast<int>(impl_->swapchain_extent.width);
        readback_height = static_cast<int>(impl_->swapchain_extent.height);
        if (readback_width <= 0 || readback_height <= 0) {
            impl_->last_readback_error = "Vulkan readback failed: drawable size is empty";
        } else if (!impl_->swapchain_readback_supported && !readback_from_offscreen) {
            impl_->last_readback_error =
                "Vulkan readback failed: no transfer-source readback target is available";
        } else {
            const VkDeviceSize readback_size =
                static_cast<VkDeviceSize>(readback_width) *
                static_cast<VkDeviceSize>(readback_height) * 4U;
            auto buffer = create_buffer(
                impl_->physical_device, impl_->device, readback_size,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if (!buffer) {
                impl_->last_readback_error = buffer.error().message;
            } else {
                readback_buffer = *buffer;
                has_readback_buffer = true;

                VkImage source_image = impl_->swapchain_images[impl_->image_index];
                VkImageLayout transfer_old_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                VkPipelineStageFlags transfer_source_stage =
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                VkAccessFlags transfer_source_access =
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                if (readback_from_offscreen) {
                    source_image = impl_->readback_color.image;
                    transfer_old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                }

                VkImageMemoryBarrier to_transfer_source{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask = transfer_source_access,
                    .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                    .oldLayout = transfer_old_layout,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = source_image,
                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                };
                vkCmdPipelineBarrier(command_buffer, transfer_source_stage,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                                     nullptr, 1, &to_transfer_source);
                const VkBufferImageCopy region{
                    .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                    .imageExtent = {impl_->swapchain_extent.width,
                                    impl_->swapchain_extent.height, 1},
                };
                if (readback_from_offscreen) {
                    VkImageMemoryBarrier swapchain_to_transfer{
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        .srcAccessMask = 0,
                        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image = impl_->swapchain_images[impl_->image_index],
                        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                    };
                    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                                         nullptr, 1, &swapchain_to_transfer);
                    const VkImageCopy image_copy{
                        .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                        .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                        .extent = {impl_->swapchain_extent.width,
                                   impl_->swapchain_extent.height, 1},
                    };
                    vkCmdCopyImage(command_buffer, source_image,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   impl_->swapchain_images[impl_->image_index],
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);
                }
                vkCmdCopyImageToBuffer(command_buffer, source_image,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       readback_buffer.buffer, 1, &region);
                VkImageMemoryBarrier source_after_transfer{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                    .dstAccessMask = 0,
                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .newLayout = readback_from_offscreen
                                     ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                     : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = source_image,
                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                };
                vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                                     nullptr, 0, nullptr, 1, &source_after_transfer);
                if (readback_from_offscreen) {
                    VkImageMemoryBarrier swapchain_to_present{
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .dstAccessMask = 0,
                        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image = impl_->swapchain_images[impl_->image_index],
                        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                    };
                    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                                         nullptr, 0, nullptr, 1, &swapchain_to_present);
                }
            }
        }
    }

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        impl_->frame_started = false;
        impl_->rendering_to_readback_target = false;
        if (has_readback_buffer) {
            destroy_buffer(impl_->device, readback_buffer);
        }
        if (readback_requested) {
            impl_->last_readback_error = "Vulkan readback failed: command buffer end failed";
            impl_->readback_requested = false;
        }
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
        if (has_readback_buffer) {
            destroy_buffer(impl_->device, readback_buffer);
        }
        if (readback_requested) {
            impl_->last_readback_error = "Vulkan readback failed: queue submit failed";
            impl_->readback_requested = false;
        }
        impl_->rendering_to_readback_target = false;
        return;
    }
    if (readback_requested &&
        vkWaitForFences(
            impl_->device, kMaxFramesInFlight, &impl_->in_flight, VK_TRUE, UINT64_MAX) !=
            VK_SUCCESS) {
        impl_->last_readback_error = "Vulkan readback failed: frame wait failed";
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

    if (readback_requested && !impl_->last_readback_error.has_value() &&
        has_readback_buffer) {
        void* mapped = nullptr;
        const VkResult map_result = vkMapMemory(
            impl_->device, readback_buffer.memory, 0, readback_buffer.size, 0, &mapped);
        if (map_result != VK_SUCCESS) {
            impl_->last_readback_error =
                vk_result_message("Vulkan readback buffer map", map_result);
        } else {
            stellar::graphics::FrameReadback readback;
            readback.width = readback_width;
            readback.height = readback_height;
            const auto* source = static_cast<const std::uint8_t*>(mapped);
            copy_swapchain_rgba(
                readback_from_offscreen ? impl_->readback_color_format
                                        : impl_->swapchain_format,
                readback_width, readback_height,
                std::span<const std::uint8_t>(
                    source, static_cast<std::size_t>(readback_width) *
                                static_cast<std::size_t>(readback_height) * 4U),
                readback.rgba_pixels);
            vkUnmapMemory(impl_->device, readback_buffer.memory);
            impl_->last_readback = std::move(readback);
        }
    }
    if (has_readback_buffer) {
        destroy_buffer(impl_->device, readback_buffer);
    }
    impl_->readback_requested = false;
    impl_->rendering_to_readback_target = false;
    impl_->frame_started = false;
}

void VulkanGraphicsDevice::request_frame_readback() noexcept {
    if (!impl_->initialized) {
        return;
    }
    impl_->readback_requested = true;
    impl_->last_readback.reset();
    impl_->last_readback_error.reset();
}

std::expected<std::optional<stellar::graphics::FrameReadback>, stellar::platform::Error>
VulkanGraphicsDevice::take_frame_readback() noexcept {
    if (!impl_->initialized) {
        return std::unexpected(error("Vulkan readback failed: device is not initialized"));
    }
    if (impl_->last_readback_error.has_value()) {
        return std::unexpected(error(std::move(*impl_->last_readback_error)));
    }
    std::optional<stellar::graphics::FrameReadback> result;
    if (impl_->last_readback.has_value()) {
        result = std::move(impl_->last_readback);
        impl_->last_readback.reset();
    }
    return result;
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
    if (material == impl_->fallback_material) {
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
