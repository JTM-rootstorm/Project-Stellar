#include "VulkanGraphicsDevicePrivate.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <SDL2/SDL_vulkan.h>

namespace stellar::graphics::vulkan {

namespace {

constexpr std::size_t kFramesInFlight = 2;

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

} // namespace

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

void VulkanGraphicsDevice::destroy_command_resources() noexcept {
    for (FrameResources& frame : frames_) {
        destroy_buffer(frame.skin_draw_uniform_buffer, frame.skin_draw_uniform_memory);
        frame.skin_draw_descriptor_sets.clear();
        frame.skin_draw_upload_cursor = 0;
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
} // namespace stellar::graphics::vulkan
