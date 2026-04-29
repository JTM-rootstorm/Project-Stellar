#include "VulkanGraphicsDevicePrivate.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace stellar::graphics::vulkan {

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

std::uint32_t sanitize_dimension(int value, std::uint32_t fallback) noexcept {
    return value > 0 ? static_cast<std::uint32_t>(value) : fallback;
}
} // namespace stellar::graphics::vulkan
