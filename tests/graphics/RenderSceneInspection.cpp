#include "stellar/graphics/RenderLevel.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "RecordingGraphicsDevice.hpp"
#include "stellar/graphics/DebugCubeMesh.hpp"
#include "stellar/graphics/LevelRenderer.hpp"

namespace {

constexpr std::array<float, 16> kIdentity{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                          0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F,
                                          0.0F, 0.0F, 0.0F, 1.0F};

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
  assert(device->draw_calls.size() == 1);
  assert(device->primitive_draw(0).material == device->material_handles[0]);
}

void verify_visibility_culls_static_surfaces(stellar::platform::Window &window) {
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
  view.camera_up = {0.0F, 2.0F, 0.0F};

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
  assert(quads[0].positions[2][1] == 3.0F);
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
  view.camera_up = {0.0F, 1.0F, 0.0F};

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
  assert(camera.far_plane > camera.near_plane);

  const auto empty_bounds =
      stellar::graphics::compute_level_bounds(stellar::assets::LevelAsset{});
  assert(empty_bounds.center[0] == 0.0F);
  assert(empty_bounds.radius == 1.0F);
}

void verify_level_render_state_uses_override_camera_for_culling() {
  stellar::graphics::LevelRenderView view;
  view.eye = {10.0F, 20.0F, 30.0F};
  view.target = {10.0F, 20.0F, 29.0F};
  view.near_plane = 0.25F;
  view.far_plane = 512.0F;

  const auto state = stellar::graphics::compute_level_render_state(
      view, stellar::graphics::GraphicsBackend::kOpenGL, 16.0F / 9.0F);

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
      view, stellar::graphics::GraphicsBackend::kOpenGL, 0.0F);

  assert(!state.camera_world_position.has_value());
  assert(state.view_projection[0] != 0.0F);
}

void verify_debug_cube_winding_matches_normals() {
  const auto mesh = stellar::graphics::create_debug_cube_mesh();
  assert(mesh.has_value());
  assert(mesh->primitives.size() == 6);

  for (const auto &primitive : mesh->primitives) {
    assert(primitive.vertices.size() == 4);
    assert(primitive.indices == std::vector<std::uint32_t>({0, 1, 2, 0, 2, 3}));
  }
}

} // namespace

int main() {
  stellar::platform::Window window;
  verify_surface_material_indices(window);
  verify_missing_material_uses_default(window);
  verify_visibility_culls_static_surfaces(window);
  verify_visibility_falls_back_when_camera_leaf_missing(window);
  verify_billboard_quad_generation_sorting_and_fields();
  verify_billboards_submit_after_static_geometry(window);
  verify_level_bounds_and_camera_fit();
  verify_level_render_state_uses_override_camera_for_culling();
  verify_level_render_state_can_disable_culling_for_fallback();
  verify_debug_cube_winding_matches_normals();
  return 0;
}
