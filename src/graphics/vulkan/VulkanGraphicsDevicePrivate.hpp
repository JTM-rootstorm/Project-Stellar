#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

namespace stellar::graphics::vulkan {

constexpr std::size_t kMaxSkinJoints = kMaxSkinPaletteJoints;
constexpr std::size_t kSkinDrawUniformSlotsPerFrame = 256;

struct VulkanSkinDrawUniform {
    std::array<float, 16> mvp{};
    std::array<float, 16> world{};
    std::array<float, 16> normal{};
    std::uint32_t has_skinning = 0;
    std::uint32_t joint_count = 0;
    std::array<std::uint32_t, 2> padding{};
    std::array<std::array<float, 16>, kMaxSkinJoints> joint_matrices{};
};

static_assert(sizeof(VulkanSkinDrawUniform) == 208 + (kMaxSkinJoints * 64));

struct VulkanDrawPushConstants {
    std::array<float, 16> mvp{};
    std::array<float, 4> base_color{};
    std::array<float, 4> emissive_alpha_cutoff{};
    std::array<float, 4> factors{};
    std::array<std::uint32_t, 4> flags{};
};

static_assert(sizeof(VulkanDrawPushConstants) == 128);

struct VulkanMaterialUniform {
    std::array<std::array<float, 4>, 5> transform0{};
    std::array<std::array<float, 4>, 5> transform1{};
};

static_assert(sizeof(VulkanMaterialUniform) == 160);

stellar::platform::Error vulkan_error(const char* operation, VkResult result);
void log_vulkan_message(std::string_view message) noexcept;
std::uint32_t sanitize_dimension(int value, std::uint32_t fallback) noexcept;

} // namespace stellar::graphics::vulkan
