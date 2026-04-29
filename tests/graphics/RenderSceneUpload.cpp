#include "stellar/graphics/RenderScene.hpp"

#include <cassert>
#include <expected>
#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "stellar/assets/SkinAsset.hpp"

namespace {

class MockGraphicsDevice final : public stellar::graphics::GraphicsDevice {
public:
    std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& /*window*/) override {
        initialized = true;
        return {};
    }

    std::expected<stellar::graphics::MeshHandle, stellar::platform::Error>
    create_mesh(const stellar::assets::MeshAsset& mesh) override {
        uploaded_primitive_count = mesh.primitives.size();
        uploaded_first_primitive_has_colors = !mesh.primitives.empty() && mesh.primitives[0].has_colors;
        std::vector<std::uint16_t> mesh_max_joint_indices;
        mesh_max_joint_indices.reserve(mesh.primitives.size());
        for (const auto& primitive : mesh.primitives) {
            std::uint16_t max_joint_index = 0;
            if (primitive.has_skinning) {
                for (const auto& vertex : primitive.vertices) {
                    for (std::uint16_t joint : vertex.joints0) {
                        if (joint >= stellar::graphics::kMaxSkinPaletteJoints) {
                            return std::unexpected(stellar::platform::Error(
                                "Mesh primitive skin joint index exceeds 256-joint runtime cap"));
                        }
                        max_joint_index = std::max(max_joint_index, joint);
                    }
                }
            }
            mesh_max_joint_indices.push_back(max_joint_index);
        }
        max_joint_indices.push_back(std::move(mesh_max_joint_indices));
        return stellar::graphics::MeshHandle{next_handle++};
    }

    std::expected<stellar::graphics::TextureHandle, stellar::platform::Error>
    create_texture(const stellar::graphics::TextureUpload& texture) override {
        uploaded_image_width = texture.image.width;
        uploaded_image_widths.push_back(texture.image.width);
        uploaded_texture_color_spaces.push_back(texture.color_space);
        return stellar::graphics::TextureHandle{next_handle++};
    }

    std::expected<stellar::graphics::MaterialHandle, stellar::platform::Error>
    create_material(const stellar::graphics::MaterialUpload& material) override {
        material_uploads.push_back(material);
        return stellar::graphics::MaterialHandle{next_handle++};
    }

    void begin_frame(int width, int height) noexcept override {
        began_frame = width == 64 && height == 64;
    }

    void draw_mesh(stellar::graphics::MeshHandle mesh,
                   std::span<const stellar::graphics::MeshPrimitiveDrawCommand> commands,
                   const stellar::graphics::MeshDrawTransforms& transforms) noexcept override {
        const auto mesh_slot = static_cast<std::size_t>(mesh.value - 1);
        const auto max_joint_index = mesh_slot < max_joint_indices.size() &&
                commands[0].primitive_index < max_joint_indices[mesh_slot].size()
            ? max_joint_indices[mesh_slot][commands[0].primitive_index]
            : 0;
        if (commands[0].skin_joint_matrices.size() > stellar::graphics::kMaxSkinPaletteJoints ||
            (!commands[0].skin_joint_matrices.empty() &&
             commands[0].skin_joint_matrices.size() <= max_joint_index)) {
            ++skipped_skin_draws;
            return;
        }
        drew_mesh = static_cast<bool>(mesh) && commands.size() == 1 &&
                    static_cast<bool>(commands[0].material) && transforms.mvp[0] == 2.0F &&
                    transforms.world[5] == 3.0F && transforms.normal[8] == 0.25F;
        draw_order.push_back(commands[0].primitive_index);
        skin_joint_counts.push_back(commands[0].skin_joint_matrices.size());
        if (!commands[0].skin_joint_matrices.empty()) {
            first_skin_joint_matrix = commands[0].skin_joint_matrices[0];
        }
    }

    void end_frame() noexcept override {
        ended_frame = true;
    }

    void destroy_mesh(stellar::graphics::MeshHandle /*mesh*/) noexcept override {
        ++destroyed_meshes;
    }

    void destroy_texture(stellar::graphics::TextureHandle /*texture*/) noexcept override {
        ++destroyed_textures;
    }

    void destroy_material(stellar::graphics::MaterialHandle /*material*/) noexcept override {
        ++destroyed_materials;
    }

    bool initialized = false;
    bool began_frame = false;
    bool drew_mesh = false;
    bool ended_frame = false;
    std::size_t uploaded_primitive_count = 0;
    std::uint32_t uploaded_image_width = 0;
    bool uploaded_first_primitive_has_colors = false;
    std::vector<std::uint32_t> uploaded_image_widths;
    std::vector<stellar::assets::TextureColorSpace> uploaded_texture_color_spaces;
    std::vector<std::size_t> draw_order;
    std::vector<std::size_t> skin_joint_counts;
    std::array<float, 16> first_skin_joint_matrix{};
    std::vector<std::vector<std::uint16_t>> max_joint_indices;
    int skipped_skin_draws = 0;
    int destroyed_meshes = 0;
    int destroyed_textures = 0;
    int destroyed_materials = 0;
    std::uint64_t next_handle = 1;
    std::vector<stellar::graphics::MaterialUpload> material_uploads;
};

stellar::assets::SceneAsset make_scene() {
    stellar::assets::SceneAsset scene;
    for (int image_index = 0; image_index < 3; ++image_index) {
        scene.images.push_back(stellar::assets::ImageAsset{
            .width = static_cast<std::uint32_t>(image_index + 1),
            .height = 1,
            .format = stellar::assets::ImageFormat::kR8G8B8A8,
            .pixels = {255, 255, 255, 255},
        });
    }
    scene.samplers.push_back(stellar::assets::SamplerAsset{
        .name = "nearest_clamp",
        .mag_filter = stellar::assets::TextureFilter::kNearest,
        .min_filter = stellar::assets::TextureFilter::kNearest,
        .wrap_s = stellar::assets::TextureWrapMode::kClampToEdge,
        .wrap_t = stellar::assets::TextureWrapMode::kClampToEdge,
    });
    scene.samplers.push_back(stellar::assets::SamplerAsset{
        .name = "linear_mirror",
        .mag_filter = stellar::assets::TextureFilter::kLinear,
        .min_filter = stellar::assets::TextureFilter::kLinearMipmapLinear,
        .wrap_s = stellar::assets::TextureWrapMode::kMirroredRepeat,
        .wrap_t = stellar::assets::TextureWrapMode::kMirroredRepeat,
    });
    scene.samplers.push_back(stellar::assets::SamplerAsset{
        .name = "default_repeat",
        .mag_filter = stellar::assets::TextureFilter::kUnspecified,
        .min_filter = stellar::assets::TextureFilter::kUnspecified,
        .wrap_s = stellar::assets::TextureWrapMode::kRepeat,
        .wrap_t = stellar::assets::TextureWrapMode::kRepeat,
    });
    for (std::size_t texture_index = 0; texture_index < 3; ++texture_index) {
        const char* texture_name = "metallic_roughness";
        if (texture_index == 0) {
            texture_name = "base_color";
        } else if (texture_index == 1) {
            texture_name = "normal";
        }
        scene.textures.push_back(stellar::assets::TextureAsset{
            .name = texture_name,
            .image_index = (texture_index + 2) % 3,
            .sampler_index = texture_index,
            .color_space = texture_index == 0 ? stellar::assets::TextureColorSpace::kSrgb
                                               : stellar::assets::TextureColorSpace::kLinear,
        });
    }
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "textured",
        .base_color_factor = {0.5F, 0.5F, 0.5F, 1.0F},
        .base_color_texture = stellar::assets::MaterialTextureSlot{.texture_index = 0,
                                                                    .texcoord_set = 1,
                                                                    .scale = 1.0F,
                                                                    .transform = {
                                                                        .offset = {0.25F, 0.5F},
                                                                        .rotation = 0.125F,
                                                                        .scale = {2.0F, 0.5F},
                                                                        .texcoord_set = 0U,
                                                                        .enabled = true}},
        .normal_texture = stellar::assets::MaterialTextureSlot{.texture_index = 1,
                                                                .texcoord_set = 0,
                                                               .scale = 0.5F},
        .metallic_roughness_texture = stellar::assets::MaterialTextureSlot{.texture_index = 2,
                                                                           .texcoord_set = 2},
        .occlusion_texture = stellar::assets::MaterialTextureSlot{.texture_index = 2,
                                                                  .texcoord_set = 0,
                                                                  .scale = 0.75F},
        .emissive_texture = stellar::assets::MaterialTextureSlot{.texture_index = 0,
                                                                 .texcoord_set = 0},
        .emissive_factor = {0.1F, 0.2F, 0.3F},
        .occlusion_strength = 0.75F,
        .metallic_factor = 0.75F,
        .roughness_factor = 0.35F,
        .alpha_mode = stellar::assets::AlphaMode::kMask,
        .alpha_cutoff = 0.25F,
        .double_sided = true,
        .unlit = true,
    });
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "blend",
        .base_color_factor = {1.0F, 0.75F, 0.5F, 0.4F},
        .alpha_mode = stellar::assets::AlphaMode::kBlend,
    });
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "double_sided",
        .double_sided = true,
    });

    stellar::assets::MeshPrimitive primitive;
    primitive.vertices.resize(3);
    primitive.vertices[0].position = {-0.5F, -0.5F, 0.0F};
    primitive.vertices[0].normal = {0.0F, 0.0F, 1.0F};
    primitive.vertices[0].uv0 = {0.0F, 0.0F};
    primitive.vertices[0].uv1 = {0.25F, 0.25F};
    primitive.vertices[0].color = {1.0F, 0.5F, 0.5F, 1.0F};
    primitive.vertices[1].position = {0.5F, -0.5F, 0.0F};
    primitive.vertices[1].normal = {0.0F, 0.0F, 1.0F};
    primitive.vertices[1].uv0 = {1.0F, 0.0F};
    primitive.vertices[1].uv1 = {0.75F, 0.25F};
    primitive.vertices[1].color = {0.5F, 1.0F, 0.5F, 1.0F};
    primitive.vertices[2].position = {0.0F, 0.5F, 0.0F};
    primitive.vertices[2].normal = {0.0F, 0.0F, 1.0F};
    primitive.vertices[2].uv0 = {0.5F, 1.0F};
    primitive.vertices[2].uv1 = {0.5F, 0.75F};
    primitive.vertices[2].color = {0.5F, 0.5F, 1.0F, 1.0F};
    primitive.indices = {0, 1, 2};
    primitive.has_colors = true;
    primitive.material_index = 0;
    primitive.bounds_min = {-0.5F, -0.5F, 0.0F};
    primitive.bounds_max = {0.5F, 0.5F, 0.0F};
    stellar::assets::MeshPrimitive far_blend = primitive;
    far_blend.material_index = 1;
    far_blend.bounds_min = {-0.5F, -0.5F, -5.0F};
    far_blend.bounds_max = {0.5F, 0.5F, -5.0F};
    stellar::assets::MeshPrimitive near_blend = primitive;
    near_blend.material_index = 1;
    near_blend.bounds_min = {-0.5F, -0.5F, -1.0F};
    near_blend.bounds_max = {0.5F, 0.5F, -1.0F};
    scene.meshes.push_back(stellar::assets::MeshAsset{.name = "triangle",
                                                       .primitives = {primitive, far_blend,
                                                                      near_blend}});
    stellar::scene::Node root_node;
    root_node.name = "root";
    root_node.local_transform.scale = {2.0F, 3.0F, 4.0F};
    root_node.mesh_instances = {stellar::scene::MeshInstance{0}};
    scene.nodes.push_back(root_node);
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0}});
    scene.default_scene_index = 0;
    return scene;
}

stellar::assets::SceneAsset make_single_skinned_scene(std::uint16_t max_joint_index) {
    stellar::assets::SceneAsset scene;
    stellar::assets::MeshPrimitive primitive;
    primitive.vertices.resize(3);
    primitive.vertices[0].joints0 = {0, 1, max_joint_index, max_joint_index};
    primitive.vertices[0].weights0 = {0.25F, 0.25F, 0.25F, 0.25F};
    primitive.indices = {0, 1, 2};
    primitive.has_skinning = true;
    scene.meshes.push_back(stellar::assets::MeshAsset{.name = "skinned",
                                                       .primitives = {primitive}});
    stellar::assets::SkinAsset skin;
    for (std::size_t joint = 0; joint <= max_joint_index; ++joint) {
        skin.joints.push_back(joint + 1);
    }
    scene.skins.push_back(std::move(skin));
    stellar::scene::Node root;
    root.name = "skinned_root";
    root.mesh_instances = {stellar::scene::MeshInstance{0}};
    root.skin_index = 0;
    scene.nodes.push_back(root);
    for (std::size_t joint = 0; joint <= max_joint_index; ++joint) {
        scene.nodes.push_back(stellar::scene::Node{});
    }
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0}});
    scene.default_scene_index = 0;
    return scene;
}

stellar::scene::ScenePose make_pose_with_joint_count(std::size_t joint_count) {
    const std::array<float, 16> identity{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                         0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    stellar::scene::ScenePose pose;
    pose.world_transforms.assign(joint_count + 1, identity);
    pose.skin_poses.resize(1);
    pose.skin_poses[0].joint_matrices.assign(joint_count, identity);
    return pose;
}

} // namespace

int main() {
    stellar::platform::Window window;
    auto mock = std::make_unique<MockGraphicsDevice>();
    MockGraphicsDevice* mock_ptr = mock.get();

    stellar::graphics::RenderScene render_scene;
    auto result = render_scene.initialize(std::move(mock), window, make_scene());
    assert(result.has_value());
    assert(mock_ptr->initialized);
    assert(mock_ptr->uploaded_primitive_count == 3);
    assert(mock_ptr->uploaded_first_primitive_has_colors);
    assert(mock_ptr->uploaded_image_width == 2);
    assert((mock_ptr->uploaded_image_widths == std::vector<std::uint32_t>{3, 1, 2}));
    assert((mock_ptr->uploaded_texture_color_spaces ==
            std::vector<stellar::assets::TextureColorSpace>{
                stellar::assets::TextureColorSpace::kSrgb,
                stellar::assets::TextureColorSpace::kLinear,
                stellar::assets::TextureColorSpace::kLinear}));
    assert(mock_ptr->material_uploads.size() == 3);
    assert(mock_ptr->material_uploads[0].base_color_texture.has_value());
    assert(mock_ptr->material_uploads[0].base_color_texture->texture.value != 0);
    assert(mock_ptr->material_uploads[0].base_color_texture->texcoord_set == 0);
    assert(mock_ptr->material_uploads[0].base_color_texture->transform.enabled);
    assert(mock_ptr->material_uploads[0].base_color_texture->transform.offset[0] == 0.25F);
    assert(mock_ptr->material_uploads[0].base_color_texture->transform.rotation == 0.125F);
    assert(mock_ptr->material_uploads[0].base_color_texture->transform.scale[0] == 2.0F);
    assert(mock_ptr->material_uploads[0].material.base_color_texture->texcoord_set == 1);
    assert(mock_ptr->material_uploads[0].material.base_color_texture->transform.texcoord_set == 0U);
    assert(mock_ptr->material_uploads[0].base_color_texture->sampler.wrap_s ==
           stellar::assets::TextureWrapMode::kClampToEdge);
    assert(mock_ptr->material_uploads[0].normal_texture.has_value());
    assert(mock_ptr->material_uploads[0].normal_texture->texture.value != 0);
    assert(mock_ptr->material_uploads[0].material.normal_texture->scale == 0.5F);
    assert(mock_ptr->material_uploads[0].normal_texture->sampler.min_filter ==
           stellar::assets::TextureFilter::kLinearMipmapLinear);
    assert(mock_ptr->material_uploads[0].normal_texture->sampler.wrap_t ==
           stellar::assets::TextureWrapMode::kMirroredRepeat);
    assert(mock_ptr->material_uploads[0].metallic_roughness_texture.has_value());
    assert(mock_ptr->material_uploads[0].metallic_roughness_texture->texture.value != 0);
    assert(mock_ptr->material_uploads[0].metallic_roughness_texture->texcoord_set == 2);
    assert(mock_ptr->material_uploads[0].occlusion_texture.has_value());
    assert(mock_ptr->material_uploads[0].emissive_texture.has_value());
    assert(mock_ptr->material_uploads[0].material.occlusion_strength == 0.75F);
    assert(mock_ptr->material_uploads[0].material.emissive_factor[2] == 0.3F);
    assert(mock_ptr->material_uploads[0].metallic_roughness_texture->sampler.wrap_s ==
           stellar::assets::TextureWrapMode::kRepeat);
    assert(mock_ptr->material_uploads[0].material.metallic_factor > 0.74F);
    assert(mock_ptr->material_uploads[0].material.roughness_factor < 0.36F);
    assert(mock_ptr->material_uploads[0].material.double_sided);
    assert(mock_ptr->material_uploads[0].material.unlit);
    assert(mock_ptr->material_uploads[1].material.alpha_mode ==
           stellar::assets::AlphaMode::kBlend);
    assert(mock_ptr->material_uploads[1].material.base_color_factor[3] < 0.41F);
    assert(mock_ptr->material_uploads[2].material.double_sided);

    const std::array<float, 16> identity{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                         0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    render_scene.render(64, 64, identity);
    assert(mock_ptr->began_frame);
    assert(mock_ptr->drew_mesh);
    assert((mock_ptr->draw_order == std::vector<std::size_t>{0, 1, 2}));
    assert((mock_ptr->skin_joint_counts == std::vector<std::size_t>{0, 0, 0}));
    assert(mock_ptr->ended_frame);

    stellar::assets::SceneAsset skinned_scene;
    stellar::assets::MeshPrimitive skinned_primitive;
    skinned_primitive.vertices.resize(3);
    skinned_primitive.indices = {0, 1, 2};
    skinned_primitive.has_skinning = true;
    skinned_scene.meshes.push_back(stellar::assets::MeshAsset{.name = "skinned",
                                                               .primitives = {skinned_primitive}});
    skinned_scene.skins.push_back(stellar::assets::SkinAsset{.name = "skin", .joints = {1}});
    stellar::scene::Node skinned_root;
    skinned_root.name = "skinned_root";
    skinned_root.mesh_instances = {stellar::scene::MeshInstance{0}};
    skinned_root.skin_index = 0;
    skinned_root.local_transform.translation = {2.0F, 0.0F, 0.0F};
    stellar::scene::Node joint_node;
    joint_node.name = "joint";
    joint_node.local_transform.translation = {5.0F, 0.0F, 0.0F};
    skinned_scene.nodes = {skinned_root, joint_node};
    skinned_scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0, 1}});
    skinned_scene.default_scene_index = 0;

    auto skinned_mock = std::make_unique<MockGraphicsDevice>();
    MockGraphicsDevice* skinned_mock_ptr = skinned_mock.get();
    stellar::graphics::RenderScene skinned_render_scene;
    auto skinned_result = skinned_render_scene.initialize(std::move(skinned_mock), window,
                                                          skinned_scene);
    assert(skinned_result.has_value());

    stellar::scene::ScenePose pose;
    pose.local_transforms.resize(2);
    pose.world_transforms = {
        std::array<float, 16>{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                              0.0F, 0.0F, 1.0F, 0.0F, 2.0F, 0.0F, 0.0F, 1.0F},
        std::array<float, 16>{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                              0.0F, 0.0F, 1.0F, 0.0F, 7.0F, 0.0F, 0.0F, 1.0F},
    };
    pose.skin_poses.resize(1);
    pose.skin_poses[0].joint_matrices = {
        std::array<float, 16>{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                              0.0F, 0.0F, 1.0F, 0.0F, 7.0F, 0.0F, 0.0F, 1.0F},
    };
    skinned_render_scene.render(64, 64, identity, identity, pose);
    assert((skinned_mock_ptr->skin_joint_counts == std::vector<std::size_t>{1}));
    assert(skinned_mock_ptr->first_skin_joint_matrix[12] == 5.0F);

    auto old_limit_mock = std::make_unique<MockGraphicsDevice>();
    MockGraphicsDevice* old_limit_mock_ptr = old_limit_mock.get();
    stellar::graphics::RenderScene old_limit_render_scene;
    auto old_limit_result = old_limit_render_scene.initialize(
        std::move(old_limit_mock), window, make_single_skinned_scene(95));
    assert(old_limit_result.has_value());
    auto old_limit_pose = make_pose_with_joint_count(96);
    old_limit_render_scene.render(64, 64, identity, identity, old_limit_pose);
    assert((old_limit_mock_ptr->skin_joint_counts == std::vector<std::size_t>{96}));
    assert(old_limit_mock_ptr->skipped_skin_draws == 0);

    auto larger_mock = std::make_unique<MockGraphicsDevice>();
    MockGraphicsDevice* larger_mock_ptr = larger_mock.get();
    stellar::graphics::RenderScene larger_render_scene;
    auto larger_result = larger_render_scene.initialize(
        std::move(larger_mock), window, make_single_skinned_scene(127));
    assert(larger_result.has_value());
    auto larger_pose = make_pose_with_joint_count(128);
    larger_render_scene.render(64, 64, identity, identity, larger_pose);
    assert((larger_mock_ptr->skin_joint_counts == std::vector<std::size_t>{128}));
    assert(larger_mock_ptr->skipped_skin_draws == 0);

    auto undersized_mock = std::make_unique<MockGraphicsDevice>();
    MockGraphicsDevice* undersized_mock_ptr = undersized_mock.get();
    stellar::graphics::RenderScene undersized_render_scene;
    auto undersized_result = undersized_render_scene.initialize(
        std::move(undersized_mock), window, make_single_skinned_scene(127));
    assert(undersized_result.has_value());
    auto undersized_pose = make_pose_with_joint_count(96);
    undersized_render_scene.render(64, 64, identity, identity, undersized_pose);
    assert(undersized_mock_ptr->skin_joint_counts.empty());
    assert(undersized_mock_ptr->skipped_skin_draws == 1);

    auto over_cap_mock = std::make_unique<MockGraphicsDevice>();
    stellar::graphics::RenderScene over_cap_render_scene;
    auto over_cap_result = over_cap_render_scene.initialize(
        std::move(over_cap_mock), window,
        make_single_skinned_scene(static_cast<std::uint16_t>(
            stellar::graphics::kMaxSkinPaletteJoints)));
    assert(!over_cap_result.has_value());
    assert(over_cap_result.error().message.find("256-joint runtime cap") != std::string::npos);

    return 0;
}
