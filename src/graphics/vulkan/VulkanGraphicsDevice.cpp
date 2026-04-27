#include "VulkanGraphicsDevicePrivate.hpp"

#include <cstdint>

#include <SDL2/SDL.h>

namespace stellar::graphics::vulkan {

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
    if (auto result = create_descriptor_resources(); !result) {
        destroy_vulkan_objects();
        return result;
    }
    initialized_ = true;
    if (auto result = create_default_material_textures(); !result) {
        destroy_vulkan_objects();
        return result;
    }
    if (auto result = create_pipeline_layout(); !result) {
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

    return {};
}

bool VulkanGraphicsDevice::is_initialized() const noexcept {
    return initialized_;
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
        for (auto& [handle, texture] : textures_) {
            (void)handle;
            destroy_texture_record(texture);
        }
        textures_.clear();
        for (auto& [handle, material] : materials_) {
            (void)handle;
            destroy_material_record(material);
        }
        materials_.clear();
        default_white_texture_ = TextureHandle{};
        default_normal_texture_ = TextureHandle{};
        destroy_swapchain_resources();
        if (pipeline_layout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
            pipeline_layout_ = VK_NULL_HANDLE;
        }
        if (default_material_descriptor_set_ != VK_NULL_HANDLE &&
            material_descriptor_pool_ != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(device_, material_descriptor_pool_, 1,
                                 &default_material_descriptor_set_);
            default_material_descriptor_set_ = VK_NULL_HANDLE;
        }
        if (material_descriptor_pool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_, material_descriptor_pool_, nullptr);
            material_descriptor_pool_ = VK_NULL_HANDLE;
        }
        if (material_descriptor_set_layout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_, material_descriptor_set_layout_, nullptr);
            material_descriptor_set_layout_ = VK_NULL_HANDLE;
        }
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
