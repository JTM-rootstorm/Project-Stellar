#include "VulkanGraphicsDevicePrivate.hpp"

#include <array>
#include <cstdint>
#include <limits>

#include <SDL2/SDL.h>

namespace stellar::graphics::vulkan {

namespace {

constexpr std::array<float, 4> kFallbackBaseColor{0.85F, 0.9F, 1.0F, 1.0F};

} // namespace

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
    if (!initialized_ || !frame_in_progress_ || graphics_pipeline_ == VK_NULL_HANDLE ||
        pipeline_layout_ == VK_NULL_HANDLE || frames_.empty()) {
        return;
    }

    auto mesh_it = meshes_.find(mesh.value);
    if (mesh_it == meshes_.end()) {
        return;
    }

    VkCommandBuffer command_buffer = frames_[current_frame_index_].command_buffer;
    if (command_buffer == VK_NULL_HANDLE) {
        return;
    }

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

    for (const MeshPrimitiveDrawCommand& command : commands) {
        if (command.primitive_index >= mesh_it->second.primitives.size()) {
            continue;
        }

        const MeshPrimitiveRecord& primitive = mesh_it->second.primitives[command.primitive_index];
        if (primitive.vertex_buffer == VK_NULL_HANDLE || primitive.index_buffer == VK_NULL_HANDLE ||
            primitive.index_count == 0 || primitive.has_skinning ||
            !command.skin_joint_matrices.empty()) {
            continue;
        }

        const auto material_it = materials_.find(command.material.value);
        const MaterialRecord* material = material_it != materials_.end() ? &material_it->second
                                                                         : nullptr;
        if (material != nullptr) {
            const bool textured = material->upload.base_color_texture.has_value() ||
                material->upload.normal_texture.has_value() ||
                material->upload.metallic_roughness_texture.has_value() ||
                material->upload.occlusion_texture.has_value() ||
                material->upload.emissive_texture.has_value();
            const bool opaque = material->upload.material.alpha_mode ==
                stellar::assets::AlphaMode::kOpaque;
            if (textured || !opaque) {
                continue;
            }
        }

        VulkanDrawPushConstants push_constants{
            .mvp = transforms.mvp,
            .base_color = material != nullptr ? material->upload.material.base_color_factor
                                              : kFallbackBaseColor,
            .has_vertex_color = primitive.has_colors ? 1U : 0U,
        };
        vkCmdPushConstants(command_buffer, pipeline_layout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(VulkanDrawPushConstants), &push_constants);

        const VkBuffer vertex_buffers[] = {primitive.vertex_buffer};
        const VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(command_buffer, primitive.index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(command_buffer, static_cast<std::uint32_t>(primitive.index_count), 1, 0,
                         0, 0);
    }
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
} // namespace stellar::graphics::vulkan
