#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

#include <array>
#include <iostream>
#include <vector>

#include <SDL2/SDL.h>

#include "stellar/platform/Window.hpp"

namespace {

stellar::assets::MeshAsset make_upload_mesh() {
    stellar::assets::MeshPrimitive primitive;
    primitive.vertices.resize(3);
    primitive.vertices[0].position = {-0.5F, -0.5F, 0.0F};
    primitive.vertices[1].position = {0.5F, -0.5F, 0.0F};
    primitive.vertices[2].position = {0.0F, 0.5F, 0.0F};
    primitive.vertices[0].uv0 = {0.0F, 0.0F};
    primitive.vertices[1].uv0 = {1.0F, 0.0F};
    primitive.vertices[2].uv0 = {0.5F, 1.0F};
    primitive.indices = {0, 1, 2};
    primitive.bounds_min = {-0.5F, -0.5F, 0.0F};
    primitive.bounds_max = {0.5F, 0.5F, 0.0F};

    stellar::assets::MeshPrimitive colored = primitive;
    colored.has_colors = true;
    colored.vertices[0].color = {1.0F, 0.0F, 0.0F, 1.0F};
    colored.vertices[1].color = {0.0F, 1.0F, 0.0F, 1.0F};
    colored.vertices[2].color = {0.0F, 0.0F, 1.0F, 1.0F};

    stellar::assets::MeshPrimitive skinned = primitive;
    skinned.has_skinning = true;
    for (stellar::assets::StaticVertex& vertex : skinned.vertices) {
        vertex.joints0 = {0, 0, 0, 0};
        vertex.weights0 = {1.0F, 0.0F, 0.0F, 0.0F};
    }

    return stellar::assets::MeshAsset{.name = "vulkan_upload_smoke",
                                       .primitives = {primitive, colored, skinned}};
}

stellar::graphics::TextureUpload make_white_rgb_texture() {
    stellar::assets::ImageAsset image;
    image.name = "vulkan_white_rgb";
    image.width = 2;
    image.height = 2;
    image.format = stellar::assets::ImageFormat::kR8G8B8;
    image.pixels = {255, 255, 255, 255, 255, 255,
                    255, 255, 255, 255, 255, 255};
    return stellar::graphics::TextureUpload{.image = image,
                                            .color_space = stellar::assets::TextureColorSpace::kSrgb};
}

stellar::graphics::TextureUpload make_normal_rgba_texture() {
    stellar::assets::ImageAsset image;
    image.name = "vulkan_flat_normal_rgba";
    image.width = 2;
    image.height = 2;
    image.format = stellar::assets::ImageFormat::kR8G8B8A8;
    image.pixels = {128, 128, 255, 255, 128, 128, 255, 255,
                    128, 128, 255, 255, 128, 128, 255, 255};
    return stellar::graphics::TextureUpload{.image = image,
                                            .color_space = stellar::assets::TextureColorSpace::kLinear};
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

    auto mesh = device.create_mesh(make_upload_mesh());
    if (!mesh) {
        std::cerr << mesh.error().message << '\n';
        return 1;
    }

    auto white_texture = device.create_texture(make_white_rgb_texture());
    if (!white_texture) {
        std::cerr << white_texture.error().message << '\n';
        return 1;
    }

    auto normal_texture = device.create_texture(make_normal_rgba_texture());
    if (!normal_texture) {
        std::cerr << normal_texture.error().message << '\n';
        return 1;
    }

    stellar::graphics::MaterialUpload textured_material_upload;
    textured_material_upload.base_color_texture = stellar::graphics::MaterialTextureBinding{
        .texture = *white_texture,
        .sampler = stellar::assets::SamplerAsset{
            .name = "nearest_repeat",
            .mag_filter = stellar::assets::TextureFilter::kNearest,
            .min_filter = stellar::assets::TextureFilter::kNearestMipmapNearest,
            .wrap_s = stellar::assets::TextureWrapMode::kRepeat,
            .wrap_t = stellar::assets::TextureWrapMode::kClampToEdge,
        },
        .texcoord_set = 0,
    };
    auto textured_material = device.create_material(textured_material_upload);
    if (!textured_material) {
        std::cerr << textured_material.error().message << '\n';
        return 1;
    }

    stellar::graphics::MaterialUpload material_upload;
    material_upload.material.base_color_factor = {0.85F, 0.9F, 1.0F, 1.0F};
    auto material = device.create_material(material_upload);
    if (!material) {
        std::cerr << material.error().message << '\n';
        return 1;
    }

    const std::array<float, 16> identity4{1.0F, 0.0F, 0.0F, 0.0F,
                                          0.0F, 1.0F, 0.0F, 0.0F,
                                          0.0F, 0.0F, 1.0F, 0.0F,
                                          0.0F, 0.0F, 0.0F, 1.0F};
    const std::array<float, 9> identity3{1.0F, 0.0F, 0.0F,
                                         0.0F, 1.0F, 0.0F,
                                         0.0F, 0.0F, 1.0F};
    const stellar::graphics::MeshDrawTransforms transforms{.mvp = identity4,
                                                           .world = identity4,
                                                           .normal = identity3};
    const std::array<float, 16> skin_matrix{1.0F, 0.0F, 0.0F, 0.0F,
                                            0.0F, 1.0F, 0.0F, 0.0F,
                                            0.0F, 0.0F, 1.0F, 0.0F,
                                            0.1F, 0.0F, 0.0F, 1.0F};
    const std::array<std::array<float, 16>, 1> skin_palette{skin_matrix};
    std::vector<std::array<float, 16>> over_limit_skin_palette(97, identity4);
    const stellar::graphics::MeshPrimitiveDrawCommand commands[] = {
        stellar::graphics::MeshPrimitiveDrawCommand{.primitive_index = 0, .material = *material},
        stellar::graphics::MeshPrimitiveDrawCommand{.primitive_index = 0,
                                                   .material = *textured_material},
        stellar::graphics::MeshPrimitiveDrawCommand{.primitive_index = 1},
        stellar::graphics::MeshPrimitiveDrawCommand{.primitive_index = 2,
                                                    .material = *material,
                                                   .skin_joint_matrices = skin_palette},
        stellar::graphics::MeshPrimitiveDrawCommand{
            .primitive_index = 2,
            .material = *material,
            .skin_joint_matrices = std::span<const std::array<float, 16>>{
                over_limit_skin_palette.data(), over_limit_skin_palette.size()}},
    };

    for (int frame = 0; frame < 3; ++frame) {
        device.begin_frame(16, 16);
        device.draw_mesh(*mesh, commands, transforms);
        device.end_frame();
    }

    // Hidden SDL Vulkan windows do not always exercise the compositor's real resize/out-of-date
    // present path, but changing the SDL window size still drives the renderer's normal
    // SDL-size-based recreate path when the platform reports the new hidden-window extent.
    SDL_SetWindowSize(window.native_handle(), 32, 32);
    device.begin_frame(32, 32);
    device.draw_mesh(*mesh, commands, transforms);
    device.end_frame();

    device.destroy_texture(*white_texture);
    device.begin_frame(32, 32);
    device.draw_mesh(*mesh, commands, transforms);
    device.end_frame();

    device.destroy_material(*material);
    device.destroy_material(*textured_material);
    device.destroy_texture(*normal_texture);
    device.destroy_mesh(*mesh);

    device.begin_frame(32, 32);
    device.end_frame();

    return 0;
}
