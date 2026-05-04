#include "stellar/graphics/RenderLevel.hpp"

#include "stellar/assets/LevelVisibilityQueries.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string_view>

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

bool env_flag_enabled(const char *name) noexcept {
  const char *value = std::getenv(name);
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  const std::string_view text(value);
  return text == "1" || text == "true" || text == "TRUE" || text == "on" ||
         text == "ON";
}

std::size_t env_frame_limit(const char *name, std::size_t fallback) noexcept {
  const char *value = std::getenv(name);
  if (value == nullptr || value[0] == '\0') {
    return fallback;
  }
  char *end = nullptr;
  const unsigned long parsed = std::strtoul(value, &end, 10);
  if (end == value) {
    return fallback;
  }
  return static_cast<std::size_t>(parsed);
}

struct DebugLightmapStats {
  std::array<std::uint8_t, 3> min_rgb{255U, 255U, 255U};
  std::array<std::uint8_t, 3> max_rgb{0U, 0U, 0U};
  std::array<double, 3> average_rgb{0.0, 0.0, 0.0};
  std::size_t texel_count = 0;
  bool all_black = true;
};

DebugLightmapStats debug_stats_for_image(
    const stellar::assets::ImageAsset &image) noexcept {
  DebugLightmapStats stats;
  const std::size_t channels =
      image.format == stellar::assets::ImageFormat::kR8G8B8A8 ? 4U : 3U;
  if (image.width == 0U || image.height == 0U ||
      image.pixels.size() < channels) {
    stats.min_rgb = {0U, 0U, 0U};
    return stats;
  }
  stats.texel_count = image.pixels.size() / channels;
  std::array<std::uint64_t, 3> sums{};
  for (std::size_t i = 0; i + 2U < image.pixels.size(); i += channels) {
    for (std::size_t channel = 0; channel < 3U; ++channel) {
      const std::uint8_t value = image.pixels[i + channel];
      stats.min_rgb[channel] = std::min(stats.min_rgb[channel], value);
      stats.max_rgb[channel] = std::max(stats.max_rgb[channel], value);
      sums[channel] += value;
      stats.all_black = stats.all_black && value == 0U;
    }
  }
  for (std::size_t channel = 0; channel < 3U; ++channel) {
    stats.average_rgb[channel] = static_cast<double>(sums[channel]) /
                                 static_cast<double>(stats.texel_count);
  }
  return stats;
}

void log_lightmap_debug(const stellar::assets::LevelGeometryAsset &geometry,
                        const std::string &source_uri, bool disabled,
                        bool visualization) noexcept {
  std::fprintf(stderr,
               "[stellar][lightmaps] source=%s raw_lighting_bytes=%zu "
               "lightmaps=%zu materials=%zu disabled=%d visualization=%d\n",
               source_uri.c_str(), geometry.raw_lighting.size(),
               geometry.lightmaps.size(), geometry.materials.size(),
               disabled ? 1 : 0, visualization ? 1 : 0);
  for (std::size_t index = 0; index < geometry.lightmaps.size(); ++index) {
    const auto &lightmap = geometry.lightmaps[index];
    if (lightmap.image_index >= geometry.images.size()) {
      std::fprintf(stderr,
                   "[stellar][lightmaps] index=%zu image_index=%zu invalid=1\n",
                   index, lightmap.image_index);
      continue;
    }
    const auto &image = geometry.images[lightmap.image_index];
    const DebugLightmapStats stats = debug_stats_for_image(image);
    std::fprintf(stderr,
                 "[stellar][lightmaps] index=%zu source=%s size=%ux%u "
                 "image=%s texels=%zu min_rgb=(%u,%u,%u) "
                 "max_rgb=(%u,%u,%u) average_rgb=(%.3f,%.3f,%.3f) "
                 "all_black=%d\n",
                 index, lightmap.source_name.c_str(), lightmap.size[0],
                 lightmap.size[1], image.name.c_str(), stats.texel_count,
                 static_cast<unsigned>(stats.min_rgb[0]),
                 static_cast<unsigned>(stats.min_rgb[1]),
                 static_cast<unsigned>(stats.min_rgb[2]),
                 static_cast<unsigned>(stats.max_rgb[0]),
                 static_cast<unsigned>(stats.max_rgb[1]),
                 static_cast<unsigned>(stats.max_rgb[2]), stats.average_rgb[0],
                 stats.average_rgb[1], stats.average_rgb[2],
                 stats.all_black ? 1 : 0);
  }
  for (std::size_t material_index = 0;
       material_index < geometry.materials.size(); ++material_index) {
    const auto &material = geometry.materials[material_index];
    if (!material.lightmap_index.has_value()) {
      continue;
    }
    std::fprintf(stderr,
                 "[stellar][lightmaps] material_index=%zu material=%s "
                 "source=%s lightmap_index=%zu\n",
                 material_index, material.name.c_str(),
                 material.source_name.c_str(), *material.lightmap_index);
  }
}

[[nodiscard]] const char *texture_filter_name(
    stellar::assets::TextureFilter filter) noexcept {
  switch (filter) {
  case stellar::assets::TextureFilter::kUnspecified:
    return "unspecified";
  case stellar::assets::TextureFilter::kNearest:
    return "nearest";
  case stellar::assets::TextureFilter::kLinear:
    return "linear";
  case stellar::assets::TextureFilter::kNearestMipmapNearest:
    return "nearest_mipmap_nearest";
  case stellar::assets::TextureFilter::kLinearMipmapNearest:
    return "linear_mipmap_nearest";
  case stellar::assets::TextureFilter::kNearestMipmapLinear:
    return "nearest_mipmap_linear";
  case stellar::assets::TextureFilter::kLinearMipmapLinear:
    return "linear_mipmap_linear";
  }
  return "unknown";
}

[[nodiscard]] const char *texture_wrap_name(
    stellar::assets::TextureWrapMode wrap) noexcept {
  switch (wrap) {
  case stellar::assets::TextureWrapMode::kClampToEdge:
    return "clamp_to_edge";
  case stellar::assets::TextureWrapMode::kMirroredRepeat:
    return "mirrored_repeat";
  case stellar::assets::TextureWrapMode::kRepeat:
    return "repeat";
  }
  return "unknown";
}

[[nodiscard]] const char *texture_color_space_name(
    stellar::assets::TextureColorSpace color_space) noexcept {
  switch (color_space) {
  case stellar::assets::TextureColorSpace::kLinear:
    return "linear";
  case stellar::assets::TextureColorSpace::kSrgb:
    return "srgb";
  }
  return "unknown";
}

[[nodiscard]] const char *level_lighting_mode_name(
    stellar::assets::LevelLightingMode mode) noexcept {
  switch (mode) {
  case stellar::assets::LevelLightingMode::kFullbright:
    return "fullbright";
  case stellar::assets::LevelLightingMode::kBakedRequired:
    return "baked_required";
  }
  return "unknown";
}

void log_level_lighting_debug(
    const stellar::assets::LevelLightingAsset &lighting,
    bool disable_lightmaps, bool black_fallback_uploaded) noexcept {
  std::fprintf(stderr,
               "[stellar][lighting] mode=%s lightmap_intensity=%.6g "
               "lightmap_min=%.6g global_enabled=%d "
               "global_color=(%.6g,%.6g,%.6g) global_intensity=%.6g "
               "lightmaps_disabled=%d black_fallback_uploaded=%d\n",
               level_lighting_mode_name(lighting.mode), lighting.lightmap_intensity,
               lighting.lightmap_min, lighting.global_ambient.enabled ? 1 : 0,
               lighting.global_ambient.color[0], lighting.global_ambient.color[1],
               lighting.global_ambient.color[2], lighting.global_ambient.intensity,
               disable_lightmaps ? 1 : 0, black_fallback_uploaded ? 1 : 0);
}

[[nodiscard]] const char *image_format_name(
    stellar::assets::ImageFormat format) noexcept {
  switch (format) {
  case stellar::assets::ImageFormat::kUnknown:
    return "unknown";
  case stellar::assets::ImageFormat::kR8G8B8:
    return "r8g8b8";
  case stellar::assets::ImageFormat::kR8G8B8A8:
    return "r8g8b8a8";
  }
  return "unknown";
}

[[nodiscard]] const char *texture_source_kind(
    std::string_view source_uri) noexcept {
  if (source_uri.find("#miptex/") != std::string_view::npos) {
    return "embedded_miptex";
  }
  if (source_uri.find("#wad3/") != std::string_view::npos) {
    return "external_wad";
  }
  if (source_uri.find("#developer_texture/") != std::string_view::npos) {
    return "procedural_developer";
  }
  if (source_uri.find("#lightmap/") != std::string_view::npos) {
    return "lightmap";
  }
  return "unknown";
}

void log_texture_sample_debug(std::size_t material_index,
                              const stellar::assets::ImageAsset &image,
                              std::uint32_t x, std::uint32_t y) noexcept {
  if (x >= image.width || y >= image.height) {
    return;
  }
  const std::size_t channels =
      image.format == stellar::assets::ImageFormat::kR8G8B8A8 ? 4U : 3U;
  if (image.format == stellar::assets::ImageFormat::kUnknown ||
      image.width == 0U || image.height == 0U) {
    return;
  }
  const std::size_t offset =
      (static_cast<std::size_t>(y) * image.width + x) * channels;
  if (offset + channels > image.pixels.size()) {
    return;
  }
  const std::uint8_t alpha = channels == 4U ? image.pixels[offset + 3U] : 255U;
  std::fprintf(stderr,
               "[stellar][textures] material_index=%zu image=%s "
               "sample=(%u,%u) rgba=(%u,%u,%u,%u)\n",
               material_index, image.name.c_str(), static_cast<unsigned>(x),
               static_cast<unsigned>(y),
               static_cast<unsigned>(image.pixels[offset]),
               static_cast<unsigned>(image.pixels[offset + 1U]),
               static_cast<unsigned>(image.pixels[offset + 2U]),
               static_cast<unsigned>(alpha));
}

void log_texture_samples_debug(std::size_t material_index,
                               const stellar::assets::ImageAsset &image,
                               std::string_view material_name,
                               std::string_view source_name) noexcept {
  constexpr std::array<std::array<std::uint32_t, 2>, 7> kStandardSamples{{
      {0U, 0U},     {1U, 1U},     {16U, 16U},  {32U, 32U},
      {64U, 64U},   {96U, 96U},   {127U, 127U},
  }};
  for (const auto &sample : kStandardSamples) {
    log_texture_sample_debug(material_index, image, sample[0], sample[1]);
  }

  const bool is_grid_64 =
      material_name.find("grid_64") != std::string_view::npos ||
      source_name.find("grid_64") != std::string_view::npos;
  const bool is_wall_96 =
      material_name.find("wall_96") != std::string_view::npos ||
      source_name.find("wall_96") != std::string_view::npos;
  if (is_grid_64) {
    log_texture_sample_debug(material_index, image, 64U, 10U);
  }
  if (is_wall_96) {
    log_texture_sample_debug(material_index, image, 8U, 48U);
  }
}

void log_texture_debug(const stellar::assets::LevelGeometryAsset &geometry,
                       const std::string &source_uri) noexcept {
  std::fprintf(stderr,
               "[stellar][textures] source=%s materials=%zu textures=%zu "
               "images=%zu samplers=%zu surfaces=%zu\n",
               source_uri.c_str(), geometry.materials.size(),
               geometry.textures.size(), geometry.images.size(),
               geometry.samplers.size(), geometry.surfaces.size());

  for (std::size_t material_index = 0;
       material_index < geometry.materials.size(); ++material_index) {
    const auto &material = geometry.materials[material_index];
    const char *texture_name = "<none>";
    const char *image_name = "<none>";
    const char *image_source_uri = "<none>";
    const char *source_kind = "none";
    const char *sampler_name = "<default>";
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    stellar::assets::ImageFormat format = stellar::assets::ImageFormat::kUnknown;
    stellar::assets::TextureColorSpace color_space =
        stellar::assets::TextureColorSpace::kLinear;
    stellar::assets::SamplerAsset sampler;
    const stellar::assets::ImageAsset *sample_image = nullptr;
    std::optional<std::size_t> image_index;
    std::optional<std::size_t> sampler_index;

    if (material.texture_index.has_value() &&
        *material.texture_index < geometry.textures.size()) {
      const auto &texture = geometry.textures[*material.texture_index];
      texture_name = texture.name.c_str();
      color_space = texture.color_space;
      image_index = texture.image_index;
      sampler_index = texture.sampler_index;
      if (texture.image_index.has_value() &&
          *texture.image_index < geometry.images.size()) {
        const auto &image = geometry.images[*texture.image_index];
        image_name = image.name.c_str();
        image_source_uri = image.source_uri.c_str();
        source_kind = texture_source_kind(image.source_uri);
        width = image.width;
        height = image.height;
        format = image.format;
        sample_image = &image;
      }
      if (texture.sampler_index.has_value() &&
          *texture.sampler_index < geometry.samplers.size()) {
        sampler = geometry.samplers[*texture.sampler_index];
        sampler_name = sampler.name.c_str();
      }
    }

    const std::string texture_index_text = material.texture_index.has_value()
                                             ? std::to_string(*material.texture_index)
                                             : "none";
    const std::string image_index_text = image_index.has_value()
                                             ? std::to_string(*image_index)
                                             : "none";
    const std::string sampler_index_text = sampler_index.has_value()
                                                 ? std::to_string(*sampler_index)
                                                 : "none";
    const std::array<float, 4> base_color_factor =
        sample_image != nullptr
            ? std::array<float, 4>{1.0F, 1.0F, 1.0F, 1.0F}
            : std::array<float, 4>{0.75F, 0.75F, 0.75F, 1.0F};

    std::fprintf(stderr,
                 "[stellar][textures] material_index=%zu material=%s "
                 "source_material=%s texture_index=%s texture=%s "
                 "image_index=%s image=%s image_source_uri=%s "
                 "source_kind=%s dimensions=%ux%u format=%s "
                 "color_space=%s sampler_index=%s sampler=%s "
                 "mag=%s min=%s wrap_s=%s wrap_t=%s "
                 "base_color_factor=(%.3g,%.3g,%.3g,%.3g)\n",
                 material_index, material.name.c_str(),
                 material.source_name.c_str(), texture_index_text.c_str(),
                 texture_name, image_index_text.c_str(),
                 image_name, image_source_uri, source_kind, width, height,
                 image_format_name(format), texture_color_space_name(color_space),
                 sampler_index_text.c_str(),
                 sampler_name, texture_filter_name(sampler.mag_filter),
                 texture_filter_name(sampler.min_filter),
                 texture_wrap_name(sampler.wrap_s),
                 texture_wrap_name(sampler.wrap_t), base_color_factor[0],
                 base_color_factor[1], base_color_factor[2],
                 base_color_factor[3]);

    if (sample_image != nullptr) {
      log_texture_samples_debug(material_index, *sample_image, material.name,
                                material.source_name);
    }
  }

  for (std::size_t surface_index = 0; surface_index < geometry.surfaces.size();
       ++surface_index) {
    const auto &surface = geometry.surfaces[surface_index];
    if (surface.mesh_index >= geometry.meshes.size()) {
      continue;
    }
    const auto &mesh = geometry.meshes[surface.mesh_index];
    if (surface.primitive_index >= mesh.primitives.size()) {
      continue;
    }
    const auto &primitive = mesh.primitives[surface.primitive_index];
    if (primitive.vertices.empty()) {
      continue;
    }
    std::array<float, 2> uv_min{std::numeric_limits<float>::max(),
                                std::numeric_limits<float>::max()};
    std::array<float, 2> uv_max{std::numeric_limits<float>::lowest(),
                                std::numeric_limits<float>::lowest()};
    for (const auto &vertex : primitive.vertices) {
      uv_min[0] = std::min(uv_min[0], vertex.uv0[0]);
      uv_min[1] = std::min(uv_min[1], vertex.uv0[1]);
      uv_max[0] = std::max(uv_max[0], vertex.uv0[0]);
      uv_max[1] = std::max(uv_max[1], vertex.uv0[1]);
    }

    const char *material_name = "<none>";
    if (surface.material_index.has_value() &&
        *surface.material_index < geometry.materials.size()) {
      material_name = geometry.materials[*surface.material_index].name.c_str();
    }
    const std::string material_index_text = surface.material_index.has_value()
                                                ? std::to_string(*surface.material_index)
                                                : "none";
    std::fprintf(stderr,
                 "[stellar][textures] surface_index=%zu surface=%s "
                 "mesh_index=%zu primitive_index=%zu material_index=%s "
                 "material=%s uv0_min=(%.6g,%.6g) uv0_max=(%.6g,%.6g)\n",
                 surface_index, surface.name.c_str(), surface.mesh_index,
                 surface.primitive_index, material_index_text.c_str(),
                 material_name, uv_min[0], uv_min[1], uv_max[0], uv_max[1]);
  }
}

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

std::optional<MaterialTextureBinding> resolve_material_slot_binding(
    const stellar::assets::MaterialTextureSlot &slot,
    const stellar::assets::LevelGeometryAsset &geometry,
    const std::vector<TextureHandle> &texture_handles) {
  if (slot.texture_index >= texture_handles.size() ||
      !texture_handles[slot.texture_index]) {
    return std::nullopt;
  }

  stellar::assets::SamplerAsset sampler;
  const auto &texture = geometry.textures[slot.texture_index];
  if (texture.sampler_index.has_value() &&
      *texture.sampler_index < geometry.samplers.size()) {
    sampler = geometry.samplers[*texture.sampler_index];
  }

  return MaterialTextureBinding{
      .texture = texture_handles[slot.texture_index],
      .sampler = sampler,
      .texcoord_set = slot.transform.texcoord_set.value_or(slot.texcoord_set),
      .transform = slot.transform};
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

MaterialTextureBinding black_lightmap_binding(TextureHandle texture) {
  stellar::assets::SamplerAsset sampler;
  sampler.name = "baked_missing_black_lightmap_linear_clamp";
  sampler.mag_filter = stellar::assets::TextureFilter::kLinear;
  sampler.min_filter = stellar::assets::TextureFilter::kLinear;
  sampler.wrap_s = stellar::assets::TextureWrapMode::kClampToEdge;
  sampler.wrap_t = stellar::assets::TextureWrapMode::kClampToEdge;

  return MaterialTextureBinding{.texture = texture,
                                .sampler = sampler,
                                .texcoord_set = 1};
}

void apply_level_lighting_upload(
    const stellar::assets::LevelLightingAsset &lighting,
    MaterialUpload &upload) noexcept {
  upload.lightmap_min = lighting.lightmap_min;
  if (lighting.global_ambient.enabled) {
    upload.global_light_color = lighting.global_ambient.color;
    upload.global_light_intensity = lighting.global_ambient.intensity;
  }
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
  // Classic BSP rooms are often viewed from inside closed solids. Until the
  // importer normalizes face winding for an explicit interior-rendering
  // convention, render BSP level surfaces double-sided to avoid black interiors
  // caused by back-face culling.
  material.double_sided = true;
  return material;
}

std::optional<std::array<float, 3>>
camera_position_from_view(const glm::mat4 &view) noexcept {
  if (std::abs(glm::determinant(view)) < 0.000001F) {
    return std::nullopt;
  }
  const glm::mat4 inverse_view = glm::inverse(view);
  return std::array<float, 3>{inverse_view[3][0], inverse_view[3][1],
                              inverse_view[3][2]};
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
  debug_render_enabled_ = env_flag_enabled("STELLAR_DEBUG_RENDER");
  debug_render_frame_limit_ =
      env_frame_limit("STELLAR_DEBUG_RENDER_FRAMES",
                      debug_render_enabled_ ? 3U : 0U);
  debug_render_frame_index_ = 0;
  disable_lightmaps_ = env_flag_enabled("STELLAR_DISABLE_LIGHTMAPS") ||
                       env_flag_enabled("STELLAR_FORCE_FULLBRIGHT");
  debug_lightmaps_enabled_ = env_flag_enabled("STELLAR_DEBUG_LIGHTMAPS");
  force_lightmap_visualization_ =
      env_flag_enabled("STELLAR_FORCE_LIGHTMAP_VISUALIZATION");
  const bool debug_textures_enabled = env_flag_enabled("STELLAR_DEBUG_TEXTURES");

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

  if (!disable_lightmaps_) {
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

    if (level_.lighting.mode ==
        stellar::assets::LevelLightingMode::kBakedRequired) {
      stellar::assets::ImageAsset image;
      image.name = "baked_missing_black_lightmap";
      image.source_uri = "baked_missing_black_lightmap";
      image.width = 1;
      image.height = 1;
      image.format = stellar::assets::ImageFormat::kR8G8B8;
      image.pixels = {0U, 0U, 0U};
      auto handle = device_->create_texture(TextureUpload{
          .image = std::move(image),
          .color_space = stellar::assets::TextureColorSpace::kLinear});
      if (!handle) {
        destroy();
        return std::unexpected(handle.error());
      }
      black_lightmap_texture_ = *handle;
      owned_texture_handles_.push_back(*handle);
    }
  }

  const bool baked_required =
      level_.lighting.mode == stellar::assets::LevelLightingMode::kBakedRequired;
  const bool baked_required_lightmaps = baked_required && !disable_lightmaps_;

  stellar::assets::MaterialAsset default_material;
  default_material.name = kDefaultLevelMaterialName;
  default_material.base_color_factor = {0.75F, 0.75F, 0.75F, 1.0F};
  default_material.metallic_factor = 0.0F;
  default_material.roughness_factor = 1.0F;
  default_material.unlit = true;
  // This default can be used by imported BSP/static level surfaces with missing
  // material indices, so keep it culling-safe for the same interior winding
  // reason as explicit level materials.
  default_material.double_sided = true;
  MaterialUpload default_upload{.material = default_material};
  apply_level_lighting_upload(level_.lighting, default_upload);
  if (baked_required_lightmaps) {
    default_upload.material.unlit = false;
    if (black_lightmap_texture_) {
      default_upload.lightmap_texture =
          black_lightmap_binding(black_lightmap_texture_);
      default_upload.lightmap_multiplier = level_.lighting.lightmap_intensity;
    }
  }
  auto default_handle = device_->create_material(default_upload);
  if (!default_handle) {
    destroy();
    return std::unexpected(default_handle.error());
  }
  default_material_ = *default_handle;

  material_handles_.reserve(geometry.materials.size());
  for (const auto &surface_material : geometry.materials) {
    MaterialUpload upload;
    upload.material = surface_material.resolved_material.has_value()
                          ? *surface_material.resolved_material
                          : material_asset_for_level_surface(surface_material);
    if (!upload.material.base_color_texture.has_value() &&
        surface_material.texture_index.has_value()) {
      upload.material.base_color_texture = stellar::assets::MaterialTextureSlot{
          .texture_index = *surface_material.texture_index,
          .texcoord_set = 0,
      };
    }
    apply_level_lighting_upload(level_.lighting, upload);
    if (baked_required_lightmaps) {
      upload.material.unlit = false;
      upload.lightmap_multiplier = level_.lighting.lightmap_intensity;
    }
    if (upload.material.base_color_texture.has_value()) {
      upload.base_color_texture = resolve_material_slot_binding(
          *upload.material.base_color_texture, geometry, texture_handles_);
      if (upload.base_color_texture.has_value()) {
        upload.material.base_color_factor = {1.0F, 1.0F, 1.0F, 1.0F};
      }
    }
    if (upload.material.normal_texture.has_value()) {
      upload.normal_texture = resolve_material_slot_binding(
          *upload.material.normal_texture, geometry, texture_handles_);
    }
    if (upload.material.specular_texture.has_value()) {
      upload.specular_texture = resolve_material_slot_binding(
          *upload.material.specular_texture, geometry, texture_handles_);
    }
    if (upload.material.metallic_roughness_texture.has_value()) {
      upload.metallic_roughness_texture = resolve_material_slot_binding(
          *upload.material.metallic_roughness_texture, geometry,
          texture_handles_);
    }
    if (upload.material.occlusion_texture.has_value()) {
      upload.occlusion_texture = resolve_material_slot_binding(
          *upload.material.occlusion_texture, geometry, texture_handles_);
    }
    if (upload.material.emissive_texture.has_value()) {
      upload.emissive_texture = resolve_material_slot_binding(
          *upload.material.emissive_texture, geometry, texture_handles_);
    }
    std::optional<MaterialTextureBinding> lightmap_binding;
    if (!disable_lightmaps_ && surface_material.lightmap_index.has_value()) {
      lightmap_binding = resolve_level_lightmap_binding(
          *surface_material.lightmap_index, geometry, lightmap_texture_handles_);
      if (*surface_material.lightmap_index < geometry.lightmaps.size()) {
        const auto style =
            geometry.lightmaps[*surface_material.lightmap_index].style;
        upload.lightmap_multiplier = level_.lighting.lightmap_intensity *
                                     lightstyle_multipliers_[style];
      }
      if (force_lightmap_visualization_ && lightmap_binding.has_value()) {
        upload.base_color_texture = lightmap_binding;
        upload.material.base_color_factor = {1.0F, 1.0F, 1.0F, 1.0F};
        upload.lightmap_multiplier = 1.0F;
      } else {
        upload.lightmap_texture = lightmap_binding;
      }
    }
    if (!upload.lightmap_texture.has_value() && baked_required_lightmaps &&
        black_lightmap_texture_) {
      upload.lightmap_texture = black_lightmap_binding(black_lightmap_texture_);
      upload.lightmap_multiplier = level_.lighting.lightmap_intensity;
    }

    auto handle = device_->create_material(upload);
    if (!handle) {
      destroy();
      return std::unexpected(handle.error());
    }
    material_handles_.push_back(*handle);
  }

  if (debug_render_enabled_) {
    std::size_t primitive_count = 0;
    std::size_t index_count = 0;
    for (const auto &mesh : geometry.meshes) {
      primitive_count += mesh.primitives.size();
      for (const auto &primitive : mesh.primitives) {
        index_count += primitive.indices.size();
      }
    }
    std::size_t collision_mesh_count = 0;
    std::size_t collision_triangle_count = 0;
    if (level_.level_collision.has_value()) {
      collision_mesh_count = level_.level_collision->meshes.size();
      for (const auto &mesh : level_.level_collision->meshes) {
        collision_triangle_count += mesh.triangles.size();
      }
    }
    std::fprintf(stderr,
                 "[stellar][render] init source=%s meshes=%zu primitives=%zu "
                 "indices=%zu surfaces=%zu materials=%zu images=%zu textures=%zu "
                 "lightmaps=%zu raw_lighting_bytes=%zu visibility_available=%d "
                 "visibility_leaves=%zu level_collision_meshes=%zu "
                 "level_collision_triangles=%zu lightmaps_disabled=%d\n",
                 level_.source_uri.c_str(), geometry.meshes.size(), primitive_count,
                 index_count, geometry.surfaces.size(), geometry.materials.size(),
                 geometry.images.size(), geometry.textures.size(),
                 geometry.lightmaps.size(), geometry.raw_lighting.size(),
                 level_.visibility.available ? 1 : 0,
                 level_.visibility.leaves.size(), collision_mesh_count,
                 collision_triangle_count, disable_lightmaps_ ? 1 : 0);
  }
  if (debug_render_enabled_ || debug_lightmaps_enabled_) {
    log_level_lighting_debug(level_.lighting, disable_lightmaps_,
                             black_lightmap_texture_ != TextureHandle{});
  }
  if (debug_lightmaps_enabled_) {
    log_lightmap_debug(geometry, level_.source_uri, disable_lightmaps_,
                       force_lightmap_visualization_ && !disable_lightmaps_);
  }
  if (debug_textures_enabled) {
    log_texture_debug(geometry, level_.source_uri);
  }

  return {};
}

void RenderLevel::render(int width, int height,
                           const std::array<float, 16> &view_projection,
                           const std::array<float, 16> &view) noexcept {
  render(RenderLevelFrame{.width = width,
                          .height = height,
                          .view_projection = view_projection,
                          .view = view});
}

void RenderLevel::render(
    int width, int height, const std::array<float, 16> &view_projection,
    const std::array<float, 16> &view,
    std::optional<std::array<float, 3>> camera_world_position) noexcept {
  render(RenderLevelFrame{.width = width,
                          .height = height,
                          .view_projection = view_projection,
                          .view = view,
                          .camera_world_position = camera_world_position});
}

void RenderLevel::render(int width, int height,
                          const std::array<float, 16> &view_projection,
                           const std::array<float, 16> &view,
                           const BillboardView &billboard_view,
                           std::span<const BillboardSprite> sprites) noexcept {
  render(RenderLevelFrame{.width = width,
                          .height = height,
                          .view_projection = view_projection,
                          .view = view,
                          .billboard_view = &billboard_view,
                          .sprites = sprites});
}

void RenderLevel::render(
    int width, int height, const std::array<float, 16> &view_projection,
    const std::array<float, 16> &view,
    std::optional<std::array<float, 3>> camera_world_position,
    const BillboardView &billboard_view,
    std::span<const BillboardSprite> sprites) noexcept {
  render(RenderLevelFrame{.width = width,
                          .height = height,
                          .view_projection = view_projection,
                          .view = view,
                          .camera_world_position = camera_world_position,
                          .billboard_view = &billboard_view,
                          .sprites = sprites});
}

void RenderLevel::render(
    int width, int height,
    const std::array<float, 16> &view_projection) noexcept {
  render(RenderLevelFrame{.width = width,
                          .height = height,
                          .view_projection = view_projection,
                          .view = view_projection});
}

void RenderLevel::render(const RenderLevelFrame &frame) noexcept {
  if (!device_) {
    return;
  }

  device_->begin_frame(frame.width, frame.height);

  const glm::mat4 vp = to_glm_mat4(frame.view_projection);
  const glm::mat4 view_matrix = to_glm_mat4(frame.view);
  std::vector<QueuedLevelDraw> opaque_draws;
  std::vector<QueuedLevelDraw> blend_draws;
  const StaticDrawQueueStats static_stats = queue_static_draws(
      vp, view_matrix, frame.camera_world_position, opaque_draws, blend_draws);

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

  if (frame.billboard_view != nullptr && !frame.sprites.empty()) {
    auto quads = build_billboard_quads(frame.sprites, *frame.billboard_view);
    draw_billboard_quads(quads, vp);
  }

  if (debug_render_enabled_ &&
      debug_render_frame_index_ < debug_render_frame_limit_) {
    std::fprintf(stderr,
                 "[stellar][render] frame=%zu render_view_camera_position=%d "
                 "visibility_used=%d visibility_visible=%zu opaque_draws=%zu "
                 "blend_draws=%zu\n",
                  debug_render_frame_index_,
                  frame.camera_world_position.has_value() ? 1 : 0,
                 static_stats.visibility_used ? 1 : 0,
                 static_stats.visibility_visible_count, opaque_draws.size(),
                 blend_draws.size());
  }
  ++debug_render_frame_index_;

  device_->end_frame();
}

RenderLevel::StaticDrawQueueStats RenderLevel::queue_static_draws(
    const glm::mat4 &view_projection, const glm::mat4 &view,
    std::optional<std::array<float, 3>> camera_world_position,
    std::vector<QueuedLevelDraw> &opaque_draws,
    std::vector<QueuedLevelDraw> &blend_draws) noexcept {
  StaticDrawQueueStats stats;
  const glm::mat4 world(1.0F);
  const auto resolved_camera_position =
      camera_world_position.has_value() ? camera_world_position
                                        : camera_position_from_view(view);
  const MeshDrawTransforms transforms{.mvp = to_array(view_projection * world),
                                      .world = kIdentity4,
                                      .normal = normal_matrix_for(world),
                                      .camera_world_position =
                                          resolved_camera_position.value_or(
                                              std::array<float, 3>{}),
                                      .has_camera_world_position =
                                          resolved_camera_position.has_value()};
  const auto &geometry = level_.geometry;
  std::vector<bool> visible_surfaces;
  if (!geometry.surfaces.empty() && camera_world_position.has_value() &&
      !env_flag_enabled("STELLAR_DISABLE_VISIBILITY_CULLING")) {
    const auto leaf_index = stellar::assets::find_level_leaf_for_point(
        level_.visibility, *camera_world_position);
    if (leaf_index.has_value()) {
      visible_surfaces = stellar::assets::visible_level_surfaces_from_leaf(
          level_, *leaf_index);
      if (visible_surfaces.size() != geometry.surfaces.size()) {
        visible_surfaces.clear();
      } else {
        stats.visibility_used = true;
        stats.visibility_visible_count = static_cast<std::size_t>(
            std::count(visible_surfaces.begin(), visible_surfaces.end(), true));
        if (stats.visibility_visible_count == 0) {
          visible_surfaces.clear();
          stats.visibility_used = false;
          stats.visibility_visible_count = geometry.surfaces.size();
          if (debug_render_enabled_) {
            std::fprintf(stderr,
                         "[stellar][render] warning: visibility resolved zero "
                         "surfaces for nonempty level; falling back to all "
                         "surfaces visible\n");
          }
        }
      }
    }
  }
  if (visible_surfaces.empty()) {
    stats.visibility_visible_count = geometry.surfaces.size();
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
    return stats;
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
  return stats;
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
  black_lightmap_texture_ = {};
  material_handles_.clear();
  texture_handles_.clear();
  lightmap_texture_handles_.clear();
  owned_texture_handles_.clear();
  mesh_handles_.clear();
  level_ = {};
  debug_lightmaps_enabled_ = false;
  force_lightmap_visualization_ = false;
  disable_lightmaps_ = false;
  device_.reset();
}

} // namespace stellar::graphics
