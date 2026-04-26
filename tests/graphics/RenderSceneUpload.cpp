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
                   const std::array<float, 16>& /*mvp*/) noexcept override {
        drew_mesh = static_cast<bool>(mesh) && materials.size() == 1 &&
                    static_cast<bool>(materials[0]);
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
    scene.images.push_back(stellar::assets::ImageAsset{
        .width = 1,
        .height = 1,
        .format = stellar::assets::ImageFormat::kR8G8B8A8,
        .pixels = {255, 255, 255, 255},
    });
    scene.samplers.push_back(stellar::assets::SamplerAsset{
        .name = "nearest_clamp",
        .mag_filter = stellar::assets::TextureFilter::kNearest,
        .min_filter = stellar::assets::TextureFilter::kNearest,
        .wrap_s = stellar::assets::TextureWrapMode::kClampToEdge,
        .wrap_t = stellar::assets::TextureWrapMode::kClampToEdge,
    });
    scene.textures.push_back(stellar::assets::TextureAsset{
        .name = "base_color",
        .image_index = 0,
        .sampler_index = 0,
    });
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "textured",
        .base_color_factor = {0.5F, 0.5F, 0.5F, 1.0F},
        .base_color_texture = stellar::assets::MaterialTextureSlot{.texture_index = 0,
                                                                   .texcoord_set = 0},
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
    scene.nodes.push_back(stellar::scene::Node{.name = "root",
                                               .mesh_instances = {stellar::scene::MeshInstance{0}}});
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
    assert(mock_ptr->material_uploads[0].material.double_sided);

    const std::array<float, 16> identity{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                         0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    render_scene.render(64, 64, identity);
    assert(mock_ptr->began_frame);
    assert(mock_ptr->drew_mesh);
    assert(mock_ptr->ended_frame);

    return 0;
}
