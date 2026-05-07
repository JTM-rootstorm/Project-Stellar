#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

#if defined(__APPLE__)
#error "VulkanGraphicsDevice is Linux-only in Project Stellar; use Metal on macOS"
#endif

#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stellar::graphics::vulkan {
namespace {

constexpr std::uint32_t kMaxFramesInFlight = 1;

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
    std::uint32_t image_index = 0;
    bool frame_started = false;
    bool initialized = false;

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
VulkanGraphicsDevice::create_mesh(const stellar::assets::MeshAsset&) {
    return std::unexpected(error("Vulkan mesh upload is not implemented yet"));
}

std::expected<TextureHandle, stellar::platform::Error>
VulkanGraphicsDevice::create_texture(const TextureUpload&) {
    return std::unexpected(error("Vulkan texture upload is not implemented yet"));
}

std::expected<MaterialHandle, stellar::platform::Error>
VulkanGraphicsDevice::create_material(const MaterialUpload&) {
    return std::unexpected(error("Vulkan material upload is not implemented yet"));
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

void VulkanGraphicsDevice::destroy_mesh(MeshHandle) noexcept {}

void VulkanGraphicsDevice::destroy_texture(TextureHandle) noexcept {}

void VulkanGraphicsDevice::destroy_material(MaterialHandle) noexcept {}

} // namespace stellar::graphics::vulkan
