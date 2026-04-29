#include "stellar/graphics/RenderScene.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "RecordingGraphicsDevice.hpp"
#include "GeneratedRenderFixtures.hpp"
#include "stellar/assets/AnimationAsset.hpp"
#include "stellar/assets/SkinAsset.hpp"
#include "stellar/graphics/DebugCubeMesh.hpp"
#include "stellar/graphics/SceneRenderer.hpp"

namespace {

constexpr std::array<float, 16> kIdentity{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                          0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};

stellar::assets::MeshPrimitive make_primitive(std::optional<std::size_t> material_index,
                                              float center_z,
                                              bool has_skinning = false) {
    stellar::assets::MeshPrimitive primitive;
    primitive.vertices.resize(3);
    primitive.indices = {0, 1, 2};
    primitive.material_index = material_index;
    primitive.has_skinning = has_skinning;
    primitive.bounds_min = {-0.5F, -0.5F, center_z};
    primitive.bounds_max = {0.5F, 0.5F, center_z};
    return primitive;
}

stellar::assets::MaterialAsset make_material(stellar::assets::AlphaMode alpha_mode) {
    stellar::assets::MaterialAsset material;
    material.alpha_mode = alpha_mode;
    return material;
}

void finish_single_node_scene(stellar::assets::SceneAsset& scene) {
    stellar::scene::Node root;
    root.mesh_instances = {stellar::scene::MeshInstance{0}};
    scene.nodes.push_back(root);
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0}});
    scene.default_scene_index = 0;
}

std::pair<stellar::graphics::RenderScene*, stellar::graphics::testing::RecordingGraphicsDevice*>
initialize_scene(stellar::graphics::RenderScene& render_scene,
                 stellar::platform::Window& window,
                 stellar::assets::SceneAsset scene) {
    auto device = std::make_unique<stellar::graphics::testing::RecordingGraphicsDevice>();
    auto* device_ptr = device.get();
    auto result = render_scene.initialize(std::move(device), window, std::move(scene));
    assert(result.has_value());
    assert(device_ptr->initialized);
    return {&render_scene, device_ptr};
}

void verify_alpha_grouping_and_blend_sort(stellar::platform::Window& window) {
    stellar::assets::SceneAsset scene;
    scene.materials = {make_material(stellar::assets::AlphaMode::kOpaque),
                       make_material(stellar::assets::AlphaMode::kMask),
                       make_material(stellar::assets::AlphaMode::kBlend)};
    scene.meshes.push_back(stellar::assets::MeshAsset{
        .name = "ordering",
        .primitives = {
            make_primitive(2, -1.0F),
            make_primitive(0, 0.0F),
            make_primitive(2, -5.0F),
            make_primitive(1, 0.0F),
        },
    });
    finish_single_node_scene(scene);

    stellar::graphics::RenderScene render_scene;
    auto [_, device] = initialize_scene(render_scene, window, std::move(scene));
    render_scene.render(320, 200, kIdentity, kIdentity);

    assert(device->draw_calls.size() == 4);
    assert(device->ended_frame_count == 1);
    assert(device->primitive_draw(0).primitive_index == 1);
    assert(device->primitive_draw(1).primitive_index == 3);
    assert(device->primitive_draw(2).primitive_index == 2);
    assert(device->primitive_draw(3).primitive_index == 0);
}

void verify_material_identity_and_invalid_indices(stellar::platform::Window& window) {
    stellar::assets::SceneAsset scene;
    scene.materials = {make_material(stellar::assets::AlphaMode::kOpaque),
                       make_material(stellar::assets::AlphaMode::kMask)};
    scene.meshes.push_back(stellar::assets::MeshAsset{
        .name = "materials",
        .primitives = {
            make_primitive(1, 0.0F),
            make_primitive(99, 0.0F),
            make_primitive(std::nullopt, 0.0F),
            make_primitive(0, 0.0F),
        },
    });
    finish_single_node_scene(scene);

    stellar::graphics::RenderScene render_scene;
    auto [_, device] = initialize_scene(render_scene, window, std::move(scene));
    render_scene.render(64, 64, kIdentity, kIdentity);

    assert(device->material_handles.size() == 2);
    assert(device->draw_calls.size() == 4);
    assert(device->primitive_draw(0).material == device->material_handles[1]);
    assert(!device->primitive_draw(1).material);
    assert(!device->primitive_draw(2).material);
    assert(device->primitive_draw(3).material == device->material_handles[0]);
}

void verify_texture_transform_material_records(stellar::platform::Window& window) {
    stellar::assets::SceneAsset scene;
    scene.images.push_back(stellar::assets::ImageAsset{.width = 1,
                                                       .height = 1,
                                                       .format = stellar::assets::ImageFormat::kR8G8B8A8,
                                                       .pixels = {255, 255, 255, 255}});
    scene.textures.push_back(stellar::assets::TextureAsset{.image_index = 0});
    stellar::assets::MaterialTextureSlot base_slot;
    base_slot.texture_index = 0;
    base_slot.texcoord_set = 1;
    base_slot.transform.enabled = true;
    base_slot.transform.offset = {0.25F, 0.5F};
    base_slot.transform.rotation = 0.75F;
    base_slot.transform.scale = {2.0F, 0.5F};
    base_slot.transform.texcoord_set = 0U;
    scene.materials.push_back(stellar::assets::MaterialAsset{.base_color_texture = base_slot});
    scene.meshes.push_back(stellar::assets::MeshAsset{
        .name = "texture_transform",
        .primitives = {make_primitive(0, 0.0F)},
    });
    finish_single_node_scene(scene);

    stellar::graphics::RenderScene render_scene;
    auto [_, device] = initialize_scene(render_scene, window, std::move(scene));
    render_scene.render(64, 64, kIdentity, kIdentity);

    assert(device->uploaded_materials.size() == 1);
    assert(device->uploaded_materials[0].base_color_texture.has_value());
    assert(device->uploaded_materials[0].base_color_texture->texcoord_set == 0);
    assert(device->uploaded_materials[0].base_color_texture->transform.enabled);
    assert(device->uploaded_materials[0].base_color_texture->transform.offset[1] == 0.5F);
    assert(device->uploaded_materials[0].base_color_texture->transform.scale[0] == 2.0F);
    assert(device->uploaded_materials[0].material.base_color_texture->texcoord_set == 1);
    assert(device->draw_calls.size() == 1);
    assert(device->primitive_draw(0).material == device->material_handles[0]);
}

void verify_texture_transform_uv1_routing(stellar::platform::Window& window) {
    stellar::assets::SceneAsset scene;
    scene.images.push_back(stellar::assets::ImageAsset{.width = 1,
                                                       .height = 1,
                                                       .format = stellar::assets::ImageFormat::kR8G8B8A8,
                                                       .pixels = {255, 255, 255, 255}});
    scene.textures.push_back(stellar::assets::TextureAsset{.image_index = 0});
    stellar::assets::MaterialTextureSlot normal_slot;
    normal_slot.texture_index = 0;
    normal_slot.texcoord_set = 0;
    normal_slot.transform.enabled = true;
    normal_slot.transform.texcoord_set = 1U;
    scene.materials.push_back(stellar::assets::MaterialAsset{.normal_texture = normal_slot});
    scene.meshes.push_back(stellar::assets::MeshAsset{.name = "uv_route",
                                                       .primitives = {make_primitive(0, 0.0F)}});
    finish_single_node_scene(scene);

    stellar::graphics::RenderScene render_scene;
    auto [_, device] = initialize_scene(render_scene, window, std::move(scene));
    assert(device->uploaded_materials[0].normal_texture.has_value());
    assert(device->uploaded_materials[0].normal_texture->texcoord_set == 1);
}

void verify_unlit_material_records(stellar::platform::Window& window) {
    stellar::assets::SceneAsset scene;
    stellar::assets::MaterialAsset material;
    material.unlit = true;
    material.alpha_mode = stellar::assets::AlphaMode::kMask;
    material.alpha_cutoff = 0.33F;
    material.double_sided = true;
    material.base_color_factor = {0.25F, 0.5F, 0.75F, 0.8F};
    scene.materials.push_back(material);
    scene.meshes.push_back(stellar::assets::MeshAsset{.name = "unlit",
                                                       .primitives = {make_primitive(0, 0.0F)}});
    finish_single_node_scene(scene);

    stellar::graphics::RenderScene render_scene;
    auto [_, device] = initialize_scene(render_scene, window, std::move(scene));
    render_scene.render(64, 64, kIdentity, kIdentity);

    assert(device->uploaded_materials.size() == 1);
    assert(device->uploaded_materials[0].material.unlit);
    assert(device->uploaded_materials[0].material.alpha_mode ==
           stellar::assets::AlphaMode::kMask);
    assert(device->uploaded_materials[0].material.alpha_cutoff == 0.33F);
    assert(device->uploaded_materials[0].material.double_sided);
    assert(device->primitive_draw(0).material == device->material_handles[0]);
}

void verify_mixed_static_and_skinned_spans(stellar::platform::Window& window) {
    stellar::assets::SceneAsset scene;
    scene.meshes.push_back(stellar::assets::MeshAsset{
        .name = "mixed_skin",
        .primitives = {make_primitive(std::nullopt, 0.0F),
                       make_primitive(std::nullopt, 0.0F, true)},
    });
    scene.skins.push_back(stellar::assets::SkinAsset{.name = "skin", .joints = {0}});
    stellar::scene::Node root;
    root.mesh_instances = {stellar::scene::MeshInstance{0}};
    root.skin_index = 0;
    scene.nodes.push_back(root);
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0}});
    scene.default_scene_index = 0;

    stellar::graphics::RenderScene render_scene;
    auto [_, device] = initialize_scene(render_scene, window, std::move(scene));

    stellar::scene::ScenePose pose;
    pose.world_transforms = {kIdentity};
    pose.skin_poses.resize(1);
    pose.skin_poses[0].joint_matrices = {std::array<float, 16>{
        2.0F, 0.0F, 0.0F, 0.0F, 0.0F, 3.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 4.0F, 0.0F, 5.0F, 6.0F, 7.0F, 1.0F}};
    render_scene.render(64, 64, kIdentity, kIdentity, pose);
    pose.skin_poses[0].joint_matrices[0][12] = 123.0F;

    assert(device->draw_calls.size() == 2);
    assert(device->primitive_draw(0).skin_joint_matrices.empty());
    assert(device->primitive_draw(1).skin_joint_matrices.size() == 1);
    assert(device->primitive_draw(1).skin_joint_matrices[0][12] == 5.0F);
}

void verify_large_skin_palette_forwarding(stellar::platform::Window& window) {
    constexpr std::size_t kJointCount = 128;
    stellar::assets::SceneAsset scene;
    auto primitive = make_primitive(std::nullopt, 0.0F, true);
    primitive.vertices[0].joints0 = {0, 95, 96, 127};
    primitive.vertices[0].weights0 = {0.25F, 0.25F, 0.25F, 0.25F};
    scene.meshes.push_back(stellar::assets::MeshAsset{.name = "large_skin",
                                                       .primitives = {primitive}});
    stellar::assets::SkinAsset skin;
    for (std::size_t joint = 0; joint < kJointCount; ++joint) {
        skin.joints.push_back(joint + 1);
    }
    scene.skins.push_back(std::move(skin));
    stellar::scene::Node root;
    root.mesh_instances = {stellar::scene::MeshInstance{0}};
    root.skin_index = 0;
    scene.nodes.push_back(root);
    for (std::size_t joint = 0; joint < kJointCount; ++joint) {
        scene.nodes.push_back(stellar::scene::Node{});
    }
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0}});
    scene.default_scene_index = 0;

    stellar::graphics::RenderScene render_scene;
    auto [_, device] = initialize_scene(render_scene, window, std::move(scene));

    stellar::scene::ScenePose pose;
    pose.world_transforms.assign(kJointCount + 1, kIdentity);
    pose.skin_poses.resize(1);
    pose.skin_poses[0].joint_matrices.assign(kJointCount, kIdentity);
    pose.skin_poses[0].joint_matrices[96][12] = 96.0F;
    pose.skin_poses[0].joint_matrices[127][12] = 127.0F;
    render_scene.render(64, 64, kIdentity, kIdentity, pose);

    assert(device->draw_calls.size() == 1);
    assert(device->primitive_draw(0).skin_joint_matrices.size() == kJointCount);
    assert(device->primitive_draw(0).skin_joint_matrices[96][12] == 96.0F);
    assert(device->primitive_draw(0).skin_joint_matrices[127][12] == 127.0F);
}

void verify_large_static_scene_count(stellar::platform::Window& window) {
    constexpr std::size_t kNodeCount = 1024;
    stellar::assets::SceneAsset scene;
    scene.meshes.push_back(stellar::assets::MeshAsset{
        .name = "large_static_mesh",
        .primitives = {make_primitive(std::nullopt, 0.0F)},
    });
    std::vector<std::size_t> root_nodes;
    root_nodes.reserve(kNodeCount);
    for (std::size_t node_index = 0; node_index < kNodeCount; ++node_index) {
        stellar::scene::Node node;
        node.mesh_instances = {stellar::scene::MeshInstance{0}};
        node.local_transform.translation = {static_cast<float>(node_index), 0.0F, 0.0F};
        scene.nodes.push_back(node);
        root_nodes.push_back(node_index);
    }
    scene.scenes.push_back(stellar::scene::Scene{.name = "large", .root_nodes = root_nodes});
    scene.default_scene_index = 0;

    stellar::graphics::RenderScene render_scene;
    auto [_, device] = initialize_scene(render_scene, window, std::move(scene));
    render_scene.render(128, 128, kIdentity, kIdentity);

    assert(device->uploaded_meshes.size() == 1);
    assert(device->draw_calls.size() == kNodeCount);
    assert(device->began_frames.size() == 1);
    assert(device->ended_frame_count == 1);
}

void verify_scene_bounds_and_camera_fit() {
    stellar::assets::SceneAsset scene;
    stellar::assets::MeshPrimitive primitive;
    primitive.vertices.resize(3);
    primitive.indices = {0, 1, 2};
    primitive.bounds_min = {-1.0F, -2.0F, -3.0F};
    primitive.bounds_max = {1.0F, 2.0F, 3.0F};
    scene.meshes.push_back(
        stellar::assets::MeshAsset{.name = "bounds", .primitives = {primitive}});
    stellar::scene::Node root;
    root.local_transform.translation = {10.0F, 0.0F, 0.0F};
    root.local_transform.scale = {2.0F, 1.0F, 1.0F};
    root.mesh_instances = {stellar::scene::MeshInstance{0}};
    scene.nodes.push_back(root);
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0}});
    scene.default_scene_index = 0;

    const auto bounds = stellar::graphics::compute_scene_bounds(scene);
    assert(bounds.min[0] == 8.0F);
    assert(bounds.max[0] == 12.0F);
    assert(bounds.min[1] == -2.0F);
    assert(bounds.max[2] == 3.0F);
    assert(bounds.center[0] == 10.0F);
    assert(bounds.radius > 3.0F);

    const auto camera = stellar::graphics::fit_camera_to_bounds(bounds, 45.0F, 16.0F / 9.0F);
    assert(camera.target[0] == 10.0F);
    assert(camera.eye[2] > bounds.max[2]);
    assert(camera.far_plane > camera.near_plane);

    const auto empty_bounds =
        stellar::graphics::compute_scene_bounds(stellar::assets::SceneAsset{});
    assert(empty_bounds.center[0] == 0.0F);
    assert(empty_bounds.radius == 1.0F);
}

void verify_debug_cube_winding_matches_normals() {
    const auto mesh = stellar::graphics::create_debug_cube_mesh();
    assert(mesh.has_value());
    assert(mesh->primitives.size() == 6);

    for (const auto& primitive : mesh->primitives) {
        assert(primitive.vertices.size() == 4);
        assert(primitive.indices == std::vector<std::uint32_t>({0, 1, 2, 0, 2, 3}));

        const auto& p0 = primitive.vertices[0].position;
        const auto& p1 = primitive.vertices[1].position;
        const auto& p2 = primitive.vertices[2].position;
        const auto edge_a = std::array<float, 3>{p1[0] - p0[0], p1[1] - p0[1],
                                                 p1[2] - p0[2]};
        const auto edge_b = std::array<float, 3>{p2[0] - p0[0], p2[1] - p0[1],
                                                 p2[2] - p0[2]};
        const auto face_normal = std::array<float, 3>{
            edge_a[1] * edge_b[2] - edge_a[2] * edge_b[1],
            edge_a[2] * edge_b[0] - edge_a[0] * edge_b[2],
            edge_a[0] * edge_b[1] - edge_a[1] * edge_b[0],
        };
        const auto& normal = primitive.vertices[0].normal;
        const float winding_dot = face_normal[0] * normal[0] + face_normal[1] * normal[1] +
            face_normal[2] * normal[2];
        assert(winding_dot > 0.0F);
    }
}

void verify_animation_pose_forwarding(stellar::platform::Window& window) {
    stellar::assets::SceneAsset scene;
    scene.meshes.push_back(stellar::assets::MeshAsset{
        .name = "animated", .primitives = {make_primitive(std::nullopt, 0.0F)}});
    stellar::scene::Node root;
    root.mesh_instances = {stellar::scene::MeshInstance{0}};
    scene.nodes.push_back(root);
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0}});
    scene.default_scene_index = 0;
    scene.animations.push_back(stellar::assets::AnimationAsset{
        .name = "move",
        .samplers = {stellar::assets::AnimationSamplerAsset{.input_times = {0.0F, 1.0F},
                                                            .output_values = {0.0F, 0.0F, 0.0F,
                                                                              4.0F, 0.0F, 0.0F},
                                                            .output_components = 3}},
        .channels = {stellar::assets::AnimationChannelAsset{
            .sampler_index = 0,
            .target_node = 0,
            .target_path = stellar::assets::AnimationTargetPath::kTranslation}}});

    stellar::scene::AnimationPlayer player;
    player.bind(scene);
    auto selected = player.select_animation(0);
    assert(selected.has_value());
    player.play();
    auto updated = player.update(0.5F);
    assert(updated.has_value());

    stellar::graphics::RenderScene render_scene;
    auto [_, device] = initialize_scene(render_scene, window, std::move(scene));
    render_scene.render(64, 64, kIdentity, kIdentity, player.pose());

    assert(device->draw_calls.size() == 1);
    assert(device->draw_calls[0].transforms.world[12] > 1.99F);
    assert(device->draw_calls[0].transforms.world[12] < 2.01F);
}

void verify_generated_representative_fixtures(stellar::platform::Window& window) {
    auto scene = stellar::graphics::testing::fixtures::make_representative_render_scene();
    stellar::graphics::RenderScene render_scene;
    auto [_, device] = initialize_scene(render_scene, window, std::move(scene));

    stellar::scene::ScenePose pose;
    pose.world_transforms = {kIdentity, kIdentity};
    pose.skin_poses.resize(1);
    pose.skin_poses[0].joint_matrices = {kIdentity};
    render_scene.render(64, 64, kIdentity, kIdentity, pose);

    assert(device->uploaded_meshes.size() == 1);
    assert(device->uploaded_meshes[0].primitives.size() == 7);
    assert(device->uploaded_textures.size() == 2);
    assert(device->uploaded_materials.size() == 7);
    assert(device->uploaded_meshes[0].primitives[3].vertices[0].uv1[0] == 1.0F);
    assert(device->uploaded_meshes[0].primitives[4].has_colors);
    assert(device->uploaded_meshes[0].primitives[5].has_tangents);
    assert(device->uploaded_meshes[0].primitives[6].has_skinning);
    assert(device->uploaded_materials[1].material.alpha_mode ==
           stellar::assets::AlphaMode::kMask);
    assert(device->uploaded_materials[2].material.alpha_mode ==
           stellar::assets::AlphaMode::kBlend);
    assert(device->uploaded_materials[3].base_color_texture->texcoord_set == 1);
    assert(device->uploaded_materials[5].normal_texture.has_value());
    assert(device->draw_calls.size() == 7);
    bool drew_skinned_primitive = false;
    for (const auto& draw_call : device->draw_calls) {
        if (draw_call.commands[0].primitive_index == 6) {
            drew_skinned_primitive = draw_call.commands[0].skin_joint_matrices.size() == 1;
        }
    }
    assert(drew_skinned_primitive);
}

} // namespace

int main() {
    stellar::platform::Window window;
    verify_alpha_grouping_and_blend_sort(window);
    verify_material_identity_and_invalid_indices(window);
    verify_texture_transform_material_records(window);
    verify_texture_transform_uv1_routing(window);
    verify_unlit_material_records(window);
    verify_mixed_static_and_skinned_spans(window);
    verify_large_skin_palette_forwarding(window);
    verify_large_static_scene_count(window);
    verify_scene_bounds_and_camera_fit();
    verify_debug_cube_winding_matches_normals();
    verify_animation_pose_forwarding(window);
    verify_generated_representative_fixtures(window);
    return 0;
}
