#include "stellar/graphics/RenderLevel.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "RecordingGraphicsDevice.hpp"
#include "stellar/core/WorldAxes.hpp"
#include "stellar/graphics/LevelRenderer.hpp"

namespace {

constexpr std::array<float, 16> kIdentity{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                           0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F,
                                           0.0F, 0.0F, 0.0F, 1.0F};

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

stellar::assets::MeshPrimitive
make_primitive(std::optional<std::size_t> material_index,
               float center_z = 0.0F) {
  stellar::assets::MeshPrimitive primitive;
  primitive.vertices.resize(3);
  primitive.indices = {0, 1, 2};
  primitive.material_index = material_index;
  primitive.bounds_min = {-0.5F, -0.5F, center_z};
  primitive.bounds_max = {0.5F, 0.5F, center_z};
  return primitive;
}

stellar::assets::LevelAsset make_level_with_surfaces(
    std::vector<stellar::assets::MeshPrimitive> primitives,
    std::vector<std::optional<std::size_t>> surface_materials) {
  stellar::assets::LevelAsset level;
  level.geometry.materials = {
      stellar::assets::LevelSurfaceMaterial{.name = "opaque_a",
                                            .source_name = "OPAQUE_A"},
      stellar::assets::LevelSurfaceMaterial{.name = "opaque_b",
                                            .source_name = "OPAQUE_B"},
  };
  level.geometry.meshes.push_back(stellar::assets::MeshAsset{
      .name = "worldspawn", .primitives = std::move(primitives)});
  for (std::size_t primitive_index = 0;
       primitive_index < surface_materials.size(); ++primitive_index) {
    const auto &primitive =
        level.geometry.meshes[0].primitives[primitive_index];
    level.geometry.surfaces.push_back(stellar::assets::LevelSurface{
        .name = "face",
        .mesh_index = 0,
        .primitive_index = primitive_index,
        .material_index = surface_materials[primitive_index],
        .bounds_min = primitive.bounds_min,
        .bounds_max = primitive.bounds_max,
    });
  }
  return level;
}

void add_two_leaf_visibility(stellar::assets::LevelAsset &level) {
  level.visibility.available = true;
  level.visibility.cluster_count = 2;
  level.visibility.compressed_pvs = {std::byte{0x01}, std::byte{0x03}};
  level.visibility.leaves = {
      stellar::assets::LevelLeaf{
          .bounds_min = {-10.0F, -10.0F, -10.0F},
          .bounds_max = {0.0F, 10.0F, 10.0F},
          .surface_indices = {0},
          .compressed_pvs_offset = 0,
      },
      stellar::assets::LevelLeaf{
          .bounds_min = {0.0F, -10.0F, -10.0F},
          .bounds_max = {10.0F, 10.0F, 10.0F},
          .surface_indices = {1},
          .compressed_pvs_offset = 1,
      },
  };
}

void add_all_zero_visibility(stellar::assets::LevelAsset &level) {
  level.visibility.available = true;
  level.visibility.cluster_count = 2;
  level.visibility.compressed_pvs = {std::byte{0x00}, std::byte{0x01}};
  level.visibility.leaves = {
      stellar::assets::LevelLeaf{
          .bounds_min = {-10.0F, -10.0F, -10.0F},
          .bounds_max = {0.0F, 10.0F, 10.0F},
          .surface_indices = {0},
          .compressed_pvs_offset = 0,
      },
      stellar::assets::LevelLeaf{
          .bounds_min = {0.0F, -10.0F, -10.0F},
          .bounds_max = {10.0F, 10.0F, 10.0F},
          .surface_indices = {1},
          .compressed_pvs_offset = 0,
      },
  };
}

std::pair<stellar::graphics::RenderLevel *,
          stellar::graphics::testing::RecordingGraphicsDevice *>
initialize_level(stellar::graphics::RenderLevel &render_level,
                 stellar::platform::Window &window,
                 stellar::assets::LevelAsset level) {
  auto device =
      std::make_unique<stellar::graphics::testing::RecordingGraphicsDevice>();
  auto *device_ptr = device.get();
  auto result =
      render_level.initialize(std::move(device), window, std::move(level));
  assert(result.has_value());
  assert(device_ptr->initialized);
  return {&render_level, device_ptr};
}

void verify_surface_material_indices(stellar::platform::Window &window) {
  auto level = make_level_with_surfaces(
      {make_primitive(0), make_primitive(1), make_primitive(std::nullopt)},
      {1, std::nullopt, 0});

  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));
  render_level.render(320, 200, kIdentity, kIdentity);

  assert(device->material_handles.size() == 3);
  assert(device->draw_calls.size() == 3);
  assert(device->primitive_draw(0).primitive_index == 0);
  assert(device->primitive_draw(0).material == device->material_handles[2]);
  assert(device->primitive_draw(1).primitive_index == 1);
  assert(device->primitive_draw(1).material == device->material_handles[0]);
  assert(device->primitive_draw(2).primitive_index == 2);
  assert(device->primitive_draw(2).material == device->material_handles[1]);
}

void verify_missing_material_uses_default(stellar::platform::Window &window) {
  auto level = make_level_with_surfaces({make_primitive(std::nullopt)}, {99});

  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));
  render_level.render(64, 64, kIdentity, kIdentity);

  assert(device->uploaded_materials[0].material.name ==
         "stellar_default_level_material");
  assert(device->uploaded_materials[0].material.double_sided);
  assert(device->draw_calls.size() == 1);
  assert(device->primitive_draw(0).material == device->material_handles[0]);
}

void verify_level_surface_materials_are_double_sided(
    stellar::platform::Window &window) {
  auto level = make_level_with_surfaces({make_primitive(0)}, {0});

  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));

  assert(!device->uploaded_materials.empty());
  for (const auto &upload : device->uploaded_materials) {
    assert(upload.material.double_sided);
  }
}

void verify_visibility_culls_static_surfaces(stellar::platform::Window &window) {
  ScopedEnvValue disable_culling("STELLAR_DISABLE_VISIBILITY_CULLING", nullptr);
  auto level = make_level_with_surfaces({make_primitive(0), make_primitive(1)},
                                        {0, 1});
  add_two_leaf_visibility(level);

  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));
  render_level.render(64, 64, kIdentity, kIdentity,
                      std::array<float, 3>{-5.0F, 0.0F, 0.0F});

  assert(device->draw_calls.size() == 1);
  assert(device->primitive_draw(0).primitive_index == 0);
}

void verify_visibility_culling_env_disable_queues_all_surfaces(
    stellar::platform::Window &window) {
  ScopedEnvValue disable_culling("STELLAR_DISABLE_VISIBILITY_CULLING", "1");
  auto level = make_level_with_surfaces({make_primitive(0), make_primitive(1)},
                                        {0, 1});
  add_two_leaf_visibility(level);

  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));
  render_level.render(64, 64, kIdentity, kIdentity,
                      std::array<float, 3>{-5.0F, 0.0F, 0.0F});

  assert(device->draw_calls.size() == 2);
  assert(device->primitive_draw(0).primitive_index == 0);
  assert(device->primitive_draw(1).primitive_index == 1);
}

void verify_visibility_all_zero_falls_back_to_all_surfaces(
    stellar::platform::Window &window) {
  ScopedEnvValue disable_culling("STELLAR_DISABLE_VISIBILITY_CULLING", nullptr);
  auto level = make_level_with_surfaces({make_primitive(0), make_primitive(1)},
                                        {0, 1});
  add_all_zero_visibility(level);

  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));
  render_level.render(64, 64, kIdentity, kIdentity,
                      std::array<float, 3>{-5.0F, 0.0F, 0.0F});

  assert(device->draw_calls.size() == 2);
  assert(device->primitive_draw(0).primitive_index == 0);
  assert(device->primitive_draw(1).primitive_index == 1);
}

void verify_visibility_falls_back_when_camera_leaf_missing(
    stellar::platform::Window &window) {
  auto level = make_level_with_surfaces({make_primitive(0), make_primitive(1)},
                                        {0, 1});
  add_two_leaf_visibility(level);

  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));
  render_level.render(64, 64, kIdentity, kIdentity,
                      std::array<float, 3>{50.0F, 0.0F, 0.0F});

  assert(device->draw_calls.size() == 2);
  assert(device->primitive_draw(0).primitive_index == 0);
  assert(device->primitive_draw(1).primitive_index == 1);
}

void verify_billboard_quad_generation_sorting_and_fields() {
  stellar::graphics::BillboardView view;
  view.view = kIdentity;
  view.view_projection = kIdentity;
  view.camera_right = {1.0F, 0.0F, 0.0F};
  view.camera_up = {0.0F, 0.0F, 2.0F};

  stellar::graphics::BillboardSprite masked;
  masked.position = {0.0F, 0.0F, -2.0F};
  masked.size = {2.0F, 3.0F};
  masked.color = {0.25F, 0.5F, 0.75F, 0.9F};
  masked.texture = stellar::graphics::TextureHandle{44};
  masked.alpha_mask = true;
  masked.alpha_blend = true;
  masked.alpha_cutoff = 0.375F;

  stellar::graphics::BillboardSprite near_blend;
  near_blend.position = {0.0F, 0.0F, -1.0F};
  near_blend.texture = stellar::graphics::TextureHandle{55};

  stellar::graphics::BillboardSprite far_blend = near_blend;
  far_blend.position = {0.0F, 0.0F, -5.0F};
  far_blend.texture = stellar::graphics::TextureHandle{66};

  const std::array sprites{masked, near_blend, far_blend};
  const auto quads = stellar::graphics::build_billboard_quads(sprites, view);
  assert(quads.size() == 3);
  assert(quads[0].alpha_mask);
  assert(!quads[0].alpha_blend);
  assert(quads[0].texture == stellar::graphics::TextureHandle{44});
  assert(quads[0].color[2] == 0.75F);
  assert(quads[0].size[1] == 3.0F);
  assert(quads[0].alpha_cutoff == 0.375F);
  assert(quads[0].positions[0][0] == -1.0F);
  assert(quads[0].positions[2][2] == 1.0F);
  assert(quads[1].texture == stellar::graphics::TextureHandle{66});
  assert(quads[2].texture == stellar::graphics::TextureHandle{55});
}

void verify_billboards_submit_after_static_geometry(
    stellar::platform::Window &window) {
  auto level = make_level_with_surfaces({make_primitive(0), make_primitive(1)},
                                        {0, 1});
  add_two_leaf_visibility(level);
  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));

  stellar::graphics::BillboardView view;
  view.view = kIdentity;
  view.view_projection = kIdentity;
  view.camera_right = {1.0F, 0.0F, 0.0F};
  view.camera_up = stellar::core::kWorldUp;

  stellar::graphics::BillboardSprite near_blend;
  near_blend.position = {0.0F, 0.0F, -1.0F};
  near_blend.texture = stellar::graphics::TextureHandle{111};

  stellar::graphics::BillboardSprite far_blend = near_blend;
  far_blend.position = {0.0F, 0.0F, -5.0F};
  far_blend.texture = stellar::graphics::TextureHandle{222};

  const std::array sprites{near_blend, far_blend};
  render_level.render(64, 64, kIdentity, kIdentity,
                      std::array<float, 3>{-5.0F, 0.0F, 0.0F}, view, sprites);

  assert(device->draw_calls.size() == 3);
  assert(device->draw_calls[0].mesh == device->mesh_handles[0]);
  assert(device->draw_calls[1].mesh != device->mesh_handles[0]);
  assert(device->draw_calls[2].mesh != device->mesh_handles[0]);
  assert(device->uploaded_materials[3].base_color_texture->texture ==
         stellar::graphics::TextureHandle{222});
  assert(device->uploaded_materials[4].base_color_texture->texture ==
         stellar::graphics::TextureHandle{111});
  assert(device->destroyed_meshes.size() == 2);
  assert(device->destroyed_materials.size() == 2);
}

void verify_static_less_level_renders_no_static_surfaces(
    stellar::platform::Window &window) {
  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window,
                                      stellar::assets::LevelAsset{});
  render_level.render(64, 64, kIdentity, kIdentity);

  assert(device->mesh_handles.empty());
  assert(device->material_handles.size() == 1);
  assert(device->uploaded_materials[0].material.name ==
         "stellar_default_level_material");
  assert(device->draw_calls.empty());
}

void verify_level_lightmap_material_upload(stellar::platform::Window &window) {
  ScopedEnvValue disable_lightmaps("STELLAR_DISABLE_LIGHTMAPS", nullptr);
  ScopedEnvValue force_fullbright("STELLAR_FORCE_FULLBRIGHT", nullptr);
  ScopedEnvValue force_visualization("STELLAR_FORCE_LIGHTMAP_VISUALIZATION",
                                     nullptr);
  auto level = make_level_with_surfaces({make_primitive(0)}, {0});
  level.geometry.images = {
      stellar::assets::ImageAsset{.name = "base",
                                  .width = 1,
                                  .height = 1,
                                  .format = stellar::assets::ImageFormat::kR8G8B8A8,
                                  .pixels = {255, 255, 255, 255}},
      stellar::assets::ImageAsset{.name = "lightmap",
                                  .width = 1,
                                  .height = 1,
                                  .format = stellar::assets::ImageFormat::kR8G8B8,
                                  .pixels = {128, 128, 128}},
  };
  level.geometry.samplers.push_back(stellar::assets::SamplerAsset{});
  level.geometry.textures.push_back(stellar::assets::TextureAsset{
      .name = "base", .image_index = 0, .sampler_index = 0,
      .color_space = stellar::assets::TextureColorSpace::kSrgb});
  level.geometry.lightmaps.push_back(stellar::assets::LevelLightmap{
      .image_index = 1, .size = {1, 1}, .style = 2, .source_name = "face_0"});
  level.geometry.materials[0].texture_index = 0;
  level.geometry.materials[0].lightmap_index = 0;

  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));

  assert(device->uploaded_textures.size() == 2);
  assert(device->uploaded_textures[0].color_space ==
         stellar::assets::TextureColorSpace::kSrgb);
  assert(device->uploaded_textures[1].color_space ==
         stellar::assets::TextureColorSpace::kLinear);
  const auto &upload = device->uploaded_materials[1];
  assert(upload.base_color_texture.has_value());
  assert(upload.lightmap_texture.has_value());
  assert(upload.base_color_texture->texcoord_set == 0);
  assert(upload.lightmap_texture->texcoord_set == 1);
  assert(upload.lightmap_texture->sampler.min_filter ==
         stellar::assets::TextureFilter::kLinear);
  assert(upload.lightmap_texture->sampler.wrap_s ==
         stellar::assets::TextureWrapMode::kClampToEdge);
  assert(upload.lightmap_multiplier == 1.0F);
}

void verify_baked_required_missing_lightmap_uses_black_fallback(
    stellar::platform::Window &window) {
  ScopedEnvValue disable_lightmaps("STELLAR_DISABLE_LIGHTMAPS", nullptr);
  ScopedEnvValue force_fullbright("STELLAR_FORCE_FULLBRIGHT", nullptr);
  auto level = make_level_with_surfaces({make_primitive(0)}, {0});
  level.lighting.mode = stellar::assets::LevelLightingMode::kBakedRequired;
  level.lighting.lightmap_intensity = 0.5F;

  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));

  assert(device->uploaded_textures.size() == 1);
  assert(device->uploaded_textures[0].image.name ==
         "baked_missing_black_lightmap");
  assert(device->uploaded_textures[0].color_space ==
         stellar::assets::TextureColorSpace::kLinear);
  const auto &upload = device->uploaded_materials[1];
  assert(!upload.material.unlit);
  assert(upload.lightmap_texture.has_value());
  assert(upload.lightmap_texture->texture == device->texture_handles[0]);
  assert(upload.lightmap_texture->texcoord_set == 1);
  assert(upload.lightmap_texture->sampler.min_filter ==
         stellar::assets::TextureFilter::kLinear);
  assert(upload.lightmap_texture->sampler.wrap_s ==
         stellar::assets::TextureWrapMode::kClampToEdge);
  assert(upload.lightmap_multiplier == 0.5F);
}

void verify_fullbright_missing_lightmap_skips_black_fallback(
    stellar::platform::Window &window) {
  ScopedEnvValue disable_lightmaps("STELLAR_DISABLE_LIGHTMAPS", nullptr);
  ScopedEnvValue force_fullbright("STELLAR_FORCE_FULLBRIGHT", nullptr);
  auto level = make_level_with_surfaces({make_primitive(0)}, {0});
  level.lighting.mode = stellar::assets::LevelLightingMode::kFullbright;

  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));

  assert(device->uploaded_textures.empty());
  const auto &upload = device->uploaded_materials[1];
  assert(upload.material.unlit);
  assert(!upload.lightmap_texture.has_value());
}

void verify_baked_required_lightmap_min_and_global_light_upload(
    stellar::platform::Window &window) {
  ScopedEnvValue disable_lightmaps("STELLAR_DISABLE_LIGHTMAPS", nullptr);
  ScopedEnvValue force_fullbright("STELLAR_FORCE_FULLBRIGHT", nullptr);
  auto level = make_level_with_surfaces({make_primitive(0)}, {0});
  level.lighting.mode = stellar::assets::LevelLightingMode::kBakedRequired;
  level.lighting.global_ambient.enabled = true;
  level.lighting.global_ambient.color = {0.25F, 0.5F, 0.75F};
  level.lighting.global_ambient.intensity = 0.2F;

  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));

  const auto &upload = device->uploaded_materials[1];
  assert(upload.lightmap_min == 0.0F);
  assert(upload.global_light_color[0] == 0.25F);
  assert(upload.global_light_color[1] == 0.5F);
  assert(upload.global_light_color[2] == 0.75F);
  assert(upload.global_light_intensity == 0.2F);
}

void verify_lightmap_disable_env_skips_material_binding(
    stellar::platform::Window &window, const char *flag_name) {
  ScopedEnvValue disable_lightmaps(flag_name, "1");
  ScopedEnvValue force_visualization("STELLAR_FORCE_LIGHTMAP_VISUALIZATION",
                                     nullptr);
  auto level = make_level_with_surfaces({make_primitive(0)}, {0});
  level.geometry.images = {
      stellar::assets::ImageAsset{.name = "base",
                                  .width = 1,
                                  .height = 1,
                                  .format = stellar::assets::ImageFormat::kR8G8B8A8,
                                  .pixels = {255, 255, 255, 255}},
      stellar::assets::ImageAsset{.name = "lightmap",
                                  .width = 1,
                                  .height = 1,
                                   .format = stellar::assets::ImageFormat::kR8G8B8,
                                   .pixels = {0, 0, 0}},
  };
  level.geometry.textures.push_back(stellar::assets::TextureAsset{
      .name = "base", .image_index = 0, .sampler_index = std::nullopt,
      .color_space = stellar::assets::TextureColorSpace::kSrgb});
  level.geometry.lightmaps.push_back(stellar::assets::LevelLightmap{
      .image_index = 1, .size = {1, 1}, .style = 0, .source_name = "face_0"});
  level.geometry.materials[0].texture_index = 0;
  level.geometry.materials[0].lightmap_index = 0;

  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));

  assert(device->uploaded_textures.size() == 1);
  assert(device->uploaded_textures[0].color_space ==
         stellar::assets::TextureColorSpace::kSrgb);
  assert(device->uploaded_materials[1].material.unlit);
  assert(device->uploaded_materials[1].base_color_texture.has_value());
  assert(!device->uploaded_materials[1].lightmap_texture.has_value());
}

void verify_force_lightmap_visualization_uses_lightmap_as_base(
    stellar::platform::Window &window) {
  ScopedEnvValue disable_lightmaps("STELLAR_DISABLE_LIGHTMAPS", nullptr);
  ScopedEnvValue force_fullbright("STELLAR_FORCE_FULLBRIGHT", nullptr);
  ScopedEnvValue force_visualization("STELLAR_FORCE_LIGHTMAP_VISUALIZATION",
                                     "1");
  auto level = make_level_with_surfaces({make_primitive(0)}, {0});
  level.geometry.images = {
      stellar::assets::ImageAsset{.name = "base",
                                  .width = 1,
                                  .height = 1,
                                  .format = stellar::assets::ImageFormat::kR8G8B8A8,
                                  .pixels = {255, 255, 255, 255}},
      stellar::assets::ImageAsset{.name = "lightmap",
                                  .width = 1,
                                  .height = 1,
                                  .format = stellar::assets::ImageFormat::kR8G8B8,
                                  .pixels = {64, 128, 192}},
  };
  level.geometry.textures.push_back(stellar::assets::TextureAsset{
      .name = "base", .image_index = 0, .sampler_index = std::nullopt,
      .color_space = stellar::assets::TextureColorSpace::kSrgb});
  level.geometry.lightmaps.push_back(stellar::assets::LevelLightmap{
      .image_index = 1, .size = {1, 1}, .style = 0, .source_name = "face_0"});
  level.geometry.materials[0].texture_index = 0;
  level.geometry.materials[0].lightmap_index = 0;

  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window, std::move(level));

  assert(device->uploaded_textures.size() == 2);
  const auto &upload = device->uploaded_materials[1];
  assert(upload.base_color_texture.has_value());
  assert(upload.base_color_texture->texture == device->texture_handles[1]);
  assert(upload.base_color_texture->texcoord_set == 1);
  assert(upload.base_color_texture->sampler.wrap_s ==
         stellar::assets::TextureWrapMode::kClampToEdge);
  assert(!upload.lightmap_texture.has_value());
  assert(upload.lightmap_multiplier == 1.0F);
  assert(upload.material.base_color_factor[0] == 1.0F);
}

void verify_billboards_submit_without_static_geometry(
    stellar::platform::Window &window) {
  stellar::graphics::RenderLevel render_level;
  auto [_, device] = initialize_level(render_level, window,
                                      stellar::assets::LevelAsset{});

  stellar::graphics::BillboardView view;
  view.view = kIdentity;
  view.view_projection = kIdentity;
  view.camera_right = {1.0F, 0.0F, 0.0F};
  view.camera_up = stellar::core::kWorldUp;

  stellar::graphics::BillboardSprite sprite;
  sprite.position = {0.0F, 0.0F, -2.0F};
  sprite.texture = stellar::graphics::TextureHandle{333};

  const std::array sprites{sprite};
  render_level.render(64, 64, kIdentity, kIdentity, std::nullopt, view, sprites);

  assert(device->draw_calls.size() == 1);
  assert(device->uploaded_materials.size() == 2);
  assert(device->uploaded_materials[1].base_color_texture->texture ==
         stellar::graphics::TextureHandle{333});
  assert(device->destroyed_meshes.size() == 1);
  assert(device->destroyed_materials.size() == 1);
}

void verify_level_bounds_and_camera_fit() {
  stellar::assets::LevelAsset level;
  stellar::assets::MeshPrimitive primitive;
  primitive.vertices.resize(3);
  primitive.indices = {0, 1, 2};
  primitive.bounds_min = {-1.0F, -2.0F, -3.0F};
  primitive.bounds_max = {1.0F, 2.0F, 3.0F};
  level.geometry.meshes.push_back(
      stellar::assets::MeshAsset{.name = "bounds", .primitives = {primitive}});

  const auto bounds = stellar::graphics::compute_level_bounds(level);
  assert(bounds.min[0] == -1.0F);
  assert(bounds.max[1] == 2.0F);
  assert(bounds.center[2] == 0.0F);
  assert(bounds.radius > 3.0F);

  const auto camera =
      stellar::graphics::fit_camera_to_bounds(bounds, 45.0F, 16.0F / 9.0F);
  assert(camera.target[0] == 0.0F);
  assert(camera.eye[2] > bounds.max[2]);
  assert(camera.eye[1] < bounds.min[1]);
  assert(camera.far_plane > camera.near_plane);

  const auto empty_bounds =
      stellar::graphics::compute_level_bounds(stellar::assets::LevelAsset{});
  assert(empty_bounds.min[0] == -0.5F);
  assert(empty_bounds.max[1] == 0.5F);
  assert(empty_bounds.center[0] == 0.0F);
  assert(empty_bounds.radius == 1.0F);

  const auto empty_camera = stellar::graphics::fit_camera_to_bounds(
      empty_bounds, 45.0F, 16.0F / 9.0F);
  assert(empty_camera.target == empty_bounds.center);
  assert(empty_camera.far_plane > empty_camera.near_plane);
}

void verify_level_render_state_uses_override_camera_for_culling() {
  stellar::graphics::LevelRenderView view;
  view.eye = {10.0F, 20.0F, 30.0F};
  view.target = {10.0F, 20.0F, 29.0F};
  view.near_plane = 0.25F;
  view.far_plane = 512.0F;

  const auto state = stellar::graphics::compute_level_render_state(
      view, stellar::graphics::default_graphics_backend(), 16.0F / 9.0F);

  assert(state.camera_world_position.has_value());
  assert(*state.camera_world_position == view.eye);
  assert(state.view[12] != 0.0F || state.view[13] != 0.0F ||
         state.view[14] != 0.0F);
}

void verify_level_render_state_can_disable_culling_for_fallback() {
  stellar::graphics::LevelRenderView view;
  view.eye = {0.0F, 0.0F, 8.0F};
  view.target = {0.0F, 0.0F, 0.0F};
  view.visibility_culling = false;

  const auto state = stellar::graphics::compute_level_render_state(
      view, stellar::graphics::default_graphics_backend(), 0.0F);

  assert(!state.camera_world_position.has_value());
  assert(state.view_projection[0] != 0.0F);
}

void verify_metal_projection_uses_zero_to_one_depth_range() {
#if defined(STELLAR_ENABLE_METAL_BACKEND)
  stellar::graphics::LevelRenderView view;
  view.eye = {0.0F, 0.0F, 8.0F};
  view.target = {0.0F, 0.0F, 0.0F};
  view.near_plane = 0.25F;
  view.far_plane = 64.0F;

#if defined(STELLAR_ENABLE_VULKAN_BACKEND)
  const auto vulkan_state = stellar::graphics::compute_level_render_state(
      view, stellar::graphics::GraphicsBackend::kVulkan, 1.0F);
  const auto metal_state = stellar::graphics::compute_level_render_state(
      view, stellar::graphics::GraphicsBackend::kMetal, 1.0F);

  assert(vulkan_state.view_projection == metal_state.view_projection);
#else
  const auto state = stellar::graphics::compute_level_render_state(
      view, stellar::graphics::GraphicsBackend::kMetal, 1.0F);
  assert(state.view_projection[0] != 0.0F);
  assert(state.view_projection[5] != 0.0F);
#endif
#endif
}

void verify_billboard_view_is_derived_from_render_state() {
  stellar::graphics::LevelRenderView view;
  view.eye = {0.0F, -8.0F, 4.0F};
  view.target = {0.0F, 0.0F, 0.0F};

  const auto state = stellar::graphics::compute_level_render_state(
      view, stellar::graphics::default_graphics_backend(), 1.0F);
  const auto billboard_view = stellar::graphics::compute_billboard_view(state);

  assert(billboard_view.view == state.view);
  assert(billboard_view.view_projection == state.view_projection);
  assert(billboard_view.camera_right[0] > 0.99F);
  assert(billboard_view.camera_up[2] > 0.85F);
}

void verify_graphics_up_defaults_use_world_axis_contract() {
  const stellar::graphics::LevelRenderView level_view;
  const stellar::graphics::BillboardView billboard_view;

  assert(level_view.up == stellar::core::kWorldUp);
  assert(billboard_view.camera_up == stellar::core::kWorldUp);
}

void verify_level_renderer_retains_and_clears_presentation_state() {
  stellar::graphics::LevelRenderer renderer;
  stellar::graphics::LevelPresentationState state;
  state.sprites.push_back(stellar::graphics::BillboardSprite{});

  renderer.set_presentation_state(std::move(state));
  assert(renderer.presentation_state().sprites.size() == 1);

  renderer.clear_presentation_state();
  assert(renderer.presentation_state().sprites.empty());
}

} // namespace

int main() {
  stellar::platform::Window window;
  verify_surface_material_indices(window);
  verify_missing_material_uses_default(window);
  verify_level_surface_materials_are_double_sided(window);
  verify_visibility_culls_static_surfaces(window);
  verify_visibility_culling_env_disable_queues_all_surfaces(window);
  verify_visibility_all_zero_falls_back_to_all_surfaces(window);
  verify_visibility_falls_back_when_camera_leaf_missing(window);
  verify_billboard_quad_generation_sorting_and_fields();
  verify_billboards_submit_after_static_geometry(window);
  verify_static_less_level_renders_no_static_surfaces(window);
  verify_level_lightmap_material_upload(window);
  verify_baked_required_missing_lightmap_uses_black_fallback(window);
  verify_fullbright_missing_lightmap_skips_black_fallback(window);
  verify_baked_required_lightmap_min_and_global_light_upload(window);
  verify_lightmap_disable_env_skips_material_binding(window,
                                                     "STELLAR_DISABLE_LIGHTMAPS");
  verify_lightmap_disable_env_skips_material_binding(window,
                                                     "STELLAR_FORCE_FULLBRIGHT");
  verify_force_lightmap_visualization_uses_lightmap_as_base(window);
  verify_billboards_submit_without_static_geometry(window);
  verify_level_bounds_and_camera_fit();
  verify_level_render_state_uses_override_camera_for_culling();
  verify_level_render_state_can_disable_culling_for_fallback();
  verify_metal_projection_uses_zero_to_one_depth_range();
  verify_billboard_view_is_derived_from_render_state();
  verify_graphics_up_defaults_use_world_axis_contract();
  verify_level_renderer_retains_and_clears_presentation_state();
  return 0;
}
