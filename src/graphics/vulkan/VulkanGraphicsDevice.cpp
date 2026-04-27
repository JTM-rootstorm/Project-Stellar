#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

namespace stellar::graphics::vulkan {

namespace {

constexpr std::size_t kFramesInFlight = 2;

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

stellar::platform::Error vulkan_error(const char* operation, VkResult result) {
    std::string message(operation);
    message += " failed with VkResult ";
    message += std::to_string(static_cast<int>(result));
    message += " (";
    switch (result) {
        case VK_SUCCESS:
            message += "VK_SUCCESS";
            break;
        case VK_NOT_READY:
            message += "VK_NOT_READY";
            break;
        case VK_TIMEOUT:
            message += "VK_TIMEOUT";
            break;
        case VK_EVENT_SET:
            message += "VK_EVENT_SET";
            break;
        case VK_EVENT_RESET:
            message += "VK_EVENT_RESET";
            break;
        case VK_INCOMPLETE:
            message += "VK_INCOMPLETE";
            break;
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            message += "VK_ERROR_OUT_OF_HOST_MEMORY";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            message += "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            break;
        case VK_ERROR_INITIALIZATION_FAILED:
            message += "VK_ERROR_INITIALIZATION_FAILED";
            break;
        case VK_ERROR_DEVICE_LOST:
            message += "VK_ERROR_DEVICE_LOST";
            break;
        case VK_ERROR_MEMORY_MAP_FAILED:
            message += "VK_ERROR_MEMORY_MAP_FAILED";
            break;
        case VK_ERROR_LAYER_NOT_PRESENT:
            message += "VK_ERROR_LAYER_NOT_PRESENT";
            break;
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            message += "VK_ERROR_EXTENSION_NOT_PRESENT";
            break;
        case VK_ERROR_FEATURE_NOT_PRESENT:
            message += "VK_ERROR_FEATURE_NOT_PRESENT";
            break;
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            message += "VK_ERROR_INCOMPATIBLE_DRIVER";
            break;
        case VK_ERROR_TOO_MANY_OBJECTS:
            message += "VK_ERROR_TOO_MANY_OBJECTS";
            break;
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            message += "VK_ERROR_FORMAT_NOT_SUPPORTED";
            break;
        case VK_ERROR_SURFACE_LOST_KHR:
            message += "VK_ERROR_SURFACE_LOST_KHR";
            break;
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            message += "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            break;
        case VK_SUBOPTIMAL_KHR:
            message += "VK_SUBOPTIMAL_KHR";
            break;
        case VK_ERROR_OUT_OF_DATE_KHR:
            message += "VK_ERROR_OUT_OF_DATE_KHR";
            break;
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            message += "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
            break;
        case VK_ERROR_VALIDATION_FAILED_EXT:
            message += "VK_ERROR_VALIDATION_FAILED_EXT";
            break;
        default:
            message += "VK_RESULT_UNKNOWN";
            break;
    }
    message += ')';
    return stellar::platform::Error(std::move(message));
}

void log_vulkan_message(std::string_view message) noexcept {
    std::cerr << message << '\n';
}

bool supports_required_queue(VkPhysicalDevice physical_device,
                             VkSurfaceKHR surface,
                             std::uint32_t& queue_family) noexcept {
    std::uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    if (queue_family_count == 0) {
        return false;
    }

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                             queue_families.data());

    for (std::uint32_t index = 0; index < queue_family_count; ++index) {
        VkBool32 present_supported = VK_FALSE;
        if (surface != VK_NULL_HANDLE) {
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, index, surface,
                                                 &present_supported);
        }
        const bool graphics_supported =
            (queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        if (graphics_supported && present_supported == VK_TRUE) {
            queue_family = index;
            return true;
        }
    }

    return false;
}

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

std::expected<SwapchainSupportDetails, stellar::platform::Error>
query_swapchain_support(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
    SwapchainSupportDetails details;
    VkResult result =
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &details.capabilities);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", result));
    }

    std::uint32_t format_count = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count,
                                                  nullptr);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkGetPhysicalDeviceSurfaceFormatsKHR", result));
    }
    details.formats.resize(format_count);
    if (format_count > 0) {
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count,
                                                      details.formats.data());
        if (result != VK_SUCCESS) {
            return std::unexpected(vulkan_error("vkGetPhysicalDeviceSurfaceFormatsKHR", result));
        }
    }

    std::uint32_t present_mode_count = 0;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                                       &present_mode_count, nullptr);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkGetPhysicalDeviceSurfacePresentModesKHR", result));
    }
    details.present_modes.resize(present_mode_count);
    if (present_mode_count > 0) {
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                                           &present_mode_count,
                                                           details.present_modes.data());
        if (result != VK_SUCCESS) {
            return std::unexpected(vulkan_error("vkGetPhysicalDeviceSurfacePresentModesKHR", result));
        }
    }

    return details;
}

VkSurfaceFormatKHR choose_surface_format(
    const std::vector<VkSurfaceFormatKHR>& available_formats) noexcept {
    for (const VkSurfaceFormatKHR& format : available_formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    for (const VkSurfaceFormatKHR& format : available_formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM) {
            return format;
        }
    }
    return available_formats.front();
}

VkPresentModeKHR choose_present_mode(
    const std::vector<VkPresentModeKHR>& available_present_modes) noexcept {
    for (VkPresentModeKHR present_mode : available_present_modes) {
        if (present_mode == VK_PRESENT_MODE_FIFO_KHR) {
            return present_mode;
        }
    }
    return available_present_modes.empty() ? VK_PRESENT_MODE_FIFO_KHR
                                           : available_present_modes.front();
}

VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities,
                         std::uint32_t preferred_width,
                         std::uint32_t preferred_height) noexcept {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    return VkExtent2D{
        std::clamp(preferred_width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width),
        std::clamp(preferred_height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height),
    };
}

VkCompositeAlphaFlagBitsKHR choose_composite_alpha(
    VkCompositeAlphaFlagsKHR supported_flags) noexcept {
    constexpr VkCompositeAlphaFlagBitsKHR composite_alpha_flags[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (VkCompositeAlphaFlagBitsKHR candidate : composite_alpha_flags) {
        if ((supported_flags & candidate) != 0) {
            return candidate;
        }
    }
    return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

bool has_stencil_component(VkFormat format) noexcept {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

std::uint32_t sanitize_dimension(int value, std::uint32_t fallback) noexcept {
    return value > 0 ? static_cast<std::uint32_t>(value) : fallback;
}

} // namespace

VulkanGraphicsDevice::~VulkanGraphicsDevice() noexcept {
    destroy_vulkan_objects();
    textures_.clear();
    materials_.clear();
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::initialize(stellar::platform::Window& window) {
    if (initialized_) {
        return {};
    }

    if (window.native_handle() == nullptr) {
        return std::unexpected(stellar::platform::Error("Window is not initialized"));
    }

    window_ = window.native_handle();
    if (auto result = create_instance(window); !result) {
        destroy_vulkan_objects();
        return result;
    }
    if (auto result = create_surface(window); !result) {
        destroy_vulkan_objects();
        return result;
    }
    if (auto result = select_physical_device(); !result) {
        destroy_vulkan_objects();
        return result;
    }
    if (auto result = create_logical_device(); !result) {
        destroy_vulkan_objects();
        return result;
    }
    if (auto result = create_command_resources(); !result) {
        destroy_vulkan_objects();
        return result;
    }
    if (auto result = create_sync_objects(); !result) {
        destroy_vulkan_objects();
        return result;
    }

    int window_width = 1;
    int window_height = 1;
    SDL_GetWindowSize(window_, &window_width, &window_height);
    if (auto result = create_swapchain_resources(sanitize_dimension(window_width, 1U),
                                                 sanitize_dimension(window_height, 1U));
        !result) {
        destroy_vulkan_objects();
        return result;
    }

    initialized_ = true;
    return {};
}

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
    if (!valid_image_format(image.format)) {
        return std::unexpected(stellar::platform::Error("Unsupported image format"));
    }

    const std::uint64_t handle_value = allocate_handle();
    textures_.emplace(handle_value, TextureRecord{.image = texture.image,
                                                  .color_space = texture.color_space});
    return TextureHandle{handle_value};
}

std::expected<MaterialHandle, stellar::platform::Error>
VulkanGraphicsDevice::create_material(const MaterialUpload& material) {
    if (!initialized_) {
        return std::unexpected(stellar::platform::Error("Vulkan graphics device is not initialized"));
    }

    const std::uint64_t handle_value = allocate_handle();
    materials_.emplace(handle_value, MaterialRecord{material});
    return MaterialHandle{handle_value};
}

void VulkanGraphicsDevice::begin_frame(int width, int height) noexcept {
    if (!initialized_ || window_ == nullptr || device_ == VK_NULL_HANDLE ||
        graphics_queue_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE ||
        frames_.empty() || frame_in_progress_) {
        return;
    }

    FrameResources& frame = frames_[current_frame_index_];
    if (frame.command_buffer == VK_NULL_HANDLE || frame.command_pool == VK_NULL_HANDLE ||
        frame.image_available_semaphore == VK_NULL_HANDLE ||
        frame.render_finished_semaphore == VK_NULL_HANDLE ||
        frame.in_flight_fence == VK_NULL_HANDLE) {
        return;
    }

    int window_width = 0;
    int window_height = 0;
    SDL_GetWindowSize(window_, &window_width, &window_height);
    const std::uint32_t preferred_width =
        sanitize_dimension(window_width, sanitize_dimension(width, swapchain_extent_.width));
    const std::uint32_t preferred_height =
        sanitize_dimension(window_height, sanitize_dimension(height, swapchain_extent_.height));
    if (preferred_width == 0 || preferred_height == 0) {
        swapchain_needs_rebuild_ = true;
        return;
    }

    if (swapchain_needs_rebuild_ || swapchain_extent_.width != preferred_width ||
        swapchain_extent_.height != preferred_height) {
        if (auto result = recreate_swapchain_resources(preferred_width, preferred_height); !result) {
            log_vulkan_message(result.error().message);
            return;
        }
    }

    VkResult result =
        vkWaitForFences(device_, 1, &frame.in_flight_fence, VK_TRUE,
                        std::numeric_limits<std::uint64_t>::max());
    if (result != VK_SUCCESS) {
        initialized_ = false;
        log_vulkan_message(vulkan_error("vkWaitForFences", result).message);
        return;
    }

    result = vkAcquireNextImageKHR(device_, swapchain_, std::numeric_limits<std::uint64_t>::max(),
                                   frame.image_available_semaphore, VK_NULL_HANDLE,
                                   &current_swapchain_image_index_);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchain_needs_rebuild_ = true;
        if (auto recreate = recreate_swapchain_resources(preferred_width, preferred_height);
            !recreate) {
            log_vulkan_message(recreate.error().message);
        }
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        log_vulkan_message(vulkan_error("vkAcquireNextImageKHR", result).message);
        return;
    }
    if (result == VK_SUBOPTIMAL_KHR) {
        swapchain_needs_rebuild_ = true;
    }

    if (current_swapchain_image_index_ >= swapchain_image_fences_.size()) {
        initialized_ = false;
        log_vulkan_message("Acquired Vulkan swapchain image index is out of bounds");
        return;
    }
    VkFence& image_fence = swapchain_image_fences_[current_swapchain_image_index_];
    if (image_fence != VK_NULL_HANDLE && image_fence != frame.in_flight_fence) {
        result = vkWaitForFences(device_, 1, &image_fence, VK_TRUE,
                                 std::numeric_limits<std::uint64_t>::max());
        if (result != VK_SUCCESS) {
            initialized_ = false;
            log_vulkan_message(vulkan_error("vkWaitForFences", result).message);
            return;
        }
    }
    image_fence = frame.in_flight_fence;

    result = vkResetCommandPool(device_, frame.command_pool, 0);
    if (result != VK_SUCCESS) {
        initialized_ = false;
        log_vulkan_message(vulkan_error("vkResetCommandPool", result).message);
        return;
    }

    const VkCommandBufferBeginInfo command_buffer_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    result = vkBeginCommandBuffer(frame.command_buffer, &command_buffer_begin_info);
    if (result != VK_SUCCESS) {
        initialized_ = false;
        log_vulkan_message(vulkan_error("vkBeginCommandBuffer", result).message);
        return;
    }

    const VkClearValue clear_values[2] = {
        VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
        VkClearValue{.depthStencil = {1.0f, 0}},
    };
    const VkRenderPassBeginInfo render_pass_begin_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = render_pass_,
        .framebuffer = swapchain_framebuffers_[current_swapchain_image_index_],
        .renderArea = {{0, 0}, swapchain_extent_},
        .clearValueCount = 2,
        .pClearValues = clear_values,
    };
    vkCmdBeginRenderPass(frame.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    frame_in_progress_ = true;
}

void VulkanGraphicsDevice::draw_mesh(MeshHandle mesh,
                                     std::span<const MeshPrimitiveDrawCommand> commands,
                                     const MeshDrawTransforms& transforms) noexcept {
    (void)commands;
    (void)transforms;
    if (!initialized_ || meshes_.find(mesh.value) == meshes_.end()) {
        return;
    }
    // Phase 5E.3 limitation: mesh buffers are uploaded, but Vulkan draw submission remains a no-op
    // until graphics pipelines, descriptors, and shader resources exist.
}

void VulkanGraphicsDevice::end_frame() noexcept {
    if (!initialized_ || !frame_in_progress_ || frames_.empty()) {
        return;
    }

    FrameResources& frame = frames_[current_frame_index_];
    if (frame.command_buffer == VK_NULL_HANDLE || frame.in_flight_fence == VK_NULL_HANDLE) {
        frame_in_progress_ = false;
        return;
    }

    vkCmdEndRenderPass(frame.command_buffer);
    VkResult result = vkEndCommandBuffer(frame.command_buffer);
    if (result != VK_SUCCESS) {
        frame_in_progress_ = false;
        initialized_ = false;
        log_vulkan_message(vulkan_error("vkEndCommandBuffer", result).message);
        return;
    }

    result = vkResetFences(device_, 1, &frame.in_flight_fence);
    if (result != VK_SUCCESS) {
        frame_in_progress_ = false;
        initialized_ = false;
        log_vulkan_message(vulkan_error("vkResetFences", result).message);
        return;
    }

    constexpr VkPipelineStageFlags kWaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame.image_available_semaphore,
        .pWaitDstStageMask = &kWaitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &frame.command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &frame.render_finished_semaphore,
    };
    result = vkQueueSubmit(graphics_queue_, 1, &submit_info, frame.in_flight_fence);
    if (result != VK_SUCCESS) {
        frame_in_progress_ = false;
        initialized_ = false;
        log_vulkan_message(vulkan_error("vkQueueSubmit", result).message);
        return;
    }

    const VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame.render_finished_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &current_swapchain_image_index_,
    };
    result = vkQueuePresentKHR(graphics_queue_, &present_info);
    frame_in_progress_ = false;
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        swapchain_needs_rebuild_ = true;
        current_frame_index_ = (current_frame_index_ + 1) % frames_.size();
        return;
    }
    if (result != VK_SUCCESS) {
        initialized_ = false;
        log_vulkan_message(vulkan_error("vkQueuePresentKHR", result).message);
        return;
    }

    current_frame_index_ = (current_frame_index_ + 1) % frames_.size();
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
    textures_.erase(texture.value);
}

void VulkanGraphicsDevice::destroy_material(MaterialHandle material) noexcept {
    materials_.erase(material.value);
}

bool VulkanGraphicsDevice::is_initialized() const noexcept {
    return initialized_;
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::create_instance(stellar::platform::Window& window) {
    unsigned int extension_count = 0;
    if (SDL_Vulkan_GetInstanceExtensions(window.native_handle(), &extension_count, nullptr) !=
        SDL_TRUE) {
        std::string message = "Failed to query SDL Vulkan instance extensions: ";
        message += SDL_GetError();
        return std::unexpected(stellar::platform::Error(std::move(message)));
    }

    std::vector<const char*> extensions(extension_count);
    if (SDL_Vulkan_GetInstanceExtensions(window.native_handle(), &extension_count,
                                         extensions.data()) != SDL_TRUE) {
        std::string message = "Failed to get SDL Vulkan instance extensions: ";
        message += SDL_GetError();
        return std::unexpected(stellar::platform::Error(std::move(message)));
    }

    const VkApplicationInfo application_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Stellar Engine",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 3),
        .pEngineName = "Stellar Engine",
        .engineVersion = VK_MAKE_VERSION(0, 1, 3),
        .apiVersion = VK_API_VERSION_1_2,
    };

    const VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions.data(),
    };

    const VkResult result = vkCreateInstance(&create_info, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkCreateInstance", result));
    }

    return {};
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::create_surface(stellar::platform::Window& window) {
    if (SDL_Vulkan_CreateSurface(window.native_handle(), instance_, &surface_) != SDL_TRUE) {
        std::string message = "Failed to create SDL Vulkan surface: ";
        message += SDL_GetError();
        return std::unexpected(stellar::platform::Error(std::move(message)));
    }
    return {};
}

std::expected<void, stellar::platform::Error> VulkanGraphicsDevice::select_physical_device() {
    std::uint32_t device_count = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkEnumeratePhysicalDevices", result));
    }
    if (device_count == 0) {
        return std::unexpected(stellar::platform::Error("No Vulkan physical devices are available"));
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    result = vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkEnumeratePhysicalDevices", result));
    }

    for (VkPhysicalDevice candidate : devices) {
        std::uint32_t family = 0;
        if (supports_required_queue(candidate, surface_, family)) {
            physical_device_ = candidate;
            graphics_queue_family_ = family;
            return {};
        }
    }

    return std::unexpected(stellar::platform::Error(
        "No Vulkan physical device supports graphics and presentation for this surface"));
}

std::expected<void, stellar::platform::Error> VulkanGraphicsDevice::create_logical_device() {
    constexpr float kQueuePriority = 1.0f;
    const VkDeviceQueueCreateInfo queue_create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = graphics_queue_family_,
        .queueCount = 1,
        .pQueuePriorities = &kQueuePriority,
    };

    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const VkDeviceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = extensions,
    };

    const VkResult result = vkCreateDevice(physical_device_, &create_info, nullptr, &device_);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkCreateDevice", result));
    }

    vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
    return {};
}

std::expected<void, stellar::platform::Error> VulkanGraphicsDevice::create_command_resources() {
    frames_.assign(kFramesInFlight, FrameResources{});
    for (FrameResources& frame : frames_) {
        const VkCommandPoolCreateInfo command_pool_create_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = graphics_queue_family_,
        };
        VkResult result =
            vkCreateCommandPool(device_, &command_pool_create_info, nullptr, &frame.command_pool);
        if (result != VK_SUCCESS) {
            return std::unexpected(vulkan_error("vkCreateCommandPool", result));
        }

        const VkCommandBufferAllocateInfo command_buffer_allocate_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = frame.command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        result = vkAllocateCommandBuffers(device_, &command_buffer_allocate_info,
                                          &frame.command_buffer);
        if (result != VK_SUCCESS) {
            return std::unexpected(vulkan_error("vkAllocateCommandBuffers", result));
        }
    }

    return {};
}

std::expected<void, stellar::platform::Error> VulkanGraphicsDevice::create_sync_objects() {
    for (FrameResources& frame : frames_) {
        const VkSemaphoreCreateInfo semaphore_create_info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VkResult result = vkCreateSemaphore(device_, &semaphore_create_info, nullptr,
                                            &frame.image_available_semaphore);
        if (result != VK_SUCCESS) {
            return std::unexpected(vulkan_error("vkCreateSemaphore", result));
        }

        result = vkCreateSemaphore(device_, &semaphore_create_info, nullptr,
                                   &frame.render_finished_semaphore);
        if (result != VK_SUCCESS) {
            return std::unexpected(vulkan_error("vkCreateSemaphore", result));
        }

        const VkFenceCreateInfo fence_create_info{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        result = vkCreateFence(device_, &fence_create_info, nullptr, &frame.in_flight_fence);
        if (result != VK_SUCCESS) {
            return std::unexpected(vulkan_error("vkCreateFence", result));
        }
    }

    return {};
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::create_swapchain_resources(std::uint32_t preferred_width,
                                                 std::uint32_t preferred_height) {
    if (auto result = create_swapchain(preferred_width, preferred_height); !result) {
        return result;
    }
    if (auto result = create_swapchain_image_views(); !result) {
        destroy_swapchain_resources();
        return result;
    }
    if (auto result = create_render_pass(); !result) {
        destroy_swapchain_resources();
        return result;
    }
    if (auto result = create_depth_resources(); !result) {
        destroy_swapchain_resources();
        return result;
    }
    if (auto result = create_framebuffers(); !result) {
        destroy_swapchain_resources();
        return result;
    }

    swapchain_image_fences_.assign(swapchain_images_.size(), VK_NULL_HANDLE);
    swapchain_needs_rebuild_ = false;
    return {};
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::recreate_swapchain_resources(std::uint32_t preferred_width,
                                                   std::uint32_t preferred_height) {
    if (device_ == VK_NULL_HANDLE) {
        return std::unexpected(stellar::platform::Error("Vulkan device is not initialized"));
    }
    if (preferred_width == 0 || preferred_height == 0) {
        swapchain_needs_rebuild_ = true;
        return {};
    }

    const VkResult wait_result = vkDeviceWaitIdle(device_);
    if (wait_result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkDeviceWaitIdle", wait_result));
    }

    destroy_swapchain_resources();
    return create_swapchain_resources(preferred_width, preferred_height);
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::create_swapchain(std::uint32_t preferred_width,
                                       std::uint32_t preferred_height) {
    auto support = query_swapchain_support(physical_device_, surface_);
    if (!support) {
        return std::unexpected(support.error());
    }
    if (support->formats.empty()) {
        return std::unexpected(stellar::platform::Error("No Vulkan surface formats are available"));
    }
    if (support->present_modes.empty()) {
        return std::unexpected(stellar::platform::Error("No Vulkan present modes are available"));
    }

    const VkSurfaceFormatKHR surface_format = choose_surface_format(support->formats);
    const VkPresentModeKHR present_mode = choose_present_mode(support->present_modes);
    const VkExtent2D extent = choose_extent(support->capabilities, preferred_width, preferred_height);

    std::uint32_t image_count = support->capabilities.minImageCount + 1;
    if (support->capabilities.maxImageCount > 0 &&
        image_count > support->capabilities.maxImageCount) {
        image_count = support->capabilities.maxImageCount;
    }

    const VkSwapchainCreateInfoKHR create_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface_,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = support->capabilities.currentTransform,
        .compositeAlpha = choose_composite_alpha(support->capabilities.supportedCompositeAlpha),
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    VkResult result = vkCreateSwapchainKHR(device_, &create_info, nullptr, &swapchain_);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkCreateSwapchainKHR", result));
    }

    result = vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkGetSwapchainImagesKHR", result));
    }
    swapchain_images_.resize(image_count);
    result = vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swapchain_images_.data());
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkGetSwapchainImagesKHR", result));
    }

    swapchain_image_format_ = surface_format.format;
    swapchain_extent_ = extent;
    return {};
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::create_swapchain_image_views() {
    swapchain_image_views_.clear();
    swapchain_image_views_.reserve(swapchain_images_.size());

    for (VkImage image : swapchain_images_) {
        const VkImageViewCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain_image_format_,
            .components = {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                VK_IMAGE_ASPECT_COLOR_BIT,
                0,
                1,
                0,
                1,
            },
        };

        VkImageView image_view = VK_NULL_HANDLE;
        const VkResult result = vkCreateImageView(device_, &create_info, nullptr, &image_view);
        if (result != VK_SUCCESS) {
            return std::unexpected(vulkan_error("vkCreateImageView", result));
        }
        swapchain_image_views_.push_back(image_view);
    }

    return {};
}

std::expected<void, stellar::platform::Error> VulkanGraphicsDevice::create_render_pass() {
    const auto depth_format = find_depth_format();
    if (!depth_format) {
        return std::unexpected(depth_format.error());
    }
    depth_format_ = *depth_format;

    const VkAttachmentDescription color_attachment{
        .format = swapchain_image_format_,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    const VkAttachmentDescription depth_attachment{
        .format = depth_format_,
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
    const VkAttachmentDescription attachments[] = {color_attachment, depth_attachment};
    const VkRenderPassCreateInfo render_pass_create_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    const VkResult result =
        vkCreateRenderPass(device_, &render_pass_create_info, nullptr, &render_pass_);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkCreateRenderPass", result));
    }

    return {};
}

std::expected<void, stellar::platform::Error> VulkanGraphicsDevice::create_depth_resources() {
    const VkImageCreateInfo image_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depth_format_,
        .extent = {swapchain_extent_.width, swapchain_extent_.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkResult result = vkCreateImage(device_, &image_create_info, nullptr, &depth_image_);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkCreateImage", result));
    }

    VkMemoryRequirements memory_requirements{};
    vkGetImageMemoryRequirements(device_, depth_image_, &memory_requirements);
    const auto memory_type_index =
        find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memory_type_index) {
        return std::unexpected(memory_type_index.error());
    }

    const VkMemoryAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = *memory_type_index,
    };
    result = vkAllocateMemory(device_, &allocate_info, nullptr, &depth_image_memory_);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkAllocateMemory", result));
    }

    result = vkBindImageMemory(device_, depth_image_, depth_image_memory_, 0);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkBindImageMemory", result));
    }

    VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (has_stencil_component(depth_format_)) {
        aspect_mask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    const VkImageViewCreateInfo image_view_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depth_image_,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depth_format_,
        .subresourceRange = {
            aspect_mask,
            0,
            1,
            0,
            1,
        },
    };
    result = vkCreateImageView(device_, &image_view_create_info, nullptr, &depth_image_view_);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkCreateImageView", result));
    }

    return {};
}

std::expected<void, stellar::platform::Error> VulkanGraphicsDevice::create_framebuffers() {
    swapchain_framebuffers_.clear();
    swapchain_framebuffers_.reserve(swapchain_image_views_.size());

    for (VkImageView image_view : swapchain_image_views_) {
        const VkImageView attachments[] = {image_view, depth_image_view_};
        const VkFramebufferCreateInfo framebuffer_create_info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass_,
            .attachmentCount = 2,
            .pAttachments = attachments,
            .width = swapchain_extent_.width,
            .height = swapchain_extent_.height,
            .layers = 1,
        };

        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        const VkResult result =
            vkCreateFramebuffer(device_, &framebuffer_create_info, nullptr, &framebuffer);
        if (result != VK_SUCCESS) {
            return std::unexpected(vulkan_error("vkCreateFramebuffer", result));
        }
        swapchain_framebuffers_.push_back(framebuffer);
    }

    return {};
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

std::expected<VkFormat, stellar::platform::Error> VulkanGraphicsDevice::find_depth_format() const {
    constexpr VkFormat kCandidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (VkFormat format : kCandidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physical_device_, format, &properties);
        if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) !=
            0) {
            return format;
        }
    }

    return std::unexpected(stellar::platform::Error(
        "No Vulkan depth format supports VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT"));
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

void VulkanGraphicsDevice::destroy_swapchain_resources() noexcept {
    swapchain_image_fences_.clear();

    for (VkFramebuffer framebuffer : swapchain_framebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
    }
    swapchain_framebuffers_.clear();

    if (depth_image_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, depth_image_view_, nullptr);
        depth_image_view_ = VK_NULL_HANDLE;
    }
    if (depth_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, depth_image_, nullptr);
        depth_image_ = VK_NULL_HANDLE;
    }
    if (depth_image_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, depth_image_memory_, nullptr);
        depth_image_memory_ = VK_NULL_HANDLE;
    }
    depth_format_ = VK_FORMAT_UNDEFINED;

    if (render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }

    for (VkImageView image_view : swapchain_image_views_) {
        if (image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, image_view, nullptr);
        }
    }
    swapchain_image_views_.clear();
    swapchain_images_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }

    swapchain_image_format_ = VK_FORMAT_UNDEFINED;
    swapchain_extent_ = {0, 0};
}

void VulkanGraphicsDevice::destroy_command_resources() noexcept {
    for (FrameResources& frame : frames_) {
        frame.command_buffer = VK_NULL_HANDLE;
        if (frame.command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, frame.command_pool, nullptr);
            frame.command_pool = VK_NULL_HANDLE;
        }
    }
    frames_.clear();
}

void VulkanGraphicsDevice::destroy_sync_objects() noexcept {
    for (FrameResources& frame : frames_) {
        if (frame.in_flight_fence != VK_NULL_HANDLE) {
            vkDestroyFence(device_, frame.in_flight_fence, nullptr);
            frame.in_flight_fence = VK_NULL_HANDLE;
        }
        if (frame.render_finished_semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, frame.render_finished_semaphore, nullptr);
            frame.render_finished_semaphore = VK_NULL_HANDLE;
        }
        if (frame.image_available_semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, frame.image_available_semaphore, nullptr);
            frame.image_available_semaphore = VK_NULL_HANDLE;
        }
    }
}

void VulkanGraphicsDevice::destroy_vulkan_objects() noexcept {
    initialized_ = false;
    frame_in_progress_ = false;
    swapchain_needs_rebuild_ = false;
    current_frame_index_ = 0;
    current_swapchain_image_index_ = 0;

    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        for (auto& [handle, mesh] : meshes_) {
            (void)handle;
            destroy_mesh_record(mesh);
        }
        meshes_.clear();
        destroy_swapchain_resources();
        destroy_sync_objects();
        destroy_command_resources();
    }

    graphics_queue_ = VK_NULL_HANDLE;
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    physical_device_ = VK_NULL_HANDLE;
    if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    graphics_queue_family_ = 0;
    window_ = nullptr;
}

std::uint64_t VulkanGraphicsDevice::allocate_handle() noexcept {
    return next_handle_++;
}

} // namespace stellar::graphics::vulkan
