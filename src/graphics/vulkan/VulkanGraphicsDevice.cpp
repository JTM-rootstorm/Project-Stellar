#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

#include <cstdint>
#include <cstring>
#include <string>

#include <SDL2/SDL_vulkan.h>

namespace stellar::graphics::vulkan {

namespace {

stellar::platform::Error vulkan_error(const char* operation, VkResult result) {
    std::string message(operation);
    message += " failed with VkResult ";
    message += std::to_string(static_cast<int>(result));
    return stellar::platform::Error(std::move(message));
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

} // namespace

VulkanGraphicsDevice::~VulkanGraphicsDevice() noexcept {
    meshes_.clear();
    textures_.clear();
    materials_.clear();
    destroy_vulkan_objects();
}

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::initialize(stellar::platform::Window& window) {
    if (initialized_) {
        return {};
    }

    if (window.native_handle() == nullptr) {
        return std::unexpected(stellar::platform::Error("Window is not initialized"));
    }

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
        record.primitives.push_back(MeshPrimitiveRecord{
            .vertex_count = primitive.vertices.size(),
            .index_count = primitive.indices.size(),
            .has_tangents = primitive.has_tangents,
            .has_colors = primitive.has_colors,
            .has_skinning = primitive.has_skinning,
        });
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

void VulkanGraphicsDevice::begin_frame(int /*width*/, int /*height*/) noexcept {
    // Phase 4A limitation: swapchain and render-pass setup are intentionally deferred.
}

void VulkanGraphicsDevice::draw_mesh(MeshHandle mesh,
                                     std::span<const MeshPrimitiveDrawCommand> commands,
                                     const MeshDrawTransforms& transforms) noexcept {
    (void)commands;
    (void)transforms;
    if (!initialized_ || meshes_.find(mesh.value) == meshes_.end()) {
        return;
    }
    // Phase 4A limitation: uploaded records are retained for parity, but Vulkan drawing is a no-op
    // until swapchain, pipeline, descriptor, and GPU-buffer support are implemented.
}

void VulkanGraphicsDevice::end_frame() noexcept {
    // Phase 4A limitation: presentation is deferred with swapchain support.
}

void VulkanGraphicsDevice::destroy_mesh(MeshHandle mesh) noexcept {
    meshes_.erase(mesh.value);
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

void VulkanGraphicsDevice::destroy_vulkan_objects() noexcept {
    initialized_ = false;
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
}

std::uint64_t VulkanGraphicsDevice::allocate_handle() noexcept {
    return next_handle_++;
}

} // namespace stellar::graphics::vulkan
