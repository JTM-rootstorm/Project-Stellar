#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include <SDL2/SDL.h>

#include "stellar/platform/Window.hpp"

namespace {

constexpr std::array<float, 16> kIdentity4{1.0F, 0.0F, 0.0F, 0.0F,
                                           0.0F, 1.0F, 0.0F, 0.0F,
                                           0.0F, 0.0F, 1.0F, 0.0F,
                                           0.0F, 0.0F, 0.0F, 1.0F};
constexpr std::array<float, 9> kIdentity3{1.0F, 0.0F, 0.0F,
                                          0.0F, 1.0F, 0.0F,
                                          0.0F, 0.0F, 1.0F};

stellar::assets::MeshPrimitive make_triangle(float z_offset) {
    stellar::assets::MeshPrimitive primitive;
    primitive.vertices.resize(3);
    primitive.vertices[0].position = {-0.5F, -0.5F, z_offset};
    primitive.vertices[1].position = {0.5F, -0.5F, z_offset};
    primitive.vertices[2].position = {0.0F, 0.5F, z_offset};
    primitive.vertices[0].normal = {0.0F, 0.0F, 1.0F};
    primitive.vertices[1].normal = {0.0F, 0.0F, 1.0F};
    primitive.vertices[2].normal = {0.0F, 0.0F, 1.0F};
    primitive.vertices[0].uv0 = {0.0F, 0.0F};
    primitive.vertices[1].uv0 = {1.0F, 0.0F};
    primitive.vertices[2].uv0 = {0.5F, 1.0F};
    primitive.vertices[0].uv1 = {1.0F, 1.0F};
    primitive.vertices[1].uv1 = {0.0F, 1.0F};
    primitive.vertices[2].uv1 = {0.5F, 0.0F};
    primitive.indices = {0, 1, 2};
    primitive.bounds_min = {-0.5F, -0.5F, z_offset};
    primitive.bounds_max = {0.5F, 0.5F, z_offset};
    return primitive;
}

stellar::assets::MeshAsset make_parity_matrix_mesh() {
    stellar::assets::MeshPrimitive base = make_triangle(0.0F);

    stellar::assets::MeshPrimitive colored = make_triangle(-0.1F);
    colored.has_colors = true;
    colored.vertices[0].color = {1.0F, 0.0F, 0.0F, 1.0F};
    colored.vertices[1].color = {0.0F, 1.0F, 0.0F, 1.0F};
    colored.vertices[2].color = {0.0F, 0.0F, 1.0F, 1.0F};

    stellar::assets::MeshPrimitive tangent = make_triangle(-0.2F);
    tangent.has_tangents = true;
    for (stellar::assets::StaticVertex& vertex : tangent.vertices) {
        vertex.tangent = {1.0F, 0.0F, 0.0F, 1.0F};
    }

    stellar::assets::MeshPrimitive skinned = make_triangle(-0.3F);
    skinned.has_skinning = true;
    for (stellar::assets::StaticVertex& vertex : skinned.vertices) {
        vertex.joints0 = {0, 0, 0, 0};
        vertex.weights0 = {1.0F, 0.0F, 0.0F, 0.0F};
    }

    stellar::assets::MeshPrimitive high_joint_skinned = make_triangle(-0.4F);
    high_joint_skinned.has_skinning = true;
    for (stellar::assets::StaticVertex& vertex : high_joint_skinned.vertices) {
        vertex.joints0 = {127, 0, 0, 0};
        vertex.weights0 = {1.0F, 0.0F, 0.0F, 0.0F};
    }

    return stellar::assets::MeshAsset{.name = "vulkan_parity_matrix",
                                       .primitives = {base, colored, tangent, skinned,
                                                      high_joint_skinned}};
}

stellar::graphics::TextureUpload make_texture_upload(const char* name,
                                                     std::uint32_t width,
                                                     std::uint32_t height,
                                                     stellar::assets::ImageFormat format,
                                                     stellar::assets::TextureColorSpace color_space,
                                                     std::vector<std::uint8_t> pixels) {
    stellar::assets::ImageAsset image;
    image.name = name;
    image.width = width;
    image.height = height;
    image.format = format;
    image.pixels = std::move(pixels);
    return stellar::graphics::TextureUpload{.image = std::move(image), .color_space = color_space};
}

stellar::assets::SamplerAsset make_sampler(const char* name,
                                           stellar::assets::TextureFilter mag_filter,
                                           stellar::assets::TextureFilter min_filter,
                                           stellar::assets::TextureWrapMode wrap_s,
                                           stellar::assets::TextureWrapMode wrap_t) {
    return stellar::assets::SamplerAsset{.name = name,
                                         .mag_filter = mag_filter,
                                         .min_filter = min_filter,
                                         .wrap_s = wrap_s,
                                         .wrap_t = wrap_t};
}

stellar::graphics::MaterialTextureBinding make_binding(stellar::graphics::TextureHandle texture,
                                                       const stellar::assets::SamplerAsset& sampler,
                                                       std::uint32_t texcoord_set) {
    return stellar::graphics::MaterialTextureBinding{.texture = texture,
                                                     .sampler = sampler,
                                                     .texcoord_set = texcoord_set};
}

template <typename T>
bool require_expected(const T& result, const char* label) {
    if (!result) {
        std::cerr << label << ": " << result.error().message << '\n';
        return false;
    }
    return true;
}

void draw_frame(stellar::graphics::vulkan::VulkanGraphicsDevice& device,
                stellar::graphics::MeshHandle mesh,
                std::span<const stellar::graphics::MeshPrimitiveDrawCommand> commands,
                int width,
                int height) {
    const stellar::graphics::MeshDrawTransforms transforms{.mvp = kIdentity4,
                                                           .world = kIdentity4,
                                                           .normal = kIdentity3};
    device.begin_frame(width, height);
    device.draw_mesh(mesh, commands, transforms);
    device.end_frame();
}

} // namespace

int main() {
    stellar::platform::Window window;
    auto window_result = window.create(16, 16, "stellar-vulkan-smoke",
                                       SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
    if (!window_result) {
        std::cerr << window_result.error().message << '\n';
        return 1;
    }

    stellar::graphics::vulkan::VulkanGraphicsDevice device;
    auto init_result = device.initialize(window);
    if (!init_result) {
        std::cerr << init_result.error().message << '\n';
        return 1;
    }

    if (!device.is_initialized()) {
        std::cerr << "Vulkan device reported success but is not initialized\n";
        return 1;
    }

    auto mesh = device.create_mesh(make_parity_matrix_mesh());
    if (!require_expected(mesh, "create parity mesh")) {
        return 1;
    }

    auto base_color_texture = device.create_texture(make_texture_upload(
        "vulkan_base_color_rgb", 2, 2, stellar::assets::ImageFormat::kR8G8B8,
        stellar::assets::TextureColorSpace::kSrgb,
        {255, 255, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255}));
    if (!require_expected(base_color_texture, "create base color texture")) {
        return 1;
    }

    auto normal_texture = device.create_texture(make_texture_upload(
        "vulkan_flat_normal_rgba", 2, 2, stellar::assets::ImageFormat::kR8G8B8A8,
        stellar::assets::TextureColorSpace::kLinear,
        {128, 128, 255, 255, 128, 128, 255, 255,
         128, 128, 255, 255, 128, 128, 255, 255}));
    if (!require_expected(normal_texture, "create normal texture")) {
        return 1;
    }

    auto metallic_roughness_texture = device.create_texture(make_texture_upload(
        "vulkan_metallic_roughness", 1, 1, stellar::assets::ImageFormat::kR8G8B8A8,
        stellar::assets::TextureColorSpace::kLinear, {255, 128, 64, 255}));
    if (!require_expected(metallic_roughness_texture, "create metallic-roughness texture")) {
        return 1;
    }

    auto occlusion_texture = device.create_texture(make_texture_upload(
        "vulkan_occlusion", 1, 1, stellar::assets::ImageFormat::kR8G8B8A8,
        stellar::assets::TextureColorSpace::kLinear, {192, 192, 192, 255}));
    if (!require_expected(occlusion_texture, "create occlusion texture")) {
        return 1;
    }

    auto emissive_texture = device.create_texture(make_texture_upload(
        "vulkan_emissive", 1, 1, stellar::assets::ImageFormat::kR8G8B8A8,
        stellar::assets::TextureColorSpace::kSrgb, {32, 64, 255, 255}));
    if (!require_expected(emissive_texture, "create emissive texture")) {
        return 1;
    }

    const auto nearest_repeat = make_sampler("nearest_repeat", stellar::assets::TextureFilter::kNearest,
                                             stellar::assets::TextureFilter::kNearestMipmapNearest,
                                             stellar::assets::TextureWrapMode::kRepeat,
                                             stellar::assets::TextureWrapMode::kClampToEdge);
    const auto linear_mirror = make_sampler("linear_mirror", stellar::assets::TextureFilter::kLinear,
                                            stellar::assets::TextureFilter::kLinearMipmapLinear,
                                            stellar::assets::TextureWrapMode::kMirroredRepeat,
                                            stellar::assets::TextureWrapMode::kMirroredRepeat);

    stellar::graphics::MaterialUpload full_material_upload;
    full_material_upload.material.name = "full_lightweight_material";
    full_material_upload.material.base_color_factor = {0.8F, 0.9F, 1.0F, 1.0F};
    full_material_upload.material.base_color_texture = stellar::assets::MaterialTextureSlot{0, 0};
    full_material_upload.material.normal_texture = stellar::assets::MaterialTextureSlot{1, 0, 0.75F};
    full_material_upload.material.metallic_roughness_texture =
        stellar::assets::MaterialTextureSlot{2, 1};
    full_material_upload.material.occlusion_texture = stellar::assets::MaterialTextureSlot{3, 0};
    full_material_upload.material.emissive_texture = stellar::assets::MaterialTextureSlot{4, 1};
    full_material_upload.material.emissive_factor = {0.1F, 0.2F, 0.3F};
    full_material_upload.material.occlusion_strength = 0.65F;
    full_material_upload.material.metallic_factor = 0.5F;
    full_material_upload.material.roughness_factor = 0.4F;
    full_material_upload.base_color_texture = make_binding(*base_color_texture, nearest_repeat, 0);
    full_material_upload.normal_texture = make_binding(*normal_texture, linear_mirror, 0);
    full_material_upload.metallic_roughness_texture =
        make_binding(*metallic_roughness_texture, linear_mirror, 1);
    full_material_upload.occlusion_texture = make_binding(*occlusion_texture, nearest_repeat, 0);
    full_material_upload.emissive_texture = make_binding(*emissive_texture, linear_mirror, 1);
    auto full_material = device.create_material(full_material_upload);
    if (!require_expected(full_material, "create full material")) {
        return 1;
    }

    stellar::graphics::MaterialUpload fallback_material_upload;
    fallback_material_upload.material.name = "missing_optional_texture_fallback";
    fallback_material_upload.material.base_color_factor = {0.85F, 0.9F, 1.0F, 1.0F};
    auto fallback_material = device.create_material(fallback_material_upload);
    if (!require_expected(fallback_material, "create fallback material")) {
        return 1;
    }

    stellar::graphics::MaterialUpload alpha_mask_upload;
    alpha_mask_upload.material.name = "alpha_mask";
    alpha_mask_upload.material.base_color_factor = {1.0F, 1.0F, 1.0F, 0.35F};
    alpha_mask_upload.material.alpha_mode = stellar::assets::AlphaMode::kMask;
    alpha_mask_upload.material.alpha_cutoff = 0.5F;
    auto alpha_mask_material = device.create_material(alpha_mask_upload);
    if (!require_expected(alpha_mask_material, "create alpha mask material")) {
        return 1;
    }

    stellar::graphics::MaterialUpload alpha_blend_upload;
    alpha_blend_upload.material.name = "alpha_blend";
    alpha_blend_upload.material.base_color_factor = {1.0F, 0.75F, 0.5F, 0.45F};
    alpha_blend_upload.material.alpha_mode = stellar::assets::AlphaMode::kBlend;
    auto alpha_blend_material = device.create_material(alpha_blend_upload);
    if (!require_expected(alpha_blend_material, "create alpha blend material")) {
        return 1;
    }

    stellar::graphics::MaterialUpload double_sided_upload;
    double_sided_upload.material.name = "double_sided_opaque";
    double_sided_upload.material.double_sided = true;
    auto double_sided_material = device.create_material(double_sided_upload);
    if (!require_expected(double_sided_material, "create double-sided material")) {
        return 1;
    }

    stellar::graphics::MaterialUpload double_sided_blend_upload;
    double_sided_blend_upload.material.name = "double_sided_blend";
    double_sided_blend_upload.material.base_color_factor = {0.5F, 0.75F, 1.0F, 0.5F};
    double_sided_blend_upload.material.alpha_mode = stellar::assets::AlphaMode::kBlend;
    double_sided_blend_upload.material.double_sided = true;
    auto double_sided_blend_material = device.create_material(double_sided_blend_upload);
    if (!require_expected(double_sided_blend_material, "create double-sided blend material")) {
        return 1;
    }

    stellar::graphics::MaterialUpload unlit_upload;
    unlit_upload.material.name = "unlit_textured";
    unlit_upload.material.base_color_factor = {0.6F, 0.7F, 1.0F, 0.8F};
    unlit_upload.material.unlit = true;
    unlit_upload.material.alpha_mode = stellar::assets::AlphaMode::kMask;
    unlit_upload.material.alpha_cutoff = 0.25F;
    unlit_upload.material.double_sided = true;
    unlit_upload.material.base_color_texture = stellar::assets::MaterialTextureSlot{0, 0};
    unlit_upload.base_color_texture = make_binding(*base_color_texture, nearest_repeat, 0);
    auto unlit_material = device.create_material(unlit_upload);
    if (!require_expected(unlit_material, "create unlit material")) {
        return 1;
    }

    const std::array<float, 16> skin_matrix{1.0F, 0.0F, 0.0F, 0.0F,
                                            0.0F, 1.0F, 0.0F, 0.0F,
                                            0.0F, 0.0F, 1.0F, 0.0F,
                                            0.1F, 0.0F, 0.0F, 1.0F};
    const std::array<std::array<float, 16>, 1> one_joint_palette{skin_matrix};
    std::vector<std::array<float, 16>> full_skin_palette(128, kIdentity4);
    full_skin_palette[127] = skin_matrix;
    std::vector<std::array<float, 16>> over_limit_skin_palette(
        stellar::graphics::kMaxSkinPaletteJoints + 1, kIdentity4);
    const std::span<const std::array<float, 16>> one_joint_span{one_joint_palette.data(),
                                                               one_joint_palette.size()};
    const std::span<const std::array<float, 16>> full_skin_span{full_skin_palette.data(),
                                                               full_skin_palette.size()};
    const std::span<const std::array<float, 16>> over_limit_skin_span{
        over_limit_skin_palette.data(), over_limit_skin_palette.size()};

    const stellar::graphics::MeshPrimitiveDrawCommand commands[] = {
        {.primitive_index = 0, .material = *fallback_material},
        {.primitive_index = 1, .material = *fallback_material},
        {.primitive_index = 2, .material = *full_material},
        {.primitive_index = 0, .material = *alpha_mask_material},
        {.primitive_index = 0, .material = *alpha_blend_material},
        {.primitive_index = 0, .material = *double_sided_material},
        {.primitive_index = 0, .material = *double_sided_blend_material},
        {.primitive_index = 1, .material = *unlit_material},
        {.primitive_index = 3, .material = *fallback_material},
        {.primitive_index = 3, .material = *fallback_material, .skin_joint_matrices = one_joint_span},
        {.primitive_index = 4, .material = *fallback_material, .skin_joint_matrices = full_skin_span},
        {.primitive_index = 4, .material = *fallback_material, .skin_joint_matrices = one_joint_span},
        {.primitive_index = 3, .material = *fallback_material, .skin_joint_matrices = over_limit_skin_span},
    };

    for (int frame = 0; frame < 3; ++frame) {
        draw_frame(device, *mesh, commands, 16, 16);
    }

    // Hidden SDL Vulkan windows do not always exercise the compositor's real resize/out-of-date
    // present path, but changing the SDL window size still drives the renderer's normal
    // SDL-size-based recreate path when the platform reports the new hidden-window extent.
    SDL_SetWindowSize(window.native_handle(), 32, 32);
    draw_frame(device, *mesh, commands, 32, 32);

    device.destroy_texture(*base_color_texture);
    draw_frame(device, *mesh, commands, 32, 32);

    device.destroy_material(*full_material);
    device.destroy_material(*fallback_material);
    device.destroy_material(*alpha_mask_material);
    device.destroy_material(*alpha_blend_material);
    device.destroy_material(*double_sided_material);
    device.destroy_material(*double_sided_blend_material);
    device.destroy_material(*unlit_material);
    device.destroy_texture(*normal_texture);
    device.destroy_texture(*metallic_roughness_texture);
    device.destroy_texture(*occlusion_texture);
    device.destroy_texture(*emissive_texture);
    device.destroy_mesh(*mesh);

    for (int frame = 0; frame < 3; ++frame) {
        device.begin_frame(32, 32);
        device.end_frame();
    }

    return 0;
}
