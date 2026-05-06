#include "stellar/graphics/RenderLevel.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

class ScopedEnvValue {
public:
  ScopedEnvValue(const char *name, const char *value) : name_(name) {
    if (const char *previous = std::getenv(name_.c_str())) {
      previous_ = previous;
    }
    if (value != nullptr) {
      setenv(name_.c_str(), value, 1);
    } else {
      unsetenv(name_.c_str());
    }
  }

  ~ScopedEnvValue() {
    if (previous_.has_value()) {
      setenv(name_.c_str(), previous_->c_str(), 1);
    } else {
      unsetenv(name_.c_str());
    }
  }

  ScopedEnvValue(const ScopedEnvValue &) = delete;
  ScopedEnvValue &operator=(const ScopedEnvValue &) = delete;

private:
  std::string name_;
  std::optional<std::string> previous_;
};

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
    draw_transforms.push_back(transforms);
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
  std::vector<stellar::graphics::MeshDrawTransforms> draw_transforms;
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
  level.geometry.images.push_back(stellar::assets::ImageAsset{
      .width = 2,
      .height = 2,
      .format = stellar::assets::ImageFormat::kR8G8B8A8,
      .pixels = {255, 128, 128, 255, 128, 255, 128, 255,
                 128, 128, 255, 255, 255, 255, 255, 255},
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
  level.geometry.lightmaps.push_back(stellar::assets::LevelLightmap{
      .image_index = 1,
      .size = {2, 2},
      .style = 0,
      .source_name = "face_0",
  });
  level.geometry.materials.push_back(stellar::assets::LevelSurfaceMaterial{
      .name = "stone",
      .texture_index = 0,
      .lightmap_index = 0,
      .source_name = "STONE1",
  });
  level.geometry.materials.push_back(stellar::assets::LevelSurfaceMaterial{
      .name = "missing_texture",
      .texture_index = 99,
      .lightmap_index = 99,
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

stellar::assets::LevelAsset make_sidecar_material_level() {
  stellar::assets::LevelAsset level;
  level.source_uri = "synthetic:sidecar_material.bsp";
  level.geometry.images = {
      stellar::assets::ImageAsset{.name = "base",
                                  .width = 1,
                                  .height = 1,
                                  .format = stellar::assets::ImageFormat::kR8G8B8A8,
                                  .pixels = {255, 255, 255, 255}},
      stellar::assets::ImageAsset{.name = "normal",
                                  .width = 1,
                                  .height = 1,
                                  .format = stellar::assets::ImageFormat::kR8G8B8A8,
                                  .pixels = {128, 128, 255, 255}},
      stellar::assets::ImageAsset{.name = "specular",
                                  .width = 1,
                                  .height = 1,
                                  .format = stellar::assets::ImageFormat::kR8G8B8A8,
                                  .pixels = {255, 255, 255, 255}},
      stellar::assets::ImageAsset{.name = "metallic_roughness",
                                  .width = 1,
                                  .height = 1,
                                  .format = stellar::assets::ImageFormat::kR8G8B8A8,
                                  .pixels = {0, 192, 64, 255}},
      stellar::assets::ImageAsset{.name = "occlusion",
                                  .width = 1,
                                  .height = 1,
                                  .format = stellar::assets::ImageFormat::kR8G8B8A8,
                                  .pixels = {192, 192, 192, 255}},
      stellar::assets::ImageAsset{.name = "emissive",
                                  .width = 1,
                                  .height = 1,
                                  .format = stellar::assets::ImageFormat::kR8G8B8A8,
                                  .pixels = {32, 64, 128, 255}},
      stellar::assets::ImageAsset{.name = "lightmap",
                                  .width = 1,
                                  .height = 1,
                                  .format = stellar::assets::ImageFormat::kR8G8B8,
                                  .pixels = {128, 128, 128}},
  };
  level.geometry.samplers.push_back(stellar::assets::SamplerAsset{
      .name = "linear_repeat",
      .mag_filter = stellar::assets::TextureFilter::kLinear,
      .min_filter = stellar::assets::TextureFilter::kLinearMipmapLinear,
      .wrap_s = stellar::assets::TextureWrapMode::kRepeat,
      .wrap_t = stellar::assets::TextureWrapMode::kRepeat,
  });
  level.geometry.textures = {
      stellar::assets::TextureAsset{
          .name = "base",
          .image_index = 0,
          .sampler_index = 0,
          .color_space = stellar::assets::TextureColorSpace::kSrgb},
      stellar::assets::TextureAsset{
          .name = "normal",
          .image_index = 1,
          .sampler_index = 0,
          .color_space = stellar::assets::TextureColorSpace::kLinear},
      stellar::assets::TextureAsset{
          .name = "specular",
          .image_index = 2,
          .sampler_index = 0,
          .color_space = stellar::assets::TextureColorSpace::kLinear},
      stellar::assets::TextureAsset{
          .name = "metallic_roughness",
          .image_index = 3,
          .sampler_index = 0,
          .color_space = stellar::assets::TextureColorSpace::kLinear},
      stellar::assets::TextureAsset{
          .name = "occlusion",
          .image_index = 4,
          .sampler_index = 0,
          .color_space = stellar::assets::TextureColorSpace::kLinear},
      stellar::assets::TextureAsset{
          .name = "emissive",
          .image_index = 5,
          .sampler_index = 0,
          .color_space = stellar::assets::TextureColorSpace::kSrgb},
  };
  level.geometry.lightmaps.push_back(stellar::assets::LevelLightmap{
      .image_index = 6,
      .size = {1, 1},
      .style = 0,
      .source_name = "face_0",
  });
  stellar::assets::MaterialAsset resolved;
  resolved.name = "dev/wall_96";
  resolved.base_color_texture =
      stellar::assets::MaterialTextureSlot{.texture_index = 0};
  resolved.normal_texture =
      stellar::assets::MaterialTextureSlot{.texture_index = 1};
  resolved.specular_texture =
      stellar::assets::MaterialTextureSlot{.texture_index = 2};
  resolved.specular_texture->transform.enabled = true;
  resolved.specular_texture->transform.offset = {0.25F, 0.5F};
  resolved.specular_texture->transform.scale = {2.0F, 3.0F};
  resolved.specular_texture->transform.rotation = 0.125F;
  resolved.metallic_roughness_texture =
      stellar::assets::MaterialTextureSlot{.texture_index = 3, .texcoord_set = 1};
  resolved.occlusion_texture =
      stellar::assets::MaterialTextureSlot{.texture_index = 4};
  resolved.emissive_texture =
      stellar::assets::MaterialTextureSlot{.texture_index = 5};
  resolved.emissive_factor = {0.1F, 0.2F, 0.3F};
  resolved.normal_scale = 1.5F;
  resolved.normal_light_strength = 0.25F;
  resolved.specular_factor = 0.35F;
  resolved.specular_power = 48.0F;
  resolved.occlusion_strength = 0.65F;
  resolved.metallic_factor = 0.25F;
  resolved.roughness_factor = 0.75F;
  resolved.alpha_mode = stellar::assets::AlphaMode::kMask;
  resolved.alpha_cutoff = 0.35F;
  resolved.double_sided = true;
  resolved.unlit = false;
  level.geometry.materials.push_back(stellar::assets::LevelSurfaceMaterial{
      .name = "dev/wall_96",
      .texture_index = 0,
      .lightmap_index = 0,
      .source_name = "dev/wall_96",
      .resolved_material = resolved,
  });
  auto primitive = make_primitive(0);
  primitive.has_tangents = true;
  for (auto &vertex : primitive.vertices) {
    vertex.tangent = {1.0F, 0.0F, 0.0F, 1.0F};
  }
  level.geometry.meshes.push_back(stellar::assets::MeshAsset{
      .name = "bsp_static_world",
      .primitives = {primitive},
  });
  level.geometry.surfaces.push_back(stellar::assets::LevelSurface{
      .name = "surface",
      .mesh_index = 0,
      .primitive_index = 0,
      .material_index = 0,
      .bounds_min = primitive.bounds_min,
      .bounds_max = primitive.bounds_max,
  });
  return level;
}

} // namespace

int main() {
  ScopedEnvValue disable_lightmaps("STELLAR_DISABLE_LIGHTMAPS", nullptr);
  ScopedEnvValue force_fullbright("STELLAR_FORCE_FULLBRIGHT", nullptr);
  ScopedEnvValue force_visualization("STELLAR_FORCE_LIGHTMAP_VISUALIZATION",
                                     nullptr);
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
  assert(mock_ptr->uploaded_textures.size() == 2);
  assert(mock_ptr->uploaded_textures[0].image.width == 4);
  assert(mock_ptr->uploaded_textures[0].color_space ==
         stellar::assets::TextureColorSpace::kSrgb);
  assert(mock_ptr->uploaded_textures[1].image.width == 2);
  assert(mock_ptr->uploaded_textures[1].color_space ==
         stellar::assets::TextureColorSpace::kLinear);
  assert(mock_ptr->material_uploads.size() == 3);
  assert(mock_ptr->material_uploads[0].material.name ==
         "stellar_default_level_material");
  assert(mock_ptr->material_uploads[0].material.base_color_factor ==
         (std::array<float, 4>{0.75F, 0.75F, 0.75F, 1.0F}));
  assert(mock_ptr->material_uploads[0].material.double_sided);
  assert(mock_ptr->material_uploads[1].material.name == "stone");
  assert(mock_ptr->material_uploads[1].material.base_color_factor ==
         (std::array<float, 4>{1.0F, 1.0F, 1.0F, 1.0F}));
  assert(mock_ptr->material_uploads[1].material.double_sided);
  assert(mock_ptr->material_uploads[1].base_color_texture.has_value());
  assert(mock_ptr->material_uploads[1].base_color_texture->sampler.wrap_s ==
         stellar::assets::TextureWrapMode::kClampToEdge);
  assert(mock_ptr->material_uploads[1].lightmap_texture.has_value());
  assert(mock_ptr->material_uploads[1].lightmap_texture->texcoord_set == 1);
  assert(mock_ptr->material_uploads[1].lightmap_texture->sampler.min_filter ==
         stellar::assets::TextureFilter::kLinear);
  assert(mock_ptr->material_uploads[1].material.metallic_factor == 0.0F);
  assert(mock_ptr->material_uploads[1].material.unlit);
  assert(mock_ptr->material_uploads[2].material.name == "missing_texture");
  assert(mock_ptr->material_uploads[2].material.base_color_factor ==
         (std::array<float, 4>{0.75F, 0.75F, 0.75F, 1.0F}));
  assert(mock_ptr->material_uploads[2].material.double_sided);
  assert(!mock_ptr->material_uploads[2].base_color_texture.has_value());
  assert(!mock_ptr->material_uploads[2].lightmap_texture.has_value());

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
  assert(mock_ptr->draw_materials[0] == stellar::graphics::MaterialHandle{5});
  assert(mock_ptr->draw_materials[1] == stellar::graphics::MaterialHandle{6});
  assert(mock_ptr->draw_materials[2] == stellar::graphics::MaterialHandle{4});
  assert(mock_ptr->ended_frame);

  auto sidecar_mock = std::make_unique<MockGraphicsDevice>();
  MockGraphicsDevice *sidecar_mock_ptr = sidecar_mock.get();
  stellar::graphics::RenderLevel sidecar_render_level;
  auto sidecar_result = sidecar_render_level.initialize(
      std::move(sidecar_mock), window, make_sidecar_material_level());
  assert(sidecar_result.has_value());
  assert(sidecar_mock_ptr->uploaded_textures.size() == 7);
  assert(sidecar_mock_ptr->material_uploads.size() == 2);
  const auto &sidecar_upload = sidecar_mock_ptr->material_uploads[1];
  assert(sidecar_upload.base_color_texture.has_value());
  assert(sidecar_upload.normal_texture.has_value());
  assert(sidecar_upload.specular_texture.has_value());
  assert(sidecar_upload.metallic_roughness_texture.has_value());
  assert(sidecar_upload.occlusion_texture.has_value());
  assert(sidecar_upload.emissive_texture.has_value());
  assert(sidecar_upload.lightmap_texture.has_value());
  assert(sidecar_upload.metallic_roughness_texture->texcoord_set == 1);
  assert(sidecar_upload.specular_texture->transform.enabled);
  assert(sidecar_upload.specular_texture->transform.offset ==
         (std::array<float, 2>{0.25F, 0.5F}));
  assert(sidecar_upload.specular_texture->transform.scale ==
         (std::array<float, 2>{2.0F, 3.0F}));
  assert(sidecar_upload.specular_texture->transform.rotation == 0.125F);
  assert(sidecar_upload.material.normal_scale == 1.5F);
  assert(sidecar_upload.material.normal_light_strength == 0.25F);
  assert(sidecar_upload.material.specular_factor == 0.35F);
  assert(sidecar_upload.material.specular_power == 48.0F);
  assert(sidecar_upload.material.occlusion_strength == 0.65F);
  assert(sidecar_upload.material.metallic_factor == 0.25F);
  assert(sidecar_upload.material.roughness_factor == 0.75F);
  assert(sidecar_upload.material.emissive_factor ==
         (std::array<float, 3>{0.1F, 0.2F, 0.3F}));
  assert(sidecar_upload.material.alpha_mode == stellar::assets::AlphaMode::kMask);
  assert(sidecar_upload.material.alpha_cutoff == 0.35F);
  sidecar_render_level.render(64, 64, vp, identity,
                              std::array<float, 3>{1.0F, 2.0F, 3.0F});
  assert(!sidecar_mock_ptr->draw_transforms.empty());
  assert(sidecar_mock_ptr->draw_transforms[0].has_camera_world_position);
  assert(sidecar_mock_ptr->draw_transforms[0].camera_world_position ==
         (std::array<float, 3>{1.0F, 2.0F, 3.0F}));

  return 0;
}
