#include "stellar/graphics/RenderLevel.hpp"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace stellar::graphics {

struct QueuedLevelDraw {
  MeshHandle mesh;
  MeshPrimitiveDrawCommand command;
  MeshDrawTransforms transforms;
  float depth = 0.0F;
};

namespace {

constexpr std::array<float, 16> kIdentity4{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                           0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F,
                                           0.0F, 0.0F, 0.0F, 1.0F};
constexpr std::array<float, 9> kIdentity3{1.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                          0.0F, 0.0F, 0.0F, 1.0F};

constexpr const char *kDefaultLevelMaterialName =
    "stellar_default_level_material";

glm::mat4 to_glm_mat4(const std::array<float, 16> &data) noexcept {
  return glm::make_mat4(data.data());
}

std::array<float, 16> to_array(const glm::mat4 &matrix) noexcept {
  std::array<float, 16> result{};
  const float *data = &matrix[0][0];
  for (std::size_t index = 0; index < result.size(); ++index) {
    result[index] = data[index];
  }
  return result;
}

std::array<float, 9> to_array(const glm::mat3 &matrix) noexcept {
  std::array<float, 9> result{};
  const float *data = &matrix[0][0];
  for (std::size_t index = 0; index < result.size(); ++index) {
    result[index] = data[index];
  }
  return result;
}

std::array<float, 9> normal_matrix_for(const glm::mat4 &world) noexcept {
  const glm::mat3 linear(world);
  if (std::abs(glm::determinant(linear)) < 0.000001F) {
    return kIdentity3;
  }

  return to_array(glm::transpose(glm::inverse(linear)));
}

std::array<float, 3>
primitive_center(const stellar::assets::MeshPrimitive &primitive) noexcept {
  return {(primitive.bounds_min[0] + primitive.bounds_max[0]) * 0.5F,
          (primitive.bounds_min[1] + primitive.bounds_max[1]) * 0.5F,
          (primitive.bounds_min[2] + primitive.bounds_max[2]) * 0.5F};
}

float view_space_depth(
    const glm::mat4 &view, const glm::mat4 &world,
    const stellar::assets::MeshPrimitive &primitive) noexcept {
  const auto center = primitive_center(primitive);
  const glm::vec4 view_position =
      view * world * glm::vec4(center[0], center[1], center[2], 1.0F);
  return view_position.z;
}

std::optional<MaterialTextureBinding> resolve_level_texture_binding(
    std::size_t texture_index,
    const stellar::assets::LevelGeometryAsset &geometry,
    const std::vector<TextureHandle> &texture_handles) {
  if (texture_index >= texture_handles.size() ||
      !texture_handles[texture_index]) {
    return std::nullopt;
  }

  stellar::assets::SamplerAsset sampler;
  const auto &texture = geometry.textures[texture_index];
  if (texture.sampler_index.has_value() &&
      *texture.sampler_index < geometry.samplers.size()) {
    sampler = geometry.samplers[*texture.sampler_index];
  }

  return MaterialTextureBinding{.texture = texture_handles[texture_index],
                                .sampler = sampler,
                                .texcoord_set = 0};
}

std::optional<MaterialTextureBinding> resolve_level_lightmap_binding(
    std::size_t lightmap_index,
    const stellar::assets::LevelGeometryAsset &geometry,
    const std::vector<TextureHandle> &lightmap_texture_handles) {
  if (lightmap_index >= geometry.lightmaps.size() ||
      lightmap_index >= lightmap_texture_handles.size() ||
      !lightmap_texture_handles[lightmap_index]) {
    return std::nullopt;
  }

  stellar::assets::SamplerAsset sampler;
  sampler.name = "level_lightmap_linear_clamp";
  sampler.mag_filter = stellar::assets::TextureFilter::kLinear;
  sampler.min_filter = stellar::assets::TextureFilter::kLinear;
  sampler.wrap_s = stellar::assets::TextureWrapMode::kClampToEdge;
  sampler.wrap_t = stellar::assets::TextureWrapMode::kClampToEdge;

  return MaterialTextureBinding{
      .texture = lightmap_texture_handles[lightmap_index],
      .sampler = sampler,
      .texcoord_set = 1};
}

stellar::assets::MaterialAsset material_asset_for_level_surface(
    const stellar::assets::LevelSurfaceMaterial &surface_material) {
  stellar::assets::MaterialAsset material;
  material.name = surface_material.name.empty() ? surface_material.source_name
                                                : surface_material.name;
  if (material.name.empty()) {
    material.name = kDefaultLevelMaterialName;
  }
  material.base_color_factor = {0.75F, 0.75F, 0.75F, 1.0F};
  material.roughness_factor = 1.0F;
  material.metallic_factor = 0.0F;
  material.unlit = true;
  return material;
}

stellar::assets::MeshAsset mesh_for_billboard_quad(const BillboardQuad &quad) {
  stellar::assets::MeshPrimitive primitive;
  primitive.vertices.resize(4);
  for (std::size_t index = 0; index < quad.positions.size(); ++index) {
    primitive.vertices[index].position = quad.positions[index];
    primitive.vertices[index].normal = {0.0F, 0.0F, 1.0F};
    primitive.vertices[index].uv0 = quad.texcoords[index];
    primitive.vertices[index].color = quad.color;
  }
  primitive.indices = {0, 1, 2, 0, 2, 3};
  primitive.has_colors = true;
  primitive.material_index = 0;
  primitive.bounds_min = quad.positions[0];
  primitive.bounds_max = quad.positions[0];
  for (const auto &position : quad.positions) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
      primitive.bounds_min[axis] =
          std::min(primitive.bounds_min[axis], position[axis]);
      primitive.bounds_max[axis] =
          std::max(primitive.bounds_max[axis], position[axis]);
    }
  }

  return stellar::assets::MeshAsset{.name = "billboard_sprite",
                                    .primitives = {primitive}};
}

MaterialUpload material_for_billboard_quad(const BillboardQuad &quad) {
  stellar::assets::MaterialAsset material;
  material.name = "billboard_sprite";
  material.base_color_factor = quad.color;
  material.double_sided = true;
  material.unlit = true;
  material.alpha_cutoff = quad.alpha_cutoff;
  if (quad.alpha_mask) {
    material.alpha_mode = stellar::assets::AlphaMode::kMask;
  } else if (quad.alpha_blend) {
    material.alpha_mode = stellar::assets::AlphaMode::kBlend;
  } else {
    material.alpha_mode = stellar::assets::AlphaMode::kOpaque;
  }

  MaterialUpload upload;
  upload.material = material;
  if (quad.texture) {
    upload.base_color_texture = MaterialTextureBinding{
        .texture = quad.texture, .sampler = quad.sampler, .texcoord_set = 0};
  }
  return upload;
}

} // namespace

RenderLevel::~RenderLevel() noexcept { destroy(); }

std::expected<void, stellar::platform::Error>
RenderLevel::initialize(std::unique_ptr<GraphicsDevice> device,
                        stellar::platform::Window &window,
                        stellar::assets::LevelAsset level) {
  destroy();

  if (!device) {
    return std::unexpected(stellar::platform::Error("Graphics device is null"));
  }

  device_ = std::move(device);
  level_ = std::move(level);
  lightstyle_multipliers_.fill(1.0F);

  if (auto result = device_->initialize(window); !result) {
    destroy();
    return result;
  }

  const auto &geometry = level_.geometry;
  mesh_handles_.reserve(geometry.meshes.size());
  for (const auto &mesh : geometry.meshes) {
    auto handle = device_->create_mesh(mesh);
    if (!handle) {
      destroy();
      return std::unexpected(handle.error());
    }
    mesh_handles_.push_back(*handle);
  }

  texture_handles_.resize(geometry.textures.size());
  owned_texture_handles_.reserve(geometry.textures.size());
  for (std::size_t texture_index = 0; texture_index < geometry.textures.size();
       ++texture_index) {
    const auto &texture = geometry.textures[texture_index];
    if (!texture.image_index.has_value() ||
        *texture.image_index >= geometry.images.size()) {
      continue;
    }

    const auto &image = geometry.images[*texture.image_index];
    auto handle = device_->create_texture(
        TextureUpload{.image = image, .color_space = texture.color_space});
    if (!handle) {
      destroy();
      return std::unexpected(handle.error());
    }
    texture_handles_[texture_index] = *handle;
    owned_texture_handles_.push_back(*handle);
  }

  lightmap_texture_handles_.resize(geometry.lightmaps.size());
  for (std::size_t lightmap_index = 0;
       lightmap_index < geometry.lightmaps.size(); ++lightmap_index) {
    const auto &lightmap = geometry.lightmaps[lightmap_index];
    if (lightmap.image_index >= geometry.images.size()) {
      continue;
    }

    const auto &image = geometry.images[lightmap.image_index];
    auto handle = device_->create_texture(TextureUpload{
        .image = image,
        .color_space = stellar::assets::TextureColorSpace::kLinear});
    if (!handle) {
      destroy();
      return std::unexpected(handle.error());
    }
    lightmap_texture_handles_[lightmap_index] = *handle;
    owned_texture_handles_.push_back(*handle);
  }

  stellar::assets::MaterialAsset default_material;
  default_material.name = kDefaultLevelMaterialName;
  default_material.base_color_factor = {0.75F, 0.75F, 0.75F, 1.0F};
  default_material.metallic_factor = 0.0F;
  default_material.roughness_factor = 1.0F;
  default_material.unlit = true;
  auto default_handle =
      device_->create_material(MaterialUpload{.material = default_material});
  if (!default_handle) {
    destroy();
    return std::unexpected(default_handle.error());
  }
  default_material_ = *default_handle;

  material_handles_.reserve(geometry.materials.size());
  for (const auto &surface_material : geometry.materials) {
    MaterialUpload upload;
    upload.material = material_asset_for_level_surface(surface_material);
    if (surface_material.texture_index.has_value()) {
      upload.material.base_color_texture = stellar::assets::MaterialTextureSlot{
          .texture_index = *surface_material.texture_index,
          .texcoord_set = 0,
      };
      upload.base_color_texture = resolve_level_texture_binding(
          *surface_material.texture_index, geometry, texture_handles_);
    }
    if (surface_material.lightmap_index.has_value()) {
      upload.lightmap_texture = resolve_level_lightmap_binding(
          *surface_material.lightmap_index, geometry, lightmap_texture_handles_);
      if (*surface_material.lightmap_index < geometry.lightmaps.size()) {
        const auto style = geometry.lightmaps[*surface_material.lightmap_index].style;
        upload.lightmap_multiplier = lightstyle_multipliers_[style];
      }
    }

    auto handle = device_->create_material(upload);
    if (!handle) {
      destroy();
      return std::unexpected(handle.error());
    }
    material_handles_.push_back(*handle);
  }

  return {};
}

void RenderLevel::render(int width, int height,
                          const std::array<float, 16> &view_projection,
                          const std::array<float, 16> &view) noexcept {
  render(width, height, view_projection, view, std::nullopt, nullptr, {});
}

void RenderLevel::render(
    int width, int height, const std::array<float, 16> &view_projection,
    const std::array<float, 16> &view,
    std::optional<std::array<float, 3>> camera_world_position) noexcept {
  render(width, height, view_projection, view, camera_world_position, nullptr,
         {});
}

void RenderLevel::render(int width, int height,
                          const std::array<float, 16> &view_projection,
                          const std::array<float, 16> &view,
                          const BillboardView &billboard_view,
                          std::span<const BillboardSprite> sprites) noexcept {
  render(width, height, view_projection, view, std::nullopt, &billboard_view,
         sprites);
}

void RenderLevel::render(
    int width, int height, const std::array<float, 16> &view_projection,
    const std::array<float, 16> &view,
    std::optional<std::array<float, 3>> camera_world_position,
    const BillboardView &billboard_view,
    std::span<const BillboardSprite> sprites) noexcept {
  render(width, height, view_projection, view, camera_world_position,
         &billboard_view, sprites);
}

void RenderLevel::render(
    int width, int height,
    const std::array<float, 16> &view_projection) noexcept {
  render(width, height, view_projection, view_projection, std::nullopt, nullptr,
         {});
}

void RenderLevel::render(int width, int height,
                          const std::array<float, 16> &view_projection,
                          const std::array<float, 16> &view,
                          std::optional<std::array<float, 3>>
                              camera_world_position,
                          const BillboardView *billboard_view,
                          std::span<const BillboardSprite> sprites) noexcept {
  if (!device_) {
    return;
  }

  device_->begin_frame(width, height);

  const glm::mat4 vp = to_glm_mat4(view_projection);
  const glm::mat4 view_matrix = to_glm_mat4(view);
  std::vector<QueuedLevelDraw> opaque_draws;
  std::vector<QueuedLevelDraw> blend_draws;
  queue_static_draws(vp, view_matrix, camera_world_position, opaque_draws,
                     blend_draws);

  std::sort(
      blend_draws.begin(), blend_draws.end(),
      [](const auto &lhs, const auto &rhs) { return lhs.depth < rhs.depth; });

  for (const QueuedLevelDraw &draw : opaque_draws) {
    device_->draw_mesh(
        draw.mesh, std::span<const MeshPrimitiveDrawCommand>(&draw.command, 1),
        draw.transforms);
  }
  for (const QueuedLevelDraw &draw : blend_draws) {
    device_->draw_mesh(
        draw.mesh, std::span<const MeshPrimitiveDrawCommand>(&draw.command, 1),
        draw.transforms);
  }

  if (billboard_view != nullptr && !sprites.empty()) {
    auto quads = build_billboard_quads(sprites, *billboard_view);
    draw_billboard_quads(quads, vp);
  }

  device_->end_frame();
}

void RenderLevel::queue_static_draws(
    const glm::mat4 &view_projection, const glm::mat4 &view,
    std::optional<std::array<float, 3>> camera_world_position,
    std::vector<QueuedLevelDraw> &opaque_draws,
    std::vector<QueuedLevelDraw> &blend_draws) noexcept {
  const glm::mat4 world(1.0F);
  const MeshDrawTransforms transforms{.mvp = to_array(view_projection * world),
                                      .world = kIdentity4,
                                      .normal = normal_matrix_for(world)};
  const auto &geometry = level_.geometry;
  std::vector<bool> visible_surfaces;
  if (!geometry.surfaces.empty() && camera_world_position.has_value()) {
    const auto leaf_index = stellar::assets::find_level_leaf_for_point(
        level_.visibility, *camera_world_position);
    if (leaf_index.has_value()) {
      visible_surfaces = stellar::assets::visible_level_surfaces_from_leaf(
          level_, *leaf_index);
      if (visible_surfaces.size() != geometry.surfaces.size()) {
        visible_surfaces.clear();
      }
    }
  }

  if (!geometry.surfaces.empty()) {
    for (std::size_t surface_index = 0; surface_index < geometry.surfaces.size();
         ++surface_index) {
      if (!visible_surfaces.empty() && !visible_surfaces[surface_index]) {
        continue;
      }
      const auto &surface = geometry.surfaces[surface_index];
      if (surface.mesh_index >= geometry.meshes.size() ||
          surface.mesh_index >= mesh_handles_.size()) {
        continue;
      }
      const auto &mesh = geometry.meshes[surface.mesh_index];
      if (surface.primitive_index >= mesh.primitives.size()) {
        continue;
      }

      const auto &primitive = mesh.primitives[surface.primitive_index];
      MaterialHandle material = default_material_;
      if (surface.material_index.has_value() &&
          *surface.material_index < material_handles_.size()) {
        material = material_handles_[*surface.material_index];
      }
      QueuedLevelDraw draw{
          .mesh = mesh_handles_[surface.mesh_index],
          .command = MeshPrimitiveDrawCommand{.primitive_index =
                                                  surface.primitive_index,
                                              .material = material},
          .transforms = transforms,
          .depth = view_space_depth(view, world, primitive)};
      opaque_draws.push_back(draw);
    }
    return;
  }

  for (std::size_t mesh_index = 0;
       mesh_index < geometry.meshes.size() && mesh_index < mesh_handles_.size();
       ++mesh_index) {
    const auto &mesh = geometry.meshes[mesh_index];
    for (std::size_t primitive_index = 0;
         primitive_index < mesh.primitives.size(); ++primitive_index) {
      const auto &primitive = mesh.primitives[primitive_index];
      MaterialHandle material = default_material_;
      if (primitive.material_index.has_value() &&
          *primitive.material_index < material_handles_.size()) {
        material = material_handles_[*primitive.material_index];
      }
      QueuedLevelDraw draw{
          .mesh = mesh_handles_[mesh_index],
          .command =
              MeshPrimitiveDrawCommand{.primitive_index = primitive_index,
                                       .material = material},
          .transforms = transforms,
          .depth = view_space_depth(view, world, primitive)};
      opaque_draws.push_back(draw);
    }
  }
}

void RenderLevel::draw_billboard_quads(
    std::span<const BillboardQuad> quads,
    const glm::mat4 &view_projection) noexcept {
  if (!device_) {
    return;
  }

  const MeshDrawTransforms transforms{.mvp = to_array(view_projection),
                                      .world = kIdentity4,
                                      .normal = kIdentity3};
  for (const BillboardQuad &quad : quads) {
    auto mesh = device_->create_mesh(mesh_for_billboard_quad(quad));
    if (!mesh) {
      continue;
    }
    auto material = device_->create_material(material_for_billboard_quad(quad));
    if (!material) {
      device_->destroy_mesh(*mesh);
      continue;
    }

    const MeshPrimitiveDrawCommand command{.primitive_index = 0,
                                           .material = *material};
    device_->draw_mesh(*mesh,
                       std::span<const MeshPrimitiveDrawCommand>(&command, 1),
                       transforms);
    device_->destroy_material(*material);
    device_->destroy_mesh(*mesh);
  }
}

void RenderLevel::destroy() noexcept {
  if (device_) {
    if (default_material_) {
      device_->destroy_material(default_material_);
    }
    for (auto handle : material_handles_) {
      device_->destroy_material(handle);
    }
    for (auto handle : owned_texture_handles_) {
      device_->destroy_texture(handle);
    }
    for (auto handle : mesh_handles_) {
      device_->destroy_mesh(handle);
    }
  }

  default_material_ = {};
  material_handles_.clear();
  texture_handles_.clear();
  lightmap_texture_handles_.clear();
  owned_texture_handles_.clear();
  mesh_handles_.clear();
  level_ = {};
  device_.reset();
}

} // namespace stellar::graphics
