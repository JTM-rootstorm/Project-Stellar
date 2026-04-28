#include "VulkanGraphicsDevicePrivate.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace stellar::graphics::vulkan {

namespace {

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

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

} // namespace

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
    if (auto result = create_graphics_pipeline(); !result) {
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
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
        return std::unexpected(vulkan_error("vkGetSwapchainImagesKHR", result));
    }
    swapchain_images_.resize(image_count);
    result = vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swapchain_images_.data());
    if (result != VK_SUCCESS) {
        swapchain_images_.clear();
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
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
            for (VkImageView created_view : swapchain_image_views_) {
                if (created_view != VK_NULL_HANDLE) {
                    vkDestroyImageView(device_, created_view, nullptr);
                }
            }
            swapchain_image_views_.clear();
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
        if (depth_image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, depth_image_, nullptr);
            depth_image_ = VK_NULL_HANDLE;
        }
        return std::unexpected(memory_type_index.error());
    }

    const VkMemoryAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = *memory_type_index,
    };
    result = vkAllocateMemory(device_, &allocate_info, nullptr, &depth_image_memory_);
    if (result != VK_SUCCESS) {
        if (depth_image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, depth_image_, nullptr);
            depth_image_ = VK_NULL_HANDLE;
        }
        return std::unexpected(vulkan_error("vkAllocateMemory", result));
    }

    result = vkBindImageMemory(device_, depth_image_, depth_image_memory_, 0);
    if (result != VK_SUCCESS) {
        if (depth_image_memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_, depth_image_memory_, nullptr);
            depth_image_memory_ = VK_NULL_HANDLE;
        }
        if (depth_image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, depth_image_, nullptr);
            depth_image_ = VK_NULL_HANDLE;
        }
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
        if (depth_image_memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_, depth_image_memory_, nullptr);
            depth_image_memory_ = VK_NULL_HANDLE;
        }
        if (depth_image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, depth_image_, nullptr);
            depth_image_ = VK_NULL_HANDLE;
        }
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
            for (VkFramebuffer created_framebuffer : swapchain_framebuffers_) {
                if (created_framebuffer != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(device_, created_framebuffer, nullptr);
                }
            }
            swapchain_framebuffers_.clear();
            return std::unexpected(vulkan_error("vkCreateFramebuffer", result));
        }
        swapchain_framebuffers_.push_back(framebuffer);
    }

    return {};
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

void VulkanGraphicsDevice::destroy_swapchain_resources() noexcept {
    swapchain_image_fences_.clear();

    if (graphics_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, graphics_pipeline_, nullptr);
        graphics_pipeline_ = VK_NULL_HANDLE;
    }
    if (graphics_pipeline_double_sided_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, graphics_pipeline_double_sided_, nullptr);
        graphics_pipeline_double_sided_ = VK_NULL_HANDLE;
    }
    if (alpha_blend_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, alpha_blend_pipeline_, nullptr);
        alpha_blend_pipeline_ = VK_NULL_HANDLE;
    }
    if (alpha_blend_pipeline_double_sided_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, alpha_blend_pipeline_double_sided_, nullptr);
        alpha_blend_pipeline_double_sided_ = VK_NULL_HANDLE;
    }

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
} // namespace stellar::graphics::vulkan
