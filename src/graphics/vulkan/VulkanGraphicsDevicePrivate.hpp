#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

namespace stellar::graphics::vulkan {


struct VulkanDrawPushConstants {
    std::array<float, 16> mvp{};
    std::array<float, 4> base_color{};
    std::array<float, 4> emissive_alpha_cutoff{};
    std::array<float, 4> factors{};
    std::array<std::uint32_t, 4> flags{};
};

static_assert(sizeof(VulkanDrawPushConstants) == 128);

struct VulkanMaterialUniform {
    std::array<std::array<float, 4>, 7> transform0{};
    std::array<std::array<float, 4>, 7> transform1{};
    std::array<float, 4> baked_lighting0{};
    std::array<float, 4> baked_lighting1{};
    std::array<float, 4> lighting2{};
};

static_assert(sizeof(VulkanMaterialUniform) == 272);

stellar::platform::Error vulkan_error(const char* operation, VkResult result);
void log_vulkan_message(std::string_view message) noexcept;
std::uint32_t sanitize_dimension(int value, std::uint32_t fallback) noexcept;

} // namespace stellar::graphics::vulkan
