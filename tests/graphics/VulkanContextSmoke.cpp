#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

#include <array>
#include <iostream>

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

    return stellar::assets::MeshAsset{.name = "vulkan_upload_smoke",
                                      .primitives = {primitive, colored}};
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
    const stellar::graphics::MeshPrimitiveDrawCommand commands[] = {
        stellar::graphics::MeshPrimitiveDrawCommand{.primitive_index = 0, .material = *material},
        stellar::graphics::MeshPrimitiveDrawCommand{.primitive_index = 1},
    };

    for (int frame = 0; frame < 3; ++frame) {
        device.begin_frame(16, 16);
        device.draw_mesh(*mesh, commands, transforms);
        device.end_frame();
    }

    device.destroy_material(*material);
    device.destroy_mesh(*mesh);

    return 0;
}
