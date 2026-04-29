#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "stellar/assets/SceneAsset.hpp"

namespace stellar::graphics::testing::fixtures {

inline stellar::assets::ImageAsset make_rgba_image(std::string name,
                                                   std::array<std::uint8_t, 4> rgba) {
    return stellar::assets::ImageAsset{.name = std::move(name),
                                       .width = 1,
                                       .height = 1,
                                       .format = stellar::assets::ImageFormat::kR8G8B8A8,
                                       .pixels = {rgba[0], rgba[1], rgba[2], rgba[3]}};
}

inline stellar::assets::MeshPrimitive make_triangle(std::size_t material_index,
                                                    float z_offset,
                                                    bool has_uv1,
                                                    bool has_colors,
                                                    bool has_tangents,
                                                    bool has_skinning) {
    stellar::assets::MeshPrimitive primitive;
    primitive.vertices.resize(3);
    primitive.vertices[0].position = {-0.5F, -0.5F, z_offset};
    primitive.vertices[1].position = {0.5F, -0.5F, z_offset};
    primitive.vertices[2].position = {0.0F, 0.5F, z_offset};
    primitive.vertices[0].uv0 = {0.0F, 0.0F};
    primitive.vertices[1].uv0 = {1.0F, 0.0F};
    primitive.vertices[2].uv0 = {0.5F, 1.0F};
    primitive.bounds_min = {-0.5F, -0.5F, z_offset};
    primitive.bounds_max = {0.5F, 0.5F, z_offset};
    primitive.indices = {0, 1, 2};
    primitive.material_index = material_index;
    if (has_uv1) {
        primitive.vertices[0].uv1 = {1.0F, 1.0F};
        primitive.vertices[1].uv1 = {0.0F, 1.0F};
        primitive.vertices[2].uv1 = {0.5F, 0.0F};
    }
    if (has_colors) {
        primitive.has_colors = true;
        primitive.vertices[0].color = {1.0F, 0.0F, 0.0F, 1.0F};
        primitive.vertices[1].color = {0.0F, 1.0F, 0.0F, 1.0F};
        primitive.vertices[2].color = {0.0F, 0.0F, 1.0F, 1.0F};
    }
    if (has_tangents) {
        primitive.has_tangents = true;
        for (auto& vertex : primitive.vertices) {
            vertex.tangent = {1.0F, 0.0F, 0.0F, 1.0F};
        }
    }
    if (has_skinning) {
        primitive.has_skinning = true;
        for (auto& vertex : primitive.vertices) {
            vertex.joints0 = {0, 0, 0, 0};
            vertex.weights0 = {1.0F, 0.0F, 0.0F, 0.0F};
        }
    }
    return primitive;
}

inline stellar::assets::SceneAsset make_representative_render_scene() {
    stellar::assets::SceneAsset scene;
    scene.images.push_back(make_rgba_image("white", {255, 255, 255, 255}));
    scene.images.push_back(make_rgba_image("normal_flat", {128, 128, 255, 255}));
    scene.samplers.push_back(stellar::assets::SamplerAsset{
        .name = "nearest",
        .mag_filter = stellar::assets::TextureFilter::kNearest,
        .min_filter = stellar::assets::TextureFilter::kNearest});
    scene.textures.push_back(stellar::assets::TextureAsset{
        .name = "base_color",
        .image_index = 0,
        .sampler_index = 0,
        .color_space = stellar::assets::TextureColorSpace::kSrgb});
    scene.textures.push_back(stellar::assets::TextureAsset{
        .name = "normal",
        .image_index = 1,
        .sampler_index = 0,
        .color_space = stellar::assets::TextureColorSpace::kLinear});

    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "textured",
        .base_color_texture = stellar::assets::MaterialTextureSlot{.texture_index = 0}});
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "alpha_mask",
        .base_color_factor = {1.0F, 1.0F, 1.0F, 0.4F},
        .alpha_mode = stellar::assets::AlphaMode::kMask,
        .alpha_cutoff = 0.5F});
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "alpha_blend",
        .base_color_factor = {1.0F, 1.0F, 1.0F, 0.5F},
        .alpha_mode = stellar::assets::AlphaMode::kBlend});
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "uv1_textured",
        .base_color_texture = stellar::assets::MaterialTextureSlot{.texture_index = 0,
                                                                    .texcoord_set = 1}});
    scene.materials.push_back(stellar::assets::MaterialAsset{.name = "vertex_color"});
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "normal_map",
        .base_color_texture = stellar::assets::MaterialTextureSlot{.texture_index = 0},
        .normal_texture = stellar::assets::MaterialTextureSlot{.texture_index = 1,
                                                               .scale = 1.0F}});
    scene.materials.push_back(stellar::assets::MaterialAsset{.name = "skinned"});

    scene.meshes.push_back(stellar::assets::MeshAsset{
        .name = "representative_mesh",
        .primitives = {
            make_triangle(0, 0.0F, false, false, false, false),
            make_triangle(1, -0.1F, false, false, false, false),
            make_triangle(2, -3.0F, false, false, false, false),
            make_triangle(3, -0.2F, true, false, false, false),
            make_triangle(4, -0.3F, false, true, false, false),
            make_triangle(5, -0.4F, false, false, true, false),
            make_triangle(6, -0.5F, false, false, false, true),
        }});
    scene.skins.push_back(stellar::assets::SkinAsset{.name = "one_joint", .joints = {1}});

    stellar::scene::Node mesh_node;
    mesh_node.name = "mesh";
    mesh_node.mesh_instances = {stellar::scene::MeshInstance{0}};
    mesh_node.skin_index = 0;
    stellar::scene::Node joint_node;
    joint_node.name = "joint";
    scene.nodes = {mesh_node, joint_node};
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0, 1}});
    scene.default_scene_index = 0;
    return scene;
}

} // namespace stellar::graphics::testing::fixtures
