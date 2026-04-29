#include "VulkanGraphicsDevicePrivate.hpp"

#include <array>
#include <cstdint>
#include <limits>

#include <SDL2/SDL.h>

namespace stellar::graphics::vulkan {

namespace {

constexpr std::array<float, 4> kFallbackBaseColor{0.85F, 0.9F, 1.0F, 1.0F};
constexpr std::uint32_t kHasBaseColorTexture = 1U << 0U;
constexpr std::uint32_t kHasNormalTexture = 1U << 1U;
constexpr std::uint32_t kHasMetallicRoughnessTexture = 1U << 2U;
constexpr std::uint32_t kHasOcclusionTexture = 1U << 3U;
constexpr std::uint32_t kHasEmissiveTexture = 1U << 4U;
constexpr std::uint32_t kHasVertexColor = 1U << 5U;
constexpr std::uint32_t kHasTangents = 1U << 6U;
constexpr std::uint32_t kUnlitMaterial = 1U << 7U;

std::uint32_t alpha_mode_value(stellar::assets::AlphaMode mode) noexcept {
    switch (mode) {
        case stellar::assets::AlphaMode::kMask:
            return 1U;
        case stellar::assets::AlphaMode::kBlend:
            return 2U;
        case stellar::assets::AlphaMode::kOpaque:
        default:
            return 0U;
    }
}

template <typename TextureMap>
bool has_live_texture(const TextureMap& textures,
                      const std::optional<MaterialTextureBinding>& binding) noexcept {
    return binding.has_value() && textures.find(binding->texture.value) != textures.end();
}

std::uint32_t uv_bit(const std::optional<MaterialTextureBinding>& binding,
                     std::uint32_t slot) noexcept {
    return binding.has_value() && binding->texcoord_set == 1U ? (1U << slot) : 0U;
}

} // namespace

void VulkanGraphicsDevice::mark_swapchain_rebuild_pending() noexcept {
    swapchain_needs_rebuild_ = true;
}

void VulkanGraphicsDevice::reset_frame_state_after_failed_recording() noexcept {
    frame_in_progress_ = false;
    current_swapchain_image_index_ = 0;
}

VkExtent2D VulkanGraphicsDevice::current_window_extent_or_pending_rebuild(int width,
                                                                          int height) noexcept {
    (void)width;
    (void)height;
    int window_width = 0;
    int window_height = 0;
    if (window_ != nullptr) {
        SDL_GetWindowSize(window_, &window_width, &window_height);
    }
    if (window_width <= 0 || window_height <= 0) {
        mark_swapchain_rebuild_pending();
        return VkExtent2D{0, 0};
    }
    return VkExtent2D{sanitize_dimension(window_width, swapchain_extent_.width),
                      sanitize_dimension(window_height, swapchain_extent_.height)};
}

bool VulkanGraphicsDevice::recreate_swapchain_from_window_extent() noexcept {
    const VkExtent2D extent = current_window_extent_or_pending_rebuild(
        static_cast<int>(swapchain_extent_.width), static_cast<int>(swapchain_extent_.height));
    if (extent.width == 0 || extent.height == 0) {
        return false;
    }
    if (auto result = recreate_swapchain_resources(extent.width, extent.height); !result) {
        reset_frame_state_after_failed_recording();
        mark_swapchain_rebuild_pending();
        log_vulkan_message(result.error().message);
        return false;
    }
    current_swapchain_image_index_ = 0;
    return true;
}

void VulkanGraphicsDevice::begin_frame(int width, int height) noexcept {
    if (!initialized_ || window_ == nullptr || device_ == VK_NULL_HANDLE ||
        graphics_queue_ == VK_NULL_HANDLE || frames_.empty() || frame_in_progress_) {
        return;
    }

    FrameResources& frame = frames_[current_frame_index_];
    if (frame.command_buffer == VK_NULL_HANDLE || frame.command_pool == VK_NULL_HANDLE ||
        frame.image_available_semaphore == VK_NULL_HANDLE ||
        frame.render_finished_semaphore == VK_NULL_HANDLE ||
        frame.in_flight_fence == VK_NULL_HANDLE) {
        return;
    }

    const VkExtent2D preferred_extent = current_window_extent_or_pending_rebuild(width, height);
    if (preferred_extent.width == 0 || preferred_extent.height == 0) {
        return;
    }

    if (swapchain_ == VK_NULL_HANDLE || swapchain_needs_rebuild_ ||
        swapchain_extent_.width != preferred_extent.width ||
        swapchain_extent_.height != preferred_extent.height) {
        if (auto result = recreate_swapchain_resources(preferred_extent.width,
                                                       preferred_extent.height); !result) {
            reset_frame_state_after_failed_recording();
            mark_swapchain_rebuild_pending();
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
    if (frame.submitted_serial > completed_frame_serial_) {
        completed_frame_serial_ = frame.submitted_serial;
        frame.submitted_serial = 0;
    }
    drain_deferred_deletions(false);

    result = vkAcquireNextImageKHR(device_, swapchain_, std::numeric_limits<std::uint64_t>::max(),
                                   frame.image_available_semaphore, VK_NULL_HANDLE,
                                   &current_swapchain_image_index_);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        reset_frame_state_after_failed_recording();
        mark_swapchain_rebuild_pending();
        (void)recreate_swapchain_from_window_extent();
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        reset_frame_state_after_failed_recording();
        log_vulkan_message(vulkan_error("vkAcquireNextImageKHR", result).message);
        return;
    }
    if (result == VK_SUBOPTIMAL_KHR) {
        mark_swapchain_rebuild_pending();
    }

    if (current_swapchain_image_index_ >= swapchain_image_fences_.size() ||
        current_swapchain_image_index_ >= swapchain_framebuffers_.size()) {
        initialized_ = false;
        log_vulkan_message("Acquired Vulkan swapchain image index is out of bounds");
        reset_frame_state_after_failed_recording();
        return;
    }
    VkFence& image_fence = swapchain_image_fences_[current_swapchain_image_index_];
    if (image_fence != VK_NULL_HANDLE && image_fence != frame.in_flight_fence) {
        result = vkWaitForFences(device_, 1, &image_fence, VK_TRUE,
                                 std::numeric_limits<std::uint64_t>::max());
        if (result != VK_SUCCESS) {
            initialized_ = false;
            log_vulkan_message(vulkan_error("vkWaitForFences", result).message);
            reset_frame_state_after_failed_recording();
            return;
        }
    }
    image_fence = frame.in_flight_fence;

    result = vkResetCommandPool(device_, frame.command_pool, 0);
    if (result != VK_SUCCESS) {
        initialized_ = false;
        log_vulkan_message(vulkan_error("vkResetCommandPool", result).message);
        reset_frame_state_after_failed_recording();
        return;
    }
    frame.skin_draw_upload_cursor = 0;

    const VkCommandBufferBeginInfo command_buffer_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    result = vkBeginCommandBuffer(frame.command_buffer, &command_buffer_begin_info);
    if (result != VK_SUCCESS) {
        initialized_ = false;
        log_vulkan_message(vulkan_error("vkBeginCommandBuffer", result).message);
        reset_frame_state_after_failed_recording();
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

    VkPipeline bound_pipeline = VK_NULL_HANDLE;

    for (const MeshPrimitiveDrawCommand& command : commands) {
        if (command.primitive_index >= mesh_it->second.primitives.size()) {
            continue;
        }

        const MeshPrimitiveRecord& primitive = mesh_it->second.primitives[command.primitive_index];
        if (primitive.vertex_buffer == VK_NULL_HANDLE || primitive.index_buffer == VK_NULL_HANDLE ||
            primitive.index_count == 0) {
            continue;
        }

        const bool use_skinning = primitive.has_skinning && !command.skin_joint_matrices.empty();
        if (command.skin_joint_matrices.size() > kMaxSkinPaletteJoints) {
            log_vulkan_message(
                "Vulkan skin joint count exceeds 256-joint runtime cap; skipping skinned primitive");
            continue;
        }
        if (use_skinning && command.skin_joint_matrices.size() <= primitive.max_joint_index) {
            log_vulkan_message(
                "Vulkan skin joint palette is smaller than primitive joint indices; "
                "skipping skinned primitive");
            continue;
        }

        const auto material_it = materials_.find(command.material.value);
        const MaterialRecord* material = material_it != materials_.end() ? &material_it->second
                                                                         : nullptr;
        const bool has_base_color_texture = material != nullptr &&
            has_live_texture(textures_, material->upload.base_color_texture);
        const bool has_normal_texture = material != nullptr && primitive.has_tangents &&
            has_live_texture(textures_, material->upload.normal_texture);
        const bool has_metallic_roughness_texture = material != nullptr &&
            has_live_texture(textures_, material->upload.metallic_roughness_texture);
        const bool has_occlusion_texture = material != nullptr &&
            has_live_texture(textures_, material->upload.occlusion_texture);
        const bool has_emissive_texture = material != nullptr &&
            has_live_texture(textures_, material->upload.emissive_texture);
        const auto alpha_mode = material != nullptr ? material->upload.material.alpha_mode
                                                    : stellar::assets::AlphaMode::kOpaque;
        const bool alpha_blend = alpha_mode == stellar::assets::AlphaMode::kBlend;
        const bool double_sided = material != nullptr && material->upload.material.double_sided;
        const VkPipeline pipeline = alpha_blend
            ? (double_sided ? alpha_blend_pipeline_double_sided_ : alpha_blend_pipeline_)
            : (double_sided ? graphics_pipeline_double_sided_ : graphics_pipeline_);
        if (pipeline == VK_NULL_HANDLE) {
            continue;
        }
        if (bound_pipeline != pipeline) {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            bound_pipeline = pipeline;
        }

        std::uint32_t texture_flags = 0;
        texture_flags |= has_base_color_texture ? kHasBaseColorTexture : 0U;
        texture_flags |= has_normal_texture ? kHasNormalTexture : 0U;
        texture_flags |= has_metallic_roughness_texture ? kHasMetallicRoughnessTexture : 0U;
        texture_flags |= has_occlusion_texture ? kHasOcclusionTexture : 0U;
        texture_flags |= has_emissive_texture ? kHasEmissiveTexture : 0U;
        texture_flags |= primitive.has_colors ? kHasVertexColor : 0U;
        texture_flags |= primitive.has_tangents ? kHasTangents : 0U;
        texture_flags |= material != nullptr && material->upload.material.unlit ? kUnlitMaterial
                                                                                : 0U;

        std::uint32_t uv_flags = 0;
        if (material != nullptr) {
            uv_flags |= uv_bit(material->upload.base_color_texture, 0U);
            uv_flags |= uv_bit(material->upload.normal_texture, 1U);
            uv_flags |= uv_bit(material->upload.metallic_roughness_texture, 2U);
            uv_flags |= uv_bit(material->upload.occlusion_texture, 3U);
            uv_flags |= uv_bit(material->upload.emissive_texture, 4U);
        }

        const std::array<float, 3> emissive = material != nullptr
            ? material->upload.material.emissive_factor
            : std::array<float, 3>{0.0F, 0.0F, 0.0F};
        const float normal_scale = material != nullptr &&
                material->upload.material.normal_texture.has_value()
            ? material->upload.material.normal_texture->scale
            : 1.0F;

        VulkanDrawPushConstants push_constants{
            .mvp = transforms.mvp,
            .base_color = material != nullptr ? material->upload.material.base_color_factor
                                               : kFallbackBaseColor,
            .emissive_alpha_cutoff = {emissive[0], emissive[1], emissive[2],
                                      material != nullptr ? material->upload.material.alpha_cutoff
                                                          : 0.5F},
            .factors = {material != nullptr ? material->upload.material.metallic_factor : 0.0F,
                        material != nullptr ? material->upload.material.roughness_factor : 1.0F,
                        normal_scale,
                        material != nullptr ? material->upload.material.occlusion_strength : 1.0F},
            .flags = {texture_flags, uv_flags, alpha_mode_value(alpha_mode), 0U},
        };
        const VkDescriptorSet descriptor_set = material != nullptr &&
                material->descriptor_set != VK_NULL_HANDLE
            ? material->descriptor_set
            : default_material_descriptor_set_;
        if (descriptor_set == VK_NULL_HANDLE) {
            continue;
        }
        auto skin_draw_descriptor_set = upload_skin_draw_uniform(
            frames_[current_frame_index_], transforms, command.skin_joint_matrices, use_skinning);
        if (!skin_draw_descriptor_set) {
            log_vulkan_message(skin_draw_descriptor_set.error().message);
            continue;
        }
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout_, 0, 1, &descriptor_set, 0, nullptr);
        const VkDescriptorSet vertex_descriptor_set = *skin_draw_descriptor_set;
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout_, 1, 1, &vertex_descriptor_set, 0, nullptr);
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
        if (current_swapchain_image_index_ < swapchain_image_fences_.size()) {
            swapchain_image_fences_[current_swapchain_image_index_] = VK_NULL_HANDLE;
        }
        frame_in_progress_ = false;
        initialized_ = false;
        log_vulkan_message(vulkan_error("vkEndCommandBuffer", result).message);
        return;
    }

    result = vkResetFences(device_, 1, &frame.in_flight_fence);
    if (result != VK_SUCCESS) {
        if (current_swapchain_image_index_ < swapchain_image_fences_.size()) {
            swapchain_image_fences_[current_swapchain_image_index_] = VK_NULL_HANDLE;
        }
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
        if (current_swapchain_image_index_ < swapchain_image_fences_.size()) {
            swapchain_image_fences_[current_swapchain_image_index_] = VK_NULL_HANDLE;
        }
        frame_in_progress_ = false;
        initialized_ = false;
        log_vulkan_message(vulkan_error("vkQueueSubmit", result).message);
        return;
    }
    frame.submitted_serial = ++submitted_frame_serial_;

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
        log_vulkan_message(vulkan_error("vkQueuePresentKHR", result).message);
        mark_swapchain_rebuild_pending();
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
