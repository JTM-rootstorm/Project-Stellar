#include "stellar/graphics/RenderLevel.hpp"

#include <array>
#include <cassert>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace {

class MockGraphicsDevice final : public stellar::graphics::GraphicsDevice {
public:
  std::expected<void, stellar::platform::Error>
  initialize(stellar::platform::Window & /*window*/) override {
    initialized = true;
    return {};
  }

  std::expected<stellar::graphics::MeshHandle, stellar::platform::Error>
  create_mesh(const stellar::assets::MeshAsset &mesh) override {
    uploaded_meshes.push_back(mesh);
    return stellar::graphics::MeshHandle{next_handle++};
  }

  std::expected<stellar::graphics::TextureHandle, stellar::platform::Error>
  create_texture(const stellar::graphics::TextureUpload &texture) override {
    uploaded_textures.push_back(texture);
    return stellar::graphics::TextureHandle{next_handle++};
  }

  std::expected<stellar::graphics::MaterialHandle, stellar::platform::Error>
  create_material(const stellar::graphics::MaterialUpload &material) override {
    material_uploads.push_back(material);
    return stellar::graphics::MaterialHandle{next_handle++};
  }

  void begin_frame(int width, int height) noexcept override {
    began_frame = width == 64 && height == 64;
  }

  void draw_mesh(
      stellar::graphics::MeshHandle mesh,
      std::span<const stellar::graphics::MeshPrimitiveDrawCommand> commands,
      const stellar::graphics::MeshDrawTransforms &transforms) noexcept
      override {
    drew_mesh = static_cast<bool>(mesh) && commands.size() == 1 &&
                transforms.mvp[0] == 2.0F && transforms.world[0] == 1.0F &&
                transforms.normal[8] == 1.0F;
    draw_order.push_back(commands[0].primitive_index);
    draw_materials.push_back(commands[0].material);
    skin_joint_counts.push_back(commands[0].skin_joint_matrices.size());
  }

  void end_frame() noexcept override { ended_frame = true; }

  void destroy_mesh(stellar::graphics::MeshHandle /*mesh*/) noexcept override {
    ++destroyed_meshes;
  }

  void destroy_texture(
      stellar::graphics::TextureHandle /*texture*/) noexcept override {
    ++destroyed_textures;
  }

  void destroy_material(
      stellar::graphics::MaterialHandle /*material*/) noexcept override {
    ++destroyed_materials;
  }

  bool initialized = false;
  bool began_frame = false;
  bool drew_mesh = false;
  bool ended_frame = false;
  std::vector<stellar::assets::MeshAsset> uploaded_meshes;
  std::vector<stellar::graphics::TextureUpload> uploaded_textures;
  std::vector<stellar::graphics::MaterialUpload> material_uploads;
  std::vector<std::size_t> draw_order;
  std::vector<stellar::graphics::MaterialHandle> draw_materials;
  std::vector<std::size_t> skin_joint_counts;
  int destroyed_meshes = 0;
  int destroyed_textures = 0;
  int destroyed_materials = 0;
  std::uint64_t next_handle = 1;
};

stellar::assets::MeshPrimitive
make_primitive(std::optional<std::size_t> material_index,
               float center_z = 0.0F) {
  stellar::assets::MeshPrimitive primitive;
  primitive.vertices.resize(3);
  primitive.vertices[0].position = {-0.5F, -0.5F, center_z};
  primitive.vertices[0].normal = {0.0F, 0.0F, 1.0F};
  primitive.vertices[0].uv0 = {0.0F, 0.0F};
  primitive.vertices[0].color = {1.0F, 0.5F, 0.5F, 1.0F};
  primitive.vertices[1].position = {0.5F, -0.5F, center_z};
  primitive.vertices[1].normal = {0.0F, 0.0F, 1.0F};
  primitive.vertices[1].uv0 = {1.0F, 0.0F};
  primitive.vertices[1].color = {0.5F, 1.0F, 0.5F, 1.0F};
  primitive.vertices[2].position = {0.0F, 0.5F, center_z};
  primitive.vertices[2].normal = {0.0F, 0.0F, 1.0F};
  primitive.vertices[2].uv0 = {0.5F, 1.0F};
  primitive.vertices[2].color = {0.5F, 0.5F, 1.0F, 1.0F};
  primitive.indices = {0, 1, 2};
  primitive.has_colors = true;
  primitive.material_index = material_index;
  primitive.bounds_min = {-0.5F, -0.5F, center_z};
  primitive.bounds_max = {0.5F, 0.5F, center_z};
  return primitive;
}

stellar::assets::LevelAsset make_level() {
  stellar::assets::LevelAsset level;
  level.source_uri = "synthetic:test.bsp";
  level.geometry.images.push_back(stellar::assets::ImageAsset{
      .width = 4,
      .height = 1,
      .format = stellar::assets::ImageFormat::kR8G8B8A8,
      .pixels = {255, 255, 255, 255},
  });
  level.geometry.samplers.push_back(stellar::assets::SamplerAsset{
      .name = "nearest_clamp",
      .mag_filter = stellar::assets::TextureFilter::kNearest,
      .min_filter = stellar::assets::TextureFilter::kNearest,
      .wrap_s = stellar::assets::TextureWrapMode::kClampToEdge,
      .wrap_t = stellar::assets::TextureWrapMode::kClampToEdge,
  });
  level.geometry.textures.push_back(stellar::assets::TextureAsset{
      .name = "bsp_embedded_mip",
      .image_index = 0,
      .sampler_index = 0,
      .color_space = stellar::assets::TextureColorSpace::kSrgb,
  });
  level.geometry.materials.push_back(stellar::assets::LevelSurfaceMaterial{
      .name = "stone",
      .texture_index = 0,
      .source_name = "STONE1",
  });
  level.geometry.materials.push_back(stellar::assets::LevelSurfaceMaterial{
      .name = "missing_texture",
      .texture_index = 99,
      .source_name = "MISSING",
  });
  level.geometry.meshes.push_back(stellar::assets::MeshAsset{
      .name = "bsp_static_world",
      .primitives = {make_primitive(0), make_primitive(1),
                     make_primitive(std::nullopt)},
  });
  for (std::size_t primitive_index = 0; primitive_index < 3;
       ++primitive_index) {
    const auto &primitive =
        level.geometry.meshes[0].primitives[primitive_index];
    std::optional<std::size_t> material_index;
    if (primitive_index == 0) {
      material_index = 0;
    } else if (primitive_index == 1) {
      material_index = 1;
    }
    level.geometry.surfaces.push_back(stellar::assets::LevelSurface{
        .name = "surface",
        .mesh_index = 0,
        .primitive_index = primitive_index,
        .material_index = material_index,
        .bounds_min = primitive.bounds_min,
        .bounds_max = primitive.bounds_max,
    });
  }
  return level;
}

} // namespace

int main() {
  stellar::platform::Window window;
  auto mock = std::make_unique<MockGraphicsDevice>();
  MockGraphicsDevice *mock_ptr = mock.get();

  stellar::graphics::RenderLevel render_level;
  auto result = render_level.initialize(std::move(mock), window, make_level());
  assert(result.has_value());
  assert(mock_ptr->initialized);
  assert(mock_ptr->uploaded_meshes.size() == 1);
  assert(mock_ptr->uploaded_meshes[0].primitives.size() == 3);
  assert(mock_ptr->uploaded_meshes[0].primitives[0].has_colors);
  assert(mock_ptr->uploaded_textures.size() == 1);
  assert(mock_ptr->uploaded_textures[0].image.width == 4);
  assert(mock_ptr->uploaded_textures[0].color_space ==
         stellar::assets::TextureColorSpace::kSrgb);
  assert(mock_ptr->material_uploads.size() == 3);
  assert(mock_ptr->material_uploads[0].material.name ==
         "stellar_default_level_material");
  assert(mock_ptr->material_uploads[1].material.name == "stone");
  assert(mock_ptr->material_uploads[1].base_color_texture.has_value());
  assert(mock_ptr->material_uploads[1].base_color_texture->sampler.wrap_s ==
         stellar::assets::TextureWrapMode::kClampToEdge);
  assert(mock_ptr->material_uploads[1].material.metallic_factor == 0.0F);
  assert(mock_ptr->material_uploads[1].material.unlit);
  assert(mock_ptr->material_uploads[2].material.name == "missing_texture");
  assert(!mock_ptr->material_uploads[2].base_color_texture.has_value());

  const std::array<float, 16> vp{2.0F, 0.0F, 0.0F, 0.0F, 0.0F, 2.0F,
                                 0.0F, 0.0F, 0.0F, 0.0F, 2.0F, 0.0F,
                                 0.0F, 0.0F, 0.0F, 1.0F};
  const std::array<float, 16> identity{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                       0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F,
                                       0.0F, 0.0F, 0.0F, 1.0F};
  render_level.render(64, 64, vp, identity);
  assert(mock_ptr->began_frame);
  assert(mock_ptr->drew_mesh);
  assert((mock_ptr->draw_order == std::vector<std::size_t>{0, 1, 2}));
  assert(mock_ptr->draw_materials[0] == stellar::graphics::MaterialHandle{4});
  assert(mock_ptr->draw_materials[1] == stellar::graphics::MaterialHandle{5});
  assert(mock_ptr->draw_materials[2] == stellar::graphics::MaterialHandle{3});
  assert((mock_ptr->skin_joint_counts == std::vector<std::size_t>{0, 0, 0}));
  assert(mock_ptr->ended_frame);

  return 0;
}
