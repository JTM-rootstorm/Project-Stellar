#include "VulkanGraphicsDevicePrivate.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace stellar::graphics::vulkan {

namespace {

bool valid_image_format(stellar::assets::ImageFormat format) noexcept {
    switch (format) {
        case stellar::assets::ImageFormat::kR8G8B8:
        case stellar::assets::ImageFormat::kR8G8B8A8:
            return true;
        case stellar::assets::ImageFormat::kUnknown:
        default:
            return false;
    }
}

std::expected<VkFormat, stellar::platform::Error> texture_format_from_image(
    stellar::assets::ImageFormat format,
    stellar::assets::TextureColorSpace color_space) {
    switch (format) {
        case stellar::assets::ImageFormat::kR8G8B8:
        case stellar::assets::ImageFormat::kR8G8B8A8:
            return color_space == stellar::assets::TextureColorSpace::kSrgb
                ? VK_FORMAT_R8G8B8A8_SRGB
                : VK_FORMAT_R8G8B8A8_UNORM;
        case stellar::assets::ImageFormat::kUnknown:
        default:
            return std::unexpected(stellar::platform::Error("Unsupported image format"));
    }
}

std::expected<std::vector<std::uint8_t>, stellar::platform::Error> image_pixels_as_rgba(
    const stellar::assets::ImageAsset& image) {
    const std::uint64_t texel_count = static_cast<std::uint64_t>(image.width) * image.height;
    const std::uint64_t source_channels = image.format == stellar::assets::ImageFormat::kR8G8B8
        ? 3U
        : image.format == stellar::assets::ImageFormat::kR8G8B8A8 ? 4U : 0U;
    if (source_channels == 0U || texel_count == 0U) {
        return std::unexpected(stellar::platform::Error("Unsupported image format"));
    }
    if (image.pixels.size() != texel_count * source_channels) {
        return std::unexpected(stellar::platform::Error("Image pixel data size does not match format"));
    }

    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(texel_count * 4U));
    if (source_channels == 4U) {
        std::copy(image.pixels.begin(), image.pixels.end(), rgba.begin());
        return rgba;
    }

    for (std::uint64_t texel = 0; texel < texel_count; ++texel) {
        const std::uint64_t source = texel * 3U;
        const std::uint64_t destination = texel * 4U;
        rgba[static_cast<std::size_t>(destination + 0U)] = image.pixels[static_cast<std::size_t>(source + 0U)];
        rgba[static_cast<std::size_t>(destination + 1U)] = image.pixels[static_cast<std::size_t>(source + 1U)];
        rgba[static_cast<std::size_t>(destination + 2U)] = image.pixels[static_cast<std::size_t>(source + 2U)];
        rgba[static_cast<std::size_t>(destination + 3U)] = 255U;
    }
    return rgba;
}

std::uint32_t texture_mip_level_count(std::uint32_t width, std::uint32_t height) noexcept {
    std::uint32_t levels = 1;
    std::uint32_t dimension = std::max(width, height);
    while (dimension > 1U) {
        dimension /= 2U;
        ++levels;
    }
    return levels;
}

VkFilter to_vk_filter(stellar::assets::TextureFilter filter, VkFilter fallback) noexcept {
    switch (filter) {
        case stellar::assets::TextureFilter::kNearest:
        case stellar::assets::TextureFilter::kNearestMipmapNearest:
        case stellar::assets::TextureFilter::kNearestMipmapLinear:
            return VK_FILTER_NEAREST;
        case stellar::assets::TextureFilter::kLinear:
        case stellar::assets::TextureFilter::kLinearMipmapNearest:
        case stellar::assets::TextureFilter::kLinearMipmapLinear:
            return VK_FILTER_LINEAR;
        case stellar::assets::TextureFilter::kUnspecified:
        default:
            return fallback;
    }
}

VkSamplerMipmapMode to_vk_mipmap_mode(stellar::assets::TextureFilter filter) noexcept {
    switch (filter) {
        case stellar::assets::TextureFilter::kNearestMipmapNearest:
        case stellar::assets::TextureFilter::kLinearMipmapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case stellar::assets::TextureFilter::kNearestMipmapLinear:
        case stellar::assets::TextureFilter::kLinearMipmapLinear:
        case stellar::assets::TextureFilter::kUnspecified:
        case stellar::assets::TextureFilter::kNearest:
        case stellar::assets::TextureFilter::kLinear:
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

VkSamplerAddressMode to_vk_address_mode(stellar::assets::TextureWrapMode mode) noexcept {
    switch (mode) {
        case stellar::assets::TextureWrapMode::kClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case stellar::assets::TextureWrapMode::kMirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case stellar::assets::TextureWrapMode::kRepeat:
        default:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

float to_vk_max_lod(stellar::assets::TextureFilter min_filter, std::uint32_t mip_levels) noexcept {
    switch (min_filter) {
        case stellar::assets::TextureFilter::kNearest:
        case stellar::assets::TextureFilter::kLinear:
            return 0.0F;
        case stellar::assets::TextureFilter::kUnspecified:
        case stellar::assets::TextureFilter::kNearestMipmapNearest:
        case stellar::assets::TextureFilter::kLinearMipmapNearest:
        case stellar::assets::TextureFilter::kNearestMipmapLinear:
        case stellar::assets::TextureFilter::kLinearMipmapLinear:
        default:
            return static_cast<float>(mip_levels > 0U ? mip_levels - 1U : 0U);
    }
}

} // namespace

std::expected<MeshHandle, stellar::platform::Error>
VulkanGraphicsDevice::create_mesh(const stellar::assets::MeshAsset& mesh) {
    if (!initialized_) {
        return std::unexpected(stellar::platform::Error("Vulkan graphics device is not initialized"));
    }
    if (mesh.primitives.empty()) {
        return std::unexpected(stellar::platform::Error("Mesh has no primitives"));
    }

    MeshRecord record;
    record.primitives.reserve(mesh.primitives.size());
    for (const auto& primitive : mesh.primitives) {
        if (primitive.topology != stellar::assets::PrimitiveTopology::kTriangles) {
            return std::unexpected(stellar::platform::Error("Unsupported mesh topology"));
        }
        if (primitive.vertices.empty() || primitive.indices.empty()) {
            return std::unexpected(stellar::platform::Error("Mesh primitive is empty"));
        }
        const auto invalid_index = std::ranges::find_if(primitive.indices, [&](std::uint32_t index) {
            return index >= primitive.vertices.size();
        });
        if (invalid_index != primitive.indices.end()) {
            destroy_mesh_record(record);
            return std::unexpected(stellar::platform::Error("Mesh primitive index is out of range"));
        }

        MeshPrimitiveRecord primitive_record{
            .vertex_count = primitive.vertices.size(),
            .index_count = primitive.indices.size(),
            .has_tangents = primitive.has_tangents,
            .has_colors = primitive.has_colors,
            .has_skinning = primitive.has_skinning,
        };

        primitive_record.vertex_buffer_size = static_cast<VkDeviceSize>(
            primitive.vertices.size() * sizeof(stellar::assets::StaticVertex));
        primitive_record.index_buffer_size = static_cast<VkDeviceSize>(
            primitive.indices.size() * sizeof(std::uint32_t));

        if (auto result = create_buffer(primitive_record.vertex_buffer_size,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        primitive_record.vertex_buffer,
                                        primitive_record.vertex_memory);
            !result) {
            destroy_mesh_record(record);
            return std::unexpected(result.error());
        }
        if (auto result = upload_to_buffer(primitive_record.vertex_memory, primitive.vertices.data(),
                                           primitive_record.vertex_buffer_size);
            !result) {
            destroy_buffer(primitive_record.vertex_buffer, primitive_record.vertex_memory);
            destroy_mesh_record(record);
            return std::unexpected(result.error());
        }

        if (auto result = create_buffer(primitive_record.index_buffer_size,
                                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        primitive_record.index_buffer,
                                        primitive_record.index_memory);
            !result) {
            destroy_buffer(primitive_record.vertex_buffer, primitive_record.vertex_memory);
            destroy_mesh_record(record);
            return std::unexpected(result.error());
        }
        if (auto result = upload_to_buffer(primitive_record.index_memory, primitive.indices.data(),
                                           primitive_record.index_buffer_size);
            !result) {
            destroy_buffer(primitive_record.index_buffer, primitive_record.index_memory);
            destroy_buffer(primitive_record.vertex_buffer, primitive_record.vertex_memory);
            destroy_mesh_record(record);
            return std::unexpected(result.error());
        }

        record.primitives.push_back(primitive_record);
    }

    const std::uint64_t handle_value = allocate_handle();
    meshes_.emplace(handle_value, std::move(record));
    return MeshHandle{handle_value};
}

std::expected<TextureHandle, stellar::platform::Error>
VulkanGraphicsDevice::create_texture(const TextureUpload& texture) {
    if (!initialized_) {
        return std::unexpected(stellar::platform::Error("Vulkan graphics device is not initialized"));
    }

    const auto& image = texture.image;
    if (image.width == 0 || image.height == 0 || image.pixels.empty()) {
        return std::unexpected(stellar::platform::Error("Image asset is empty"));
    }
    auto rgba_pixels = image_pixels_as_rgba(image);
    if (!rgba_pixels) {
        return std::unexpected(rgba_pixels.error());
    }
    auto image_format = texture_format_from_image(image.format, texture.color_space);
    if (!image_format) {
        return std::unexpected(image_format.error());
    }

    VkFormatProperties format_properties{};
    vkGetPhysicalDeviceFormatProperties(physical_device_, *image_format, &format_properties);
    const bool can_generate_mipmaps =
        (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) !=
        0;
    const std::uint32_t mip_levels = can_generate_mipmaps
        ? texture_mip_level_count(image.width, image.height)
        : 1U;
    const VkDeviceSize image_size = static_cast<VkDeviceSize>(rgba_pixels->size());

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;
    if (auto result = create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    staging_buffer, staging_memory);
        !result) {
        return std::unexpected(result.error());
    }
    if (auto result = upload_to_buffer(staging_memory, rgba_pixels->data(), image_size); !result) {
        destroy_buffer(staging_buffer, staging_memory);
        return std::unexpected(result.error());
    }

    TextureRecord record{.width = image.width,
                         .height = image.height,
                         .mip_levels = mip_levels,
                         .format = *image_format,
                         .color_space = texture.color_space};
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (mip_levels > 1U) {
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (auto result = create_image(image.width, image.height, mip_levels, *image_format,
                                   VK_IMAGE_TILING_OPTIMAL, usage,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, record.image,
                                   record.memory);
        !result) {
        destroy_buffer(staging_buffer, staging_memory);
        return std::unexpected(result.error());
    }
    if (auto result = transition_image_layout(record.image, *image_format, mip_levels,
                                              VK_IMAGE_LAYOUT_UNDEFINED,
                                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        !result) {
        destroy_buffer(staging_buffer, staging_memory);
        destroy_texture_record(record);
        return std::unexpected(result.error());
    }
    if (auto result = copy_buffer_to_image(staging_buffer, record.image, image.width, image.height);
        !result) {
        destroy_buffer(staging_buffer, staging_memory);
        destroy_texture_record(record);
        return std::unexpected(result.error());
    }
    destroy_buffer(staging_buffer, staging_memory);

    if (mip_levels > 1U) {
        if (auto result = generate_texture_mipmaps(record.image, *image_format, image.width,
                                                  image.height, mip_levels);
            !result) {
            destroy_texture_record(record);
            return std::unexpected(result.error());
        }
    } else {
        if (auto result = transition_image_layout(record.image, *image_format, mip_levels,
                                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            !result) {
            destroy_texture_record(record);
            return std::unexpected(result.error());
        }
    }

    auto image_view = create_texture_image_view(record.image, *image_format, mip_levels);
    if (!image_view) {
        destroy_texture_record(record);
        return std::unexpected(image_view.error());
    }
    record.view = *image_view;

    const stellar::assets::SamplerAsset default_sampler{};
    auto sampler = create_sampler(default_sampler, mip_levels);
    if (!sampler) {
        destroy_texture_record(record);
        return std::unexpected(sampler.error());
    }
    record.sampler = *sampler;

    const std::uint64_t handle_value = allocate_handle();
    textures_.emplace(handle_value, std::move(record));
    return TextureHandle{handle_value};
}

std::expected<MaterialHandle, stellar::platform::Error>
VulkanGraphicsDevice::create_material(const MaterialUpload& material) {
    if (!initialized_) {
        return std::unexpected(stellar::platform::Error("Vulkan graphics device is not initialized"));
    }

    MaterialRecord record{.upload = material};
    auto add_sampler = [&](const std::optional<MaterialTextureBinding>& binding)
        -> std::expected<void, stellar::platform::Error> {
        if (!binding.has_value()) {
            return {};
        }
        std::uint32_t mip_levels = 1;
        const auto texture_it = textures_.find(binding->texture.value);
        if (texture_it != textures_.end()) {
            mip_levels = texture_it->second.mip_levels;
        }
        auto sampler = create_sampler(binding->sampler, mip_levels);
        if (!sampler) {
            return std::unexpected(sampler.error());
        }
        record.samplers.push_back(*sampler);
        return {};
    };

    for (const auto* binding : {&material.base_color_texture,
                                &material.normal_texture,
                                &material.metallic_roughness_texture,
                                &material.occlusion_texture,
                                &material.emissive_texture}) {
        if (auto result = add_sampler(*binding); !result) {
            destroy_material_record(record);
            return std::unexpected(result.error());
        }
    }

    const std::uint64_t handle_value = allocate_handle();
    materials_.emplace(handle_value, std::move(record));
    return MaterialHandle{handle_value};
}

void VulkanGraphicsDevice::destroy_mesh(MeshHandle mesh) noexcept {
    auto it = meshes_.find(mesh.value);
    if (it == meshes_.end()) {
        return;
    }

    destroy_mesh_record(it->second);
    meshes_.erase(it);
}

void VulkanGraphicsDevice::destroy_texture(TextureHandle texture) noexcept {
    auto it = textures_.find(texture.value);
    if (it == textures_.end()) {
        return;
    }

    destroy_texture_record(it->second);
    textures_.erase(it);
}

void VulkanGraphicsDevice::destroy_material(MaterialHandle material) noexcept {
    auto it = materials_.find(material.value);
    if (it == materials_.end()) {
        return;
    }

    destroy_material_record(it->second);
    materials_.erase(it);
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::create_buffer(VkDeviceSize size,
                                    VkBufferUsageFlags usage,
                                    VkMemoryPropertyFlags properties,
                                    VkBuffer& buffer,
                                    VkDeviceMemory& memory) {
    const VkBufferCreateInfo buffer_create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkResult result = vkCreateBuffer(device_, &buffer_create_info, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkCreateBuffer", result));
    }

    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(device_, buffer, &memory_requirements);
    const auto memory_type_index =
        find_memory_type(memory_requirements.memoryTypeBits, properties);
    if (!memory_type_index) {
        destroy_buffer(buffer, memory);
        return std::unexpected(memory_type_index.error());
    }

    const VkMemoryAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = *memory_type_index,
    };
    result = vkAllocateMemory(device_, &allocate_info, nullptr, &memory);
    if (result != VK_SUCCESS) {
        destroy_buffer(buffer, memory);
        return std::unexpected(vulkan_error("vkAllocateMemory", result));
    }

    result = vkBindBufferMemory(device_, buffer, memory, 0);
    if (result != VK_SUCCESS) {
        destroy_buffer(buffer, memory);
        return std::unexpected(vulkan_error("vkBindBufferMemory", result));
    }

    return {};
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::upload_to_buffer(VkDeviceMemory memory, const void* data, VkDeviceSize size) {
    void* mapped_memory = nullptr;
    const VkResult result = vkMapMemory(device_, memory, 0, size, 0, &mapped_memory);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkMapMemory", result));
    }

    std::memcpy(mapped_memory, data, static_cast<std::size_t>(size));
    vkUnmapMemory(device_, memory);
    return {};
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::create_image(std::uint32_t width,
                                   std::uint32_t height,
                                   std::uint32_t mip_levels,
                                   VkFormat format,
                                   VkImageTiling tiling,
                                   VkImageUsageFlags usage,
                                   VkMemoryPropertyFlags properties,
                                   VkImage& image,
                                   VkDeviceMemory& memory) {
    const VkImageCreateInfo image_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = mip_levels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkResult result = vkCreateImage(device_, &image_create_info, nullptr, &image);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkCreateImage", result));
    }

    VkMemoryRequirements memory_requirements{};
    vkGetImageMemoryRequirements(device_, image, &memory_requirements);
    const auto memory_type_index =
        find_memory_type(memory_requirements.memoryTypeBits, properties);
    if (!memory_type_index) {
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(device_, image, nullptr);
            image = VK_NULL_HANDLE;
        }
        return std::unexpected(memory_type_index.error());
    }

    const VkMemoryAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = *memory_type_index,
    };
    result = vkAllocateMemory(device_, &allocate_info, nullptr, &memory);
    if (result != VK_SUCCESS) {
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(device_, image, nullptr);
            image = VK_NULL_HANDLE;
        }
        return std::unexpected(vulkan_error("vkAllocateMemory", result));
    }

    result = vkBindImageMemory(device_, image, memory, 0);
    if (result != VK_SUCCESS) {
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(device_, image, nullptr);
            image = VK_NULL_HANDLE;
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
        return std::unexpected(vulkan_error("vkBindImageMemory", result));
    }
    return {};
}

std::expected<VkCommandBuffer, stellar::platform::Error>
VulkanGraphicsDevice::begin_single_time_commands() {
    if (frames_.empty() || frames_[0].command_pool == VK_NULL_HANDLE) {
        return std::unexpected(stellar::platform::Error("No Vulkan command pool is available"));
    }

    const VkCommandBufferAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = frames_[0].command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkResult result = vkAllocateCommandBuffers(device_, &allocate_info, &command_buffer);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkAllocateCommandBuffers", result));
    }

    const VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, frames_[0].command_pool, 1, &command_buffer);
        return std::unexpected(vulkan_error("vkBeginCommandBuffer", result));
    }
    return command_buffer;
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::end_single_time_commands(VkCommandBuffer command_buffer) {
    VkResult result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, frames_[0].command_pool, 1, &command_buffer);
        return std::unexpected(vulkan_error("vkEndCommandBuffer", result));
    }

    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };
    result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
    if (result == VK_SUCCESS) {
        result = vkQueueWaitIdle(graphics_queue_);
    }
    vkFreeCommandBuffers(device_, frames_[0].command_pool, 1, &command_buffer);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("single-use Vulkan transfer", result));
    }
    return {};
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::transition_image_layout(VkImage image,
                                              VkFormat format,
                                              std::uint32_t mip_levels,
                                              VkImageLayout old_layout,
                                              VkImageLayout new_layout) {
    (void)format;
    auto command_buffer = begin_single_time_commands();
    if (!command_buffer) {
        return std::unexpected(command_buffer.error());
    }

    VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkAccessFlags source_access = 0;
    VkAccessFlags destination_access = VK_ACCESS_TRANSFER_WRITE_BIT;
    if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        source_access = VK_ACCESS_TRANSFER_WRITE_BIT;
        destination_access = VK_ACCESS_SHADER_READ_BIT;
    }

    const VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = source_access,
        .dstAccessMask = destination_access,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, 1},
    };
    vkCmdPipelineBarrier(*command_buffer, source_stage, destination_stage, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
    return end_single_time_commands(*command_buffer);
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::copy_buffer_to_image(VkBuffer buffer,
                                           VkImage image,
                                           std::uint32_t width,
                                           std::uint32_t height) {
    auto command_buffer = begin_single_time_commands();
    if (!command_buffer) {
        return std::unexpected(command_buffer.error());
    }

    const VkBufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1},
    };
    vkCmdCopyBufferToImage(*command_buffer, buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    return end_single_time_commands(*command_buffer);
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::generate_texture_mipmaps(VkImage image,
                                               VkFormat format,
                                               std::uint32_t width,
                                               std::uint32_t height,
                                               std::uint32_t mip_levels) {
    (void)format;
    auto command_buffer = begin_single_time_commands();
    if (!command_buffer) {
        return std::unexpected(command_buffer.error());
    }

    std::int32_t mip_width = static_cast<std::int32_t>(width);
    std::int32_t mip_height = static_cast<std::int32_t>(height);
    for (std::uint32_t level = 1; level < mip_levels; ++level) {
        VkImageMemoryBarrier barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1U, 1, 0, 1},
        };
        vkCmdPipelineBarrier(*command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &barrier);

        const std::int32_t next_width = mip_width > 1 ? mip_width / 2 : 1;
        const std::int32_t next_height = mip_height > 1 ? mip_height / 2 : 1;
        const VkImageBlit blit{
            .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1U, 0, 1},
            .srcOffsets = {{0, 0, 0}, {mip_width, mip_height, 1}},
            .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1},
            .dstOffsets = {{0, 0, 0}, {next_width, next_height, 1}},
        };
        vkCmdBlitImage(*command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(*command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &barrier);
        mip_width = next_width;
        mip_height = next_height;
    }

    const VkImageMemoryBarrier last_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mip_levels - 1U, 1, 0, 1},
    };
    vkCmdPipelineBarrier(*command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &last_barrier);
    return end_single_time_commands(*command_buffer);
}

std::expected<VkImageView, stellar::platform::Error>
VulkanGraphicsDevice::create_texture_image_view(VkImage image,
                                                VkFormat format,
                                                std::uint32_t mip_levels) {
    const VkImageViewCreateInfo image_view_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, 1},
    };
    VkImageView image_view = VK_NULL_HANDLE;
    const VkResult result = vkCreateImageView(device_, &image_view_create_info, nullptr,
                                              &image_view);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkCreateImageView", result));
    }
    return image_view;
}

std::expected<VkSampler, stellar::platform::Error>
VulkanGraphicsDevice::create_sampler(const stellar::assets::SamplerAsset& sampler,
                                     std::uint32_t mip_levels) {
    const VkSamplerCreateInfo sampler_create_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = to_vk_filter(sampler.mag_filter, VK_FILTER_LINEAR),
        .minFilter = to_vk_filter(sampler.min_filter, VK_FILTER_LINEAR),
        .mipmapMode = to_vk_mipmap_mode(sampler.min_filter),
        .addressModeU = to_vk_address_mode(sampler.wrap_s),
        .addressModeV = to_vk_address_mode(sampler.wrap_t),
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0F,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0F,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0F,
        .maxLod = to_vk_max_lod(sampler.min_filter, mip_levels),
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    VkSampler vk_sampler = VK_NULL_HANDLE;
    const VkResult result = vkCreateSampler(device_, &sampler_create_info, nullptr, &vk_sampler);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkCreateSampler", result));
    }
    return vk_sampler;
}

std::expected<std::uint32_t, stellar::platform::Error>
VulkanGraphicsDevice::find_memory_type(std::uint32_t type_filter,
                                       VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);

    for (std::uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
        const bool type_supported = (type_filter & (1U << index)) != 0;
        const bool properties_supported =
            (memory_properties.memoryTypes[index].propertyFlags & properties) == properties;
        if (type_supported && properties_supported) {
            return index;
        }
    }

    return std::unexpected(stellar::platform::Error("No compatible Vulkan memory type was found"));
}

void VulkanGraphicsDevice::destroy_buffer(VkBuffer& buffer, VkDeviceMemory& memory) noexcept {
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

void VulkanGraphicsDevice::destroy_mesh_record(MeshRecord& record) noexcept {
    for (MeshPrimitiveRecord& primitive : record.primitives) {
        destroy_buffer(primitive.vertex_buffer, primitive.vertex_memory);
        primitive.vertex_buffer_size = 0;
        destroy_buffer(primitive.index_buffer, primitive.index_memory);
        primitive.index_buffer_size = 0;
        primitive.vertex_count = 0;
        primitive.index_count = 0;
    }
    record.primitives.clear();
}

void VulkanGraphicsDevice::destroy_texture_record(TextureRecord& record) noexcept {
    if (record.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device_, record.sampler, nullptr);
        record.sampler = VK_NULL_HANDLE;
    }
    if (record.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, record.view, nullptr);
        record.view = VK_NULL_HANDLE;
    }
    if (record.image != VK_NULL_HANDLE) {
        vkDestroyImage(device_, record.image, nullptr);
        record.image = VK_NULL_HANDLE;
    }
    if (record.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, record.memory, nullptr);
        record.memory = VK_NULL_HANDLE;
    }
    record.width = 0;
    record.height = 0;
    record.mip_levels = 1;
    record.format = VK_FORMAT_UNDEFINED;
}

void VulkanGraphicsDevice::destroy_material_record(MaterialRecord& record) noexcept {
    for (VkSampler sampler : record.samplers) {
        if (sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device_, sampler, nullptr);
        }
    }
    record.samplers.clear();
}
} // namespace stellar::graphics::vulkan
