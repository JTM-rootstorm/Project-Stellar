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
        return stellar::graphics::MeshHandle{next_handle++};
    }

    std::expected<stellar::graphics::TextureHandle, stellar::platform::Error>
    create_texture(const stellar::assets::ImageAsset& image) override {
        uploaded_image_width = image.width;
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
                   std::span<const stellar::graphics::MaterialHandle> materials,
                   const stellar::graphics::MeshDrawTransforms& transforms) noexcept override {
        drew_mesh = static_cast<bool>(mesh) && materials.size() == 1 &&
                    static_cast<bool>(materials[0]) && transforms.mvp[0] == 2.0F &&
                    transforms.world[5] == 3.0F && transforms.normal[8] == 0.25F;
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
            .width = 1,
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
            .image_index = texture_index,
            .sampler_index = texture_index,
        });
    }
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "textured",
        .base_color_factor = {0.5F, 0.5F, 0.5F, 1.0F},
        .base_color_texture = stellar::assets::MaterialTextureSlot{.texture_index = 0,
                                                                    .texcoord_set = 0},
        .normal_texture = stellar::assets::MaterialTextureSlot{.texture_index = 1,
                                                               .texcoord_set = 0},
        .metallic_roughness_texture = stellar::assets::MaterialTextureSlot{.texture_index = 2,
                                                                           .texcoord_set = 0},
        .metallic_factor = 0.75F,
        .roughness_factor = 0.35F,
        .alpha_mode = stellar::assets::AlphaMode::kMask,
        .alpha_cutoff = 0.25F,
        .double_sided = true,
    });

    stellar::assets::MeshPrimitive primitive;
    primitive.vertices = {
        stellar::assets::StaticVertex{{-0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F},
                                      {0.0F, 0.0F}, {0.0F, 0.0F, 0.0F, 1.0F}},
        stellar::assets::StaticVertex{{0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F},
                                      {1.0F, 0.0F}, {0.0F, 0.0F, 0.0F, 1.0F}},
        stellar::assets::StaticVertex{{0.0F, 0.5F, 0.0F}, {0.0F, 0.0F, 1.0F},
                                      {0.5F, 1.0F}, {0.0F, 0.0F, 0.0F, 1.0F}},
    };
    primitive.indices = {0, 1, 2};
    primitive.material_index = 0;
    scene.meshes.push_back(stellar::assets::MeshAsset{.name = "triangle",
                                                      .primitives = {primitive}});
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
    assert(mock_ptr->uploaded_primitive_count == 1);
    assert(mock_ptr->uploaded_image_width == 1);
    assert(mock_ptr->material_uploads.size() == 1);
    assert(mock_ptr->material_uploads[0].base_color_texture.has_value());
    assert(mock_ptr->material_uploads[0].base_color_texture->texture.value != 0);
    assert(mock_ptr->material_uploads[0].base_color_texture->sampler.wrap_s ==
           stellar::assets::TextureWrapMode::kClampToEdge);
    assert(mock_ptr->material_uploads[0].normal_texture.has_value());
    assert(mock_ptr->material_uploads[0].normal_texture->texture.value != 0);
    assert(mock_ptr->material_uploads[0].normal_texture->sampler.min_filter ==
           stellar::assets::TextureFilter::kLinearMipmapLinear);
    assert(mock_ptr->material_uploads[0].normal_texture->sampler.wrap_t ==
           stellar::assets::TextureWrapMode::kMirroredRepeat);
    assert(mock_ptr->material_uploads[0].metallic_roughness_texture.has_value());
    assert(mock_ptr->material_uploads[0].metallic_roughness_texture->texture.value != 0);
    assert(mock_ptr->material_uploads[0].metallic_roughness_texture->sampler.wrap_s ==
           stellar::assets::TextureWrapMode::kRepeat);
    assert(mock_ptr->material_uploads[0].material.metallic_factor > 0.74F);
    assert(mock_ptr->material_uploads[0].material.roughness_factor < 0.36F);
    assert(mock_ptr->material_uploads[0].material.double_sided);

    const std::array<float, 16> identity{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                         0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    render_scene.render(64, 64, identity);
    assert(mock_ptr->began_frame);
    assert(mock_ptr->drew_mesh);
    assert(mock_ptr->ended_frame);

    return 0;
}
