#include "stellar/graphics/RenderScene.hpp"

#include <cassert>
#include <expected>
#include <memory>
#include <span>
#include <vector>

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
        drew_mesh = static_cast<bool>(mesh) && commands.size() == 1 &&
                    static_cast<bool>(commands[0].material) && transforms.mvp[0] == 2.0F &&
                    transforms.world[5] == 3.0F && transforms.normal[8] == 0.25F;
        draw_order.push_back(commands[0].primitive_index);
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
                                                                    .texcoord_set = 1},
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
    assert(mock_ptr->material_uploads[0].base_color_texture->texcoord_set == 1);
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
    assert(mock_ptr->ended_frame);

    return 0;
}
