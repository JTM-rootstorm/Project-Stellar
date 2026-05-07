#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

#if defined(__APPLE__)
#error "VulkanGraphicsDevice is Linux-only in Project Stellar; use Metal on macOS"
#endif

#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace stellar::graphics::vulkan {
namespace {

[[nodiscard]] stellar::platform::Error not_implemented(std::string_view operation) {
    std::string message = "Vulkan ";
    message += operation;
    message += " is not implemented yet";
    return stellar::platform::Error(std::move(message));
}

} // namespace

std::expected<void, stellar::platform::Error>
VulkanGraphicsDevice::initialize(stellar::platform::Window&) {
    return std::unexpected(not_implemented("initialization"));
}

std::expected<MeshHandle, stellar::platform::Error>
VulkanGraphicsDevice::create_mesh(const stellar::assets::MeshAsset&) {
    return std::unexpected(not_implemented("mesh upload"));
}

std::expected<TextureHandle, stellar::platform::Error>
VulkanGraphicsDevice::create_texture(const TextureUpload&) {
    return std::unexpected(not_implemented("texture upload"));
}

std::expected<MaterialHandle, stellar::platform::Error>
VulkanGraphicsDevice::create_material(const MaterialUpload&) {
    return std::unexpected(not_implemented("material upload"));
}

void VulkanGraphicsDevice::begin_frame(int, int) noexcept {}

void VulkanGraphicsDevice::draw_mesh(MeshHandle,
                                     std::span<const MeshPrimitiveDrawCommand>,
                                     const MeshDrawTransforms&) noexcept {}

void VulkanGraphicsDevice::end_frame() noexcept {}

void VulkanGraphicsDevice::destroy_mesh(MeshHandle) noexcept {}

void VulkanGraphicsDevice::destroy_texture(TextureHandle) noexcept {}

void VulkanGraphicsDevice::destroy_material(MaterialHandle) noexcept {}

} // namespace stellar::graphics::vulkan
