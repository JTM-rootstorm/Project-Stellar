#include "BspBinary.hpp"
#include "DeveloperTextures.hpp"
#include "Wad3Reader.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace stellar::import::bsp::detail {
namespace {

using Vec3 = std::array<float, 3>;

[[nodiscard]] const std::string *value_for(const Entity &entity,
                                            std::string_view key) noexcept {
  for (const auto &pair : entity.pairs) {
    if (pair.key == key) {
      return &pair.value;
    }
  }
  return nullptr;
}

[[nodiscard]] const std::string *value_for_alias(const Entity &entity,
                                                 std::string_view key,
                                                 std::string_view alias) noexcept {
  if (const std::string *value = value_for(entity, key)) {
    return value;
  }
  return value_for(entity, alias);
}

[[nodiscard]] std::string string_or(const Entity &entity, std::string_view key,
                                     std::string fallback = {}) {
  if (const std::string *value = value_for(entity, key)) {
    return *value;
  }
  return fallback;
}

[[nodiscard]] std::string string_or_alias(const Entity &entity,
                                           std::string_view key,
                                           std::string_view alias,
                                           std::string fallback = {}) {
  if (const std::string *value = value_for_alias(entity, key, alias)) {
    return *value;
  }
  return fallback;
}

struct CanonicalEntityName {
  std::string value;
  std::string_view key;
};

constexpr std::array<std::string_view, 4> kEntityNameKeys{
    "targetname", "_stellar_name", "stellar.name", "name"};

[[nodiscard]] std::optional<CanonicalEntityName>
canonical_entity_name_for(const Entity &entity) {
  for (std::string_view key : kEntityNameKeys) {
    if (const std::string *value = value_for(entity, key);
        value != nullptr && !value->empty()) {
      return CanonicalEntityName{.value = *value, .key = key};
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::string canonical_entity_name_or(const Entity &entity,
                                                    std::string fallback) {
  if (auto name = canonical_entity_name_for(entity)) {
    return name->value;
  }
  return fallback;
}

[[nodiscard]] std::optional<Vec3> parse_vec3(const std::string *text) {
  if (text == nullptr) {
    return std::nullopt;
  }
  std::istringstream stream(*text);
  Vec3 value{};
  if (stream >> value[0] >> value[1] >> value[2]) {
    std::string trailing;
    if (!(stream >> trailing)) {
      return value;
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<std::array<float, 2>> parse_vec2(const std::string *text) {
  if (text == nullptr) {
    return std::nullopt;
  }
  std::istringstream stream(*text);
  std::array<float, 2> value{};
  if (stream >> value[0] >> value[1]) {
    std::string trailing;
    if (!(stream >> trailing)) {
      return value;
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<bool> parse_bool_like(const std::string *text) {
  if (text == nullptr) {
    return std::nullopt;
  }
  std::string value;
  value.reserve(text->size());
  for (char character : *text) {
    value.push_back(static_cast<char>(
        std::tolower(static_cast<unsigned char>(character))));
  }
  if (value == "1" || value == "true" || value == "yes") {
    return true;
  }
  if (value == "0" || value == "false" || value == "no") {
    return false;
  }
  return std::nullopt;
}

[[nodiscard]] std::string lower_ascii(std::string_view text) {
  std::string value;
  value.reserve(text.size());
  for (const char character : text) {
    value.push_back(static_cast<char>(
        std::tolower(static_cast<unsigned char>(character))));
  }
  return value;
}

[[nodiscard]] std::optional<float> parse_float_value(const std::string *text) {
  if (text == nullptr) {
    return std::nullopt;
  }
  std::istringstream stream(*text);
  float value = 0.0F;
  if (stream >> value) {
    std::string trailing;
    if (!(stream >> trailing)) {
      return value;
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<stellar::assets::LevelLightingMode>
parse_lighting_mode_value(std::string_view text) {
  const std::string value = lower_ascii(text);
  if (value == "fullbright" || value == "fast" || value == "none") {
    return stellar::assets::LevelLightingMode::kFullbright;
  }
  if (value == "baked" || value == "baked_required" || value == "full") {
    return stellar::assets::LevelLightingMode::kBakedRequired;
  }
  return std::nullopt;
}

[[nodiscard]] const char *lighting_mode_name(
    stellar::assets::LevelLightingMode mode) noexcept {
  switch (mode) {
  case stellar::assets::LevelLightingMode::kFullbright:
    return "fullbright";
  case stellar::assets::LevelLightingMode::kBakedRequired:
    return "baked_required";
  }
  return "unknown";
}

[[nodiscard]] const std::string *lighting_mode_key_for(const Entity &entity) noexcept {
  for (const std::string_view key : {"_stellar_lighting_mode",
                                     "stellar.lighting_mode", "lighting_mode"}) {
    if (const std::string *value = value_for(entity, key)) {
      return value;
    }
  }
  return nullptr;
}

[[nodiscard]] bool has_face_lightmap_reference(const BspMap &map) noexcept {
  for (const Face &face : map.faces) {
    if (face.light_offset >= 0) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::size_t face_lightmap_reference_count(const BspMap &map) noexcept {
  std::size_t count = 0U;
  for (const Face &face : map.faces) {
    if (face.light_offset >= 0) {
      ++count;
    }
  }
  return count;
}

[[nodiscard]] stellar::assets::LevelLightingMode infer_lighting_mode(
    const BspMap &map, const stellar::assets::LevelGeometryAsset &geometry) noexcept {
  if (!geometry.raw_lighting.empty() || has_face_lightmap_reference(map)) {
    return stellar::assets::LevelLightingMode::kBakedRequired;
  }
  return stellar::assets::LevelLightingMode::kFullbright;
}

[[nodiscard]] std::size_t material_lightmap_count(
    const stellar::assets::LevelGeometryAsset &geometry) noexcept {
  std::size_t count = 0U;
  for (const auto &material : geometry.materials) {
    if (material.lightmap_index.has_value() &&
        *material.lightmap_index < geometry.lightmaps.size()) {
      ++count;
    }
  }
  return count;
}

[[nodiscard]] std::array<float, 3> normalize_color_255(Vec3 color) noexcept {
  return {std::clamp(color[0], 0.0F, 255.0F) / 255.0F,
          std::clamp(color[1], 0.0F, 255.0F) / 255.0F,
          std::clamp(color[2], 0.0F, 255.0F) / 255.0F};
}

[[nodiscard]] std::optional<stellar::assets::LevelGlobalLight>
parse_global_light_components(const std::string *color_text,
                              const std::string *intensity_text,
                              const std::string *enabled_text,
                              bool default_enable_from_intensity,
                              float fallback_intensity) {
  stellar::assets::LevelGlobalLight light{};
  if (auto color = parse_vec3(color_text)) {
    light.color = normalize_color_255(*color);
  } else if (color_text != nullptr) {
    return std::nullopt;
  }

  if (auto intensity = parse_float_value(intensity_text)) {
    light.intensity = std::clamp(*intensity, 0.0F, 16.0F);
  } else if (intensity_text != nullptr) {
    return std::nullopt;
  } else {
    light.intensity = std::clamp(fallback_intensity, 0.0F, 16.0F);
  }

  if (auto enabled = parse_bool_like(enabled_text)) {
    light.enabled = *enabled;
  } else if (enabled_text != nullptr) {
    return std::nullopt;
  } else if (default_enable_from_intensity) {
    light.enabled = light.intensity > 0.0F;
  }
  if (light.intensity <= 0.0F) {
    light.enabled = false;
  }
  return light;
}

[[nodiscard]] std::optional<stellar::assets::LevelGlobalLight>
parse_global_light_tuple(const std::string *text) {
  if (text == nullptr) {
    return std::nullopt;
  }
  std::istringstream stream(*text);
  Vec3 color{};
  if (!(stream >> color[0] >> color[1] >> color[2])) {
    return std::nullopt;
  }
  float intensity = 1.0F;
  if (stream >> intensity) {
    std::string trailing;
    if (stream >> trailing) {
      return std::nullopt;
    }
  }
  stellar::assets::LevelGlobalLight light{};
  light.color = normalize_color_255(color);
  light.intensity = std::clamp(intensity, 0.0F, 16.0F);
  light.enabled = light.intensity > 0.0F;
  return light;
}

[[nodiscard]] bool is_global_light_class(std::string_view classname) noexcept {
  return classname == "stellar_global_light" || classname == "light_global" ||
         classname == "light_ambient";
}

[[nodiscard]] std::optional<int> parse_model_index(const std::string *model) {
  if (model == nullptr || model->size() < 2 || (*model)[0] != '*') {
    return std::nullopt;
  }
  char *end = nullptr;
  const long value = std::strtol(model->c_str() + 1, &end, 10);
  if (end == model->c_str() + model->size() && value >= 0 &&
      value <= std::numeric_limits<int>::max()) {
    return static_cast<int>(value);
  }
  return std::nullopt;
}

[[nodiscard]] Vec3 subtract(Vec3 a, Vec3 b) noexcept {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

[[nodiscard]] Vec3 cross(Vec3 a, Vec3 b) noexcept {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}

[[nodiscard]] Vec3 normalize(Vec3 v) noexcept {
  const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (len <= 0.000001F) {
    return {0.0F, 0.0F, 1.0F};
  }
  return {v[0] / len, v[1] / len, v[2] / len};
}

void include_point(Vec3 &mins, Vec3 &maxs, const Vec3 &point) noexcept {
  for (std::size_t i = 0; i < 3; ++i) {
    mins[i] = std::min(mins[i], point[i]);
    maxs[i] = std::max(maxs[i], point[i]);
  }
}

[[nodiscard]] std::vector<Vec3> face_vertices(const BspMap &map,
                                              const Face &face) {
  std::vector<Vec3> vertices;
  if (face.first_edge < 0) {
    return vertices;
  }
  const auto first_edge = static_cast<std::size_t>(face.first_edge);
  for (std::uint16_t i = 0; i < face.edge_count; ++i) {
    const std::size_t surfedge_index = first_edge + i;
    if (surfedge_index >= map.surfedges.size()) {
      return {};
    }
    const std::int32_t surfedge = map.surfedges[surfedge_index];
    const std::size_t edge_index = static_cast<std::size_t>(std::abs(surfedge));
    if (edge_index >= map.edges.size()) {
      return {};
    }
    const Edge &edge = map.edges[edge_index];
    const std::uint16_t vertex_index = surfedge >= 0 ? edge.first : edge.second;
    if (vertex_index >= map.vertices.size()) {
      return {};
    }
    vertices.push_back(map.vertices[vertex_index].position);
  }
  return vertices;
}

[[nodiscard]] std::string texture_name_for(const BspMap &map,
                                            const Face &face) {
  if (face.texinfo >= map.texinfos.size()) {
    return "__missing_texture";
  }
  const std::int32_t miptex = map.texinfos[face.texinfo].miptex;
  if (miptex >= 0 &&
      static_cast<std::size_t>(miptex) < map.texture_names.size() &&
      !map.texture_names[static_cast<std::size_t>(miptex)].empty()) {
    return map.texture_names[static_cast<std::size_t>(miptex)];
  }
  return "__texture_" + std::to_string(miptex);
}

[[nodiscard]] const Miptex *texture_for_face(const BspMap &map,
                                             const Face &face) noexcept {
  if (face.texinfo >= map.texinfos.size()) {
    return nullptr;
  }
  const std::int32_t miptex = map.texinfos[face.texinfo].miptex;
  if (miptex < 0 || static_cast<std::size_t>(miptex) >= map.textures.size()) {
    return nullptr;
  }
  return &map.textures[static_cast<std::size_t>(miptex)];
}

void add_warning(ImportReport *report, DiagnosticCode code,
                 const std::string &source_uri, std::string message,
                 std::optional<std::size_t> lump_index = std::nullopt,
                 std::optional<std::size_t> face_index = std::nullopt) {
  if (report == nullptr) {
    return;
  }
  report->diagnostics.push_back(Diagnostic{.severity = DiagnosticSeverity::kWarning,
                                           .code = code,
                                           .message = std::move(message),
                                           .source_uri = source_uri,
                                           .lump_index = lump_index,
                                           .entity_index = std::nullopt,
                                           .face_index = face_index});
}

void add_info(ImportReport *report, DiagnosticCode code,
              const std::string &source_uri, std::string message,
              std::optional<std::size_t> lump_index = std::nullopt,
              std::optional<std::size_t> face_index = std::nullopt) {
  if (report == nullptr) {
    return;
  }
  report->diagnostics.push_back(Diagnostic{.severity = DiagnosticSeverity::kInfo,
                                           .code = code,
                                           .message = std::move(message),
                                           .source_uri = source_uri,
                                           .lump_index = lump_index,
                                           .entity_index = std::nullopt,
                                           .face_index = face_index});
}

void add_entity_warning(ImportReport *report, DiagnosticCode code,
                        const std::string &source_uri, std::size_t entity_index,
                        std::string message) {
  if (report == nullptr) {
    return;
  }
  report->diagnostics.push_back(Diagnostic{.severity = DiagnosticSeverity::kWarning,
                                           .code = code,
                                           .message = std::move(message),
                                           .source_uri = source_uri,
                                           .lump_index = static_cast<std::size_t>(LumpIndex::kEntities),
                                           .entity_index = entity_index,
                                           .face_index = std::nullopt});
}

struct TextureBuildResult {
  std::optional<std::size_t> texture_index;
  std::optional<std::array<std::uint32_t, 2>> texel_size;
};

[[nodiscard]] std::string texture_lookup_key(std::string_view name) {
  std::string key;
  key.reserve(name.size());
  for (const char ch : name) {
    key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return key;
}

[[nodiscard]] TextureBuildResult
texture_asset_index(stellar::assets::LevelGeometryAsset &geometry,
                    std::map<std::string, std::size_t> &textures,
                    const std::unordered_map<std::string, WadTextureAsset> &wad_textures,
                    const std::string &texture_name,
                    const Miptex *miptex, const std::string &source_uri,
                    ImportReport *report) {
  if (texture_name.empty()) {
    return {};
  }
  const auto existing = textures.find(texture_name);
  if (existing != textures.end()) {
    const auto &texture = geometry.textures[existing->second];
    if (texture.image_index.has_value() &&
        *texture.image_index < geometry.images.size()) {
      const auto &image = geometry.images[*texture.image_index];
      return TextureBuildResult{.texture_index = existing->second,
                                .texel_size = std::array<std::uint32_t, 2>{
                                    image.width, image.height}};
    }
    return TextureBuildResult{.texture_index = existing->second};
  }

  if (miptex != nullptr && miptex->has_embedded_pixels &&
      !miptex->pixels.empty()) {
    stellar::assets::ImageAsset image{};
    image.name = miptex->name;
    image.width = miptex->width;
    image.height = miptex->height;
    image.format = stellar::assets::ImageFormat::kR8G8B8A8;
    image.source_uri = source_uri + "#miptex/" + miptex->name;
    image.pixels.reserve(miptex->pixels.size() * 4);
    for (const std::uint8_t index : miptex->pixels) {
      image.pixels.push_back(index);
      image.pixels.push_back(index);
      image.pixels.push_back(index);
      image.pixels.push_back(255U);
    }
    const std::size_t image_index = geometry.images.size();
    geometry.images.push_back(std::move(image));

    const std::size_t texture_index = geometry.textures.size();
    geometry.textures.push_back(stellar::assets::TextureAsset{
        .name = miptex->name,
        .image_index = image_index,
        .sampler_index = std::nullopt,
        .color_space = stellar::assets::TextureColorSpace::kSrgb});
    textures.emplace(texture_name, texture_index);
    return TextureBuildResult{.texture_index = texture_index,
                              .texel_size = std::array<std::uint32_t, 2>{
                                  miptex->width, miptex->height}};
  }

  if (const auto wad = wad_textures.find(texture_lookup_key(texture_name));
      wad != wad_textures.end()) {
    stellar::assets::ImageAsset image = wad->second.image;
    image.name = texture_name;
    const std::size_t image_index = geometry.images.size();
    const std::array<std::uint32_t, 2> texel_size{image.width, image.height};
    geometry.images.push_back(std::move(image));

    stellar::assets::SamplerAsset sampler;
    sampler.name = texture_name + "_wad_repeat";
    sampler.mag_filter = stellar::assets::TextureFilter::kNearest;
    sampler.min_filter = stellar::assets::TextureFilter::kNearest;
    sampler.wrap_s = stellar::assets::TextureWrapMode::kRepeat;
    sampler.wrap_t = stellar::assets::TextureWrapMode::kRepeat;
    const std::size_t sampler_index = geometry.samplers.size();
    geometry.samplers.push_back(std::move(sampler));

    const std::size_t texture_index = geometry.textures.size();
    geometry.textures.push_back(stellar::assets::TextureAsset{
        .name = texture_name,
        .image_index = image_index,
        .sampler_index = sampler_index,
        .color_space = stellar::assets::TextureColorSpace::kSrgb});
    textures.emplace(texture_name, texture_index);
    return TextureBuildResult{.texture_index = texture_index,
                              .texel_size = texel_size};
  }

  if (auto developer_texture = make_developer_texture(texture_name, source_uri)) {
    const std::size_t image_index = geometry.images.size();
    const std::array<std::uint32_t, 2> texel_size{
        developer_texture->image.width, developer_texture->image.height};
    geometry.images.push_back(std::move(developer_texture->image));

    const std::size_t sampler_index = geometry.samplers.size();
    geometry.samplers.push_back(std::move(developer_texture->sampler));

    const std::size_t texture_index = geometry.textures.size();
    developer_texture->texture.image_index = image_index;
    developer_texture->texture.sampler_index = sampler_index;
    geometry.textures.push_back(std::move(developer_texture->texture));
    textures.emplace(texture_name, texture_index);
    return TextureBuildResult{.texture_index = texture_index,
                              .texel_size = texel_size};
  }

  if (miptex != nullptr) {
    add_warning(report, DiagnosticCode::kMissingTexture, source_uri,
                source_uri + ": BSP texture '" + texture_name +
                    "' references external WAD data; using fallback material",
                static_cast<std::size_t>(LumpIndex::kTextures));
  }
  return {};
}

[[nodiscard]] std::size_t
material_index(stellar::assets::LevelGeometryAsset &geometry,
                std::map<std::string, std::size_t> &materials,
                const std::string &name,
                std::optional<std::size_t> texture_index,
                std::optional<std::size_t> lightmap_index) {
  const std::string key = name + "|tex=" +
                          (texture_index ? std::to_string(*texture_index) : "none") +
                          "|lm=" +
                          (lightmap_index ? std::to_string(*lightmap_index) : "none");
  const auto existing = materials.find(key);
  if (existing != materials.end()) {
    return existing->second;
  }
  const std::size_t index = geometry.materials.size();
  geometry.materials.push_back(stellar::assets::LevelSurfaceMaterial{
      .name = name, .texture_index = texture_index, .lightmap_index = lightmap_index,
      .source_name = name});
  materials.emplace(key, index);
  return index;
}

struct LightmapBuildResult {
  std::optional<std::size_t> lightmap_index;
  std::optional<std::array<float, 2>> uv_min;
  std::optional<std::array<float, 2>> uv_extent;
};

struct LightmapRgbStats {
  std::array<std::uint8_t, 3> min_rgb{255U, 255U, 255U};
  std::array<std::uint8_t, 3> max_rgb{0U, 0U, 0U};
  std::array<double, 3> average_rgb{0.0, 0.0, 0.0};
  std::uint64_t texel_count = 0;
  bool all_black = true;
};

[[nodiscard]] LightmapRgbStats rgb_stats_for_pixels(
    std::span<const std::uint8_t> pixels) noexcept {
  LightmapRgbStats stats{};
  stats.texel_count = static_cast<std::uint64_t>(pixels.size() / 3U);
  if (stats.texel_count == 0U) {
    stats.min_rgb = {0U, 0U, 0U};
    return stats;
  }
  std::array<std::uint64_t, 3> sums{};
  for (std::size_t i = 0; i + 2 < pixels.size(); i += 3U) {
    for (std::size_t channel = 0; channel < 3U; ++channel) {
      const std::uint8_t value = pixels[i + channel];
      stats.min_rgb[channel] = std::min(stats.min_rgb[channel], value);
      stats.max_rgb[channel] = std::max(stats.max_rgb[channel], value);
      sums[channel] += value;
      stats.all_black = stats.all_black && value == 0U;
    }
  }
  for (std::size_t channel = 0; channel < 3U; ++channel) {
    stats.average_rgb[channel] =
        static_cast<double>(sums[channel]) / static_cast<double>(stats.texel_count);
  }
  return stats;
}

[[nodiscard]] std::string lightmap_stats_message(
    const std::string &source_uri, std::string_view prefix,
    std::size_t raw_lighting_bytes, std::size_t imported_lightmap_count,
    std::size_t lightmap_index, std::string_view source_name,
    std::array<std::uint32_t, 2> size, const LightmapRgbStats &stats) {
  std::ostringstream message;
  message << source_uri << ": " << prefix
          << " raw_lighting_bytes=" << raw_lighting_bytes
          << " imported_lightmap_count=" << imported_lightmap_count
          << " lightmap_index=" << lightmap_index
          << " source=" << source_name
          << " size=" << size[0] << 'x' << size[1]
          << " texels=" << stats.texel_count
          << " min_rgb=(" << static_cast<int>(stats.min_rgb[0]) << ','
          << static_cast<int>(stats.min_rgb[1]) << ','
          << static_cast<int>(stats.min_rgb[2]) << ')'
          << " max_rgb=(" << static_cast<int>(stats.max_rgb[0]) << ','
          << static_cast<int>(stats.max_rgb[1]) << ','
          << static_cast<int>(stats.max_rgb[2]) << ')'
          << " average_rgb=(" << stats.average_rgb[0] << ','
          << stats.average_rgb[1] << ',' << stats.average_rgb[2] << ')'
          << " all_black=" << (stats.all_black ? "true" : "false");
  return message.str();
}

[[nodiscard]] std::string lightmap_summary_message(
    const std::string &source_uri, std::size_t raw_lighting_bytes,
    std::size_t imported_lightmap_count, std::size_t materials_with_lightmaps) {
  std::ostringstream message;
  message << source_uri << ": BSP lightmap summary"
          << " raw_lighting_bytes=" << raw_lighting_bytes
          << " imported_lightmap_count=" << imported_lightmap_count
          << " materials_with_lightmaps=" << materials_with_lightmaps;
  return message.str();
}

[[nodiscard]] std::string material_lightmap_binding_message(
    const std::string &source_uri, std::size_t material_index,
    const stellar::assets::LevelSurfaceMaterial &material,
    const stellar::assets::LevelLightmap &lightmap) {
  std::ostringstream message;
  message << source_uri << ": BSP material lightmap binding"
          << " material_index=" << material_index
          << " material=" << material.name
          << " source=" << material.source_name
          << " lightmap_index=" << *material.lightmap_index
          << " lightmap_source=" << lightmap.source_name
          << " lightmap_size=" << lightmap.size[0] << 'x' << lightmap.size[1]
          << " lightstyle=" << static_cast<int>(lightmap.style);
  return message.str();
}

[[nodiscard]] bool debug_textures_enabled() noexcept {
  const char *value = std::getenv("STELLAR_DEBUG_TEXTURES");
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  const std::string_view text(value);
  return text == "1" || text == "true" || text == "TRUE" || text == "on" ||
         text == "ON";
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
  return "unknown";
}

void add_texture_diagnostics(
    const stellar::assets::LevelGeometryAsset &geometry, const BspMap &map,
    const std::vector<std::optional<std::size_t>> &face_to_surface,
    const std::string &source_uri, ImportReport *report) {
  if (report == nullptr || !debug_textures_enabled()) {
    return;
  }

  std::ostringstream summary;
  summary << source_uri << ": BSP texture summary materials="
          << geometry.materials.size() << " textures=" << geometry.textures.size()
          << " images=" << geometry.images.size()
          << " samplers=" << geometry.samplers.size()
          << " surfaces=" << geometry.surfaces.size();
  add_info(report, DiagnosticCode::kTextureStats, source_uri, summary.str(),
           static_cast<std::size_t>(LumpIndex::kTextures));

  for (std::size_t material_index = 0;
       material_index < geometry.materials.size(); ++material_index) {
    const auto &material = geometry.materials[material_index];
    std::ostringstream message;
    message << source_uri << ": BSP texture material material_index="
            << material_index << " material=" << material.name
            << " source_material=" << material.source_name;
    if (!material.texture_index.has_value() ||
        *material.texture_index >= geometry.textures.size()) {
      message << " texture_index=none texture=none source_kind=none";
    } else {
      const auto &texture = geometry.textures[*material.texture_index];
      message << " texture_index=" << *material.texture_index
              << " texture=" << texture.name;
      if (texture.image_index.has_value() &&
          *texture.image_index < geometry.images.size()) {
        const auto &image = geometry.images[*texture.image_index];
        message << " image_index=" << *texture.image_index
                << " image=" << image.name
                << " image_source_uri=" << image.source_uri
                << " source_kind=" << texture_source_kind(image.source_uri)
                << " dimensions=" << image.width << 'x' << image.height
                << " format=" << image_format_name(image.format);
      } else {
        message << " image_index=none image=none source_kind=none";
      }
      stellar::assets::SamplerAsset sampler;
      if (texture.sampler_index.has_value() &&
          *texture.sampler_index < geometry.samplers.size()) {
        sampler = geometry.samplers[*texture.sampler_index];
        message << " sampler_index=" << *texture.sampler_index;
      } else {
        message << " sampler_index=none";
      }
      message << " sampler=" << (sampler.name.empty() ? "<default>" : sampler.name)
              << " mag=" << texture_filter_name(sampler.mag_filter)
              << " min=" << texture_filter_name(sampler.min_filter)
              << " wrap_s=" << texture_wrap_name(sampler.wrap_s)
              << " wrap_t=" << texture_wrap_name(sampler.wrap_t);
    }
    add_info(report, DiagnosticCode::kTextureStats, source_uri, message.str(),
             static_cast<std::size_t>(LumpIndex::kTextures));
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
    std::ostringstream message;
    message << source_uri << ": BSP texture surface surface_index="
            << surface_index << " surface=" << surface.name
            << " mesh_index=" << surface.mesh_index
            << " primitive_index=" << surface.primitive_index;
    if (surface.material_index.has_value()) {
      message << " material_index=" << *surface.material_index;
      if (*surface.material_index < geometry.materials.size()) {
        message << " material=" << geometry.materials[*surface.material_index].name;
      }
    } else {
      message << " material_index=none material=none";
    }
    message << " uv0_min=(" << uv_min[0] << ',' << uv_min[1] << ')'
            << " uv0_max=(" << uv_max[0] << ',' << uv_max[1] << ')';
    auto face_for_surface = std::find(face_to_surface.begin(), face_to_surface.end(),
                                      std::optional<std::size_t>{surface_index});
    if (face_for_surface != face_to_surface.end()) {
      const std::size_t face_index = static_cast<std::size_t>(
          std::distance(face_to_surface.begin(), face_for_surface));
      if (face_index < map.faces.size()) {
        const Face &face = map.faces[face_index];
        message << " face_index=" << face_index
                << " texture_name=" << texture_name_for(map, face);
        if (face.texinfo < map.texinfos.size()) {
          const Texinfo &info = map.texinfos[face.texinfo];
          message << " texinfo_index=" << face.texinfo << " s_vector=("
                  << info.s[0] << ',' << info.s[1] << ',' << info.s[2] << ','
                  << info.s[3] << ") t_vector=(" << info.t[0] << ','
                  << info.t[1] << ',' << info.t[2] << ',' << info.t[3]
                  << ')';
        } else {
          message << " texinfo_index=invalid s_vector=none t_vector=none";
        }
      }
    }
    add_info(report, DiagnosticCode::kTextureStats, source_uri, message.str(),
             static_cast<std::size_t>(LumpIndex::kFaces), surface_index);
  }
}

void add_lightmap_diagnostics(const stellar::assets::LevelGeometryAsset &geometry,
                              const std::string &source_uri,
                              ImportReport *report) {
  if (report == nullptr) {
    return;
  }

  const std::size_t raw_lighting_bytes = geometry.raw_lighting.size();
  const std::size_t imported_lightmap_count = geometry.lightmaps.size();
  std::size_t materials_with_lightmaps = 0U;
  for (const auto &material : geometry.materials) {
    if (material.lightmap_index.has_value() &&
        *material.lightmap_index < geometry.lightmaps.size()) {
      ++materials_with_lightmaps;
    }
  }

  add_info(report, DiagnosticCode::kLightmapStats, source_uri,
           lightmap_summary_message(source_uri, raw_lighting_bytes,
                                    imported_lightmap_count,
                                    materials_with_lightmaps),
           static_cast<std::size_t>(LumpIndex::kLighting));
  if (geometry.lightmaps.empty()) {
    return;
  }

  LightmapRgbStats aggregate{};
  aggregate.texel_count = 0U;
  std::array<std::uint64_t, 3> aggregate_sums{};

  for (std::size_t index = 0; index < geometry.lightmaps.size(); ++index) {
    const auto &lightmap = geometry.lightmaps[index];
    if (lightmap.image_index >= geometry.images.size()) {
      continue;
    }
    const auto &image = geometry.images[lightmap.image_index];
    const LightmapRgbStats stats = rgb_stats_for_pixels(image.pixels);
    const std::string message = lightmap_stats_message(
        source_uri,
        stats.all_black ? "BSP imported all-black lightmap"
                        : "BSP imported lightmap stats",
        raw_lighting_bytes, imported_lightmap_count, index, lightmap.source_name,
        lightmap.size, stats);
    if (stats.all_black) {
      add_warning(report, DiagnosticCode::kAllBlackLightmap, source_uri, message,
                  static_cast<std::size_t>(LumpIndex::kLighting));
    } else {
      add_info(report, DiagnosticCode::kLightmapStats, source_uri, message,
               static_cast<std::size_t>(LumpIndex::kLighting));
    }

    aggregate.texel_count += stats.texel_count;
    aggregate.all_black = aggregate.all_black && stats.all_black;
    for (std::size_t channel = 0; channel < 3U; ++channel) {
      aggregate.min_rgb[channel] =
          std::min(aggregate.min_rgb[channel], stats.min_rgb[channel]);
      aggregate.max_rgb[channel] =
          std::max(aggregate.max_rgb[channel], stats.max_rgb[channel]);
      aggregate_sums[channel] += static_cast<std::uint64_t>(
          stats.average_rgb[channel] * static_cast<double>(stats.texel_count));
    }
  }

  for (std::size_t material_index = 0; material_index < geometry.materials.size();
       ++material_index) {
    const auto &material = geometry.materials[material_index];
    if (!material.lightmap_index.has_value() ||
        *material.lightmap_index >= geometry.lightmaps.size()) {
      continue;
    }
    add_info(report, DiagnosticCode::kLightmapStats, source_uri,
             material_lightmap_binding_message(
                 source_uri, material_index, material,
                 geometry.lightmaps[*material.lightmap_index]),
             static_cast<std::size_t>(LumpIndex::kLighting));
  }

  if (aggregate.texel_count == 0U) {
    aggregate.min_rgb = {0U, 0U, 0U};
  } else {
    for (std::size_t channel = 0; channel < 3U; ++channel) {
      aggregate.average_rgb[channel] = static_cast<double>(aggregate_sums[channel]) /
                                       static_cast<double>(aggregate.texel_count);
    }
  }
  add_info(report, DiagnosticCode::kLightmapStats, source_uri,
           lightmap_stats_message(source_uri, "BSP imported aggregate lightmap stats",
                                  raw_lighting_bytes, imported_lightmap_count, 0U,
                                  "aggregate", {0U, 0U}, aggregate),
            static_cast<std::size_t>(LumpIndex::kLighting));
}

void apply_lighting_policy(stellar::assets::LevelAsset &level, const BspMap &map,
                           const std::vector<Entity> &entities,
                           ImportReport *report) {
  const Entity *worldspawn = nullptr;
  for (const Entity &entity : entities) {
    if (string_or(entity, "classname") == "worldspawn") {
      worldspawn = &entity;
      break;
    }
  }

  std::string mode_source = "inferred";
  bool malformed_explicit_mode = false;
  if (worldspawn != nullptr) {
    if (const std::string *mode_text = lighting_mode_key_for(*worldspawn)) {
      if (auto parsed = parse_lighting_mode_value(*mode_text)) {
        level.lighting.mode = *parsed;
        mode_source = "explicit";
      } else {
        malformed_explicit_mode = true;
      }
    }
  }
  if (mode_source == "inferred") {
    level.lighting.mode = infer_lighting_mode(map, level.geometry);
  }
  if (malformed_explicit_mode) {
    add_warning(report, DiagnosticCode::kUnsupportedEntityKey, level.source_uri,
                level.source_uri +
                    ": BSP worldspawn has malformed lighting mode; inferred mode was used",
                static_cast<std::size_t>(LumpIndex::kEntities));
  }

  std::size_t global_entity_count = 0U;
  std::size_t enabled_global_entity_count = 0U;
  std::optional<stellar::assets::LevelGlobalLight> chosen_global_entity;
  std::optional<std::size_t> chosen_global_entity_index;
  for (std::size_t entity_index = 0; entity_index < entities.size(); ++entity_index) {
    const Entity &entity = entities[entity_index];
    if (!is_global_light_class(string_or(entity, "classname"))) {
      continue;
    }
    ++global_entity_count;
    const std::string *color_text = value_for_alias(entity, "_stellar_color", "color");
    const std::string *intensity_text =
        value_for_alias(entity, "_stellar_intensity", "intensity");
    const std::string *enabled_text = value_for(entity, "_stellar_enabled");
    auto parsed = parse_global_light_components(color_text, intensity_text,
                                                enabled_text, true, 0.0F);
    if (!parsed.has_value()) {
      add_entity_warning(report, DiagnosticCode::kUnsupportedEntityKey,
                         level.source_uri, entity_index,
                         level.source_uri +
                             ": BSP global light entity has malformed lighting keys");
      continue;
    }
    if (parsed->enabled) {
      ++enabled_global_entity_count;
      if (!chosen_global_entity.has_value()) {
        chosen_global_entity = *parsed;
        chosen_global_entity_index = entity_index;
      }
    }
  }

  if (enabled_global_entity_count > 1U) {
    add_warning(report, DiagnosticCode::kUnsupportedEntityKey, level.source_uri,
                level.source_uri +
                    ": BSP has multiple enabled global light entities; first enabled entity was used",
                static_cast<std::size_t>(LumpIndex::kEntities));
  }

  if (chosen_global_entity.has_value()) {
    level.lighting.global_ambient = *chosen_global_entity;
  } else if (worldspawn != nullptr) {
    if (auto tuple = parse_global_light_tuple(value_for(*worldspawn, "_stellar_global_light"))) {
      level.lighting.global_ambient = *tuple;
    } else if (value_for(*worldspawn, "_stellar_global_light") != nullptr) {
      add_warning(report, DiagnosticCode::kUnsupportedEntityKey, level.source_uri,
                  level.source_uri +
                      ": BSP worldspawn has malformed _stellar_global_light tuple",
                  static_cast<std::size_t>(LumpIndex::kEntities));
    } else {
      const std::string *color_text = value_for(*worldspawn, "_stellar_global_color");
      const std::string *intensity_text = value_for(*worldspawn, "_stellar_global_intensity");
      if (color_text != nullptr || intensity_text != nullptr) {
        auto parsed = parse_global_light_components(color_text, intensity_text,
                                                    nullptr, true, 0.0F);
        if (parsed.has_value()) {
          level.lighting.global_ambient = *parsed;
        } else {
          add_warning(report, DiagnosticCode::kUnsupportedEntityKey,
                      level.source_uri,
                      level.source_uri +
                          ": BSP worldspawn has malformed global light keys",
                      static_cast<std::size_t>(LumpIndex::kEntities));
        }
      }
    }
  }

  std::ostringstream mode_message;
  mode_message << level.source_uri << ": BSP lighting policy source=" << mode_source
               << " mode=" << lighting_mode_name(level.lighting.mode)
               << " raw_lighting_bytes=" << level.geometry.raw_lighting.size()
               << " face_lightmap_count=" << face_lightmap_reference_count(map)
               << " material_lightmap_count=" << material_lightmap_count(level.geometry);
  add_info(report, DiagnosticCode::kLightmapStats, level.source_uri,
           mode_message.str(), static_cast<std::size_t>(LumpIndex::kLighting));

  std::ostringstream global_message;
  global_message << level.source_uri << ": BSP global ambient entity_count="
                 << global_entity_count
                 << " enabled_entity_count=" << enabled_global_entity_count
                 << " chosen_entity_index=";
  if (chosen_global_entity_index.has_value()) {
    global_message << *chosen_global_entity_index;
  } else {
    global_message << "none";
  }
  global_message << " enabled="
                 << (level.lighting.global_ambient.enabled ? "true" : "false")
                 << " color=(" << level.lighting.global_ambient.color[0] << ','
                 << level.lighting.global_ambient.color[1] << ','
                 << level.lighting.global_ambient.color[2] << ')'
                 << " intensity=" << level.lighting.global_ambient.intensity;
  add_info(report, DiagnosticCode::kLightmapStats, level.source_uri,
           global_message.str(), static_cast<std::size_t>(LumpIndex::kEntities));
}

[[nodiscard]] std::array<std::uint8_t, 4> active_light_styles(const Face &face) noexcept {
  std::array<std::uint8_t, 4> styles{255U, 255U, 255U, 255U};
  std::size_t count = 0;
  for (const auto style : face.styles) {
    if (style == 255U) {
      break;
    }
    styles[count++] = style;
  }
  if (count == 0) {
    styles[0] = 0U;
  }
  return styles;
}

[[nodiscard]] LightmapBuildResult build_lightmap_for_face(
    stellar::assets::LevelGeometryAsset &geometry, const BspMap &map, const Face &face,
    const std::vector<Vec3> &polygon, std::size_t face_index, const std::string &source_uri,
    ImportReport *report) {
  if (!map.has_lighting || face.light_offset < 0) {
    return {};
  }
  if (face.texinfo >= map.texinfos.size()) {
    add_warning(report, DiagnosticCode::kInvalidLightingData, source_uri,
                source_uri + ": BSP face lightmap references invalid texinfo",
                static_cast<std::size_t>(LumpIndex::kLighting), face_index);
    return {};
  }
  const Texinfo &info = map.texinfos[face.texinfo];
  float min_s = std::numeric_limits<float>::max();
  float min_t = std::numeric_limits<float>::max();
  float max_s = std::numeric_limits<float>::lowest();
  float max_t = std::numeric_limits<float>::lowest();
  for (const Vec3 &point : polygon) {
    const float s = point[0] * info.s[0] + point[1] * info.s[1] + point[2] * info.s[2] + info.s[3];
    const float t = point[0] * info.t[0] + point[1] * info.t[1] + point[2] * info.t[2] + info.t[3];
    min_s = std::min(min_s, s);
    min_t = std::min(min_t, t);
    max_s = std::max(max_s, s);
    max_t = std::max(max_t, t);
  }
  const auto min_texel_s = static_cast<int>(std::floor(min_s / 16.0F));
  const auto min_texel_t = static_cast<int>(std::floor(min_t / 16.0F));
  const auto max_texel_s = static_cast<int>(std::ceil(max_s / 16.0F));
  const auto max_texel_t = static_cast<int>(std::ceil(max_t / 16.0F));
  const std::uint32_t width = static_cast<std::uint32_t>(std::max(1, max_texel_s - min_texel_s + 1));
  const std::uint32_t height = static_cast<std::uint32_t>(std::max(1, max_texel_t - min_texel_t + 1));
  const std::uint64_t byte_count = static_cast<std::uint64_t>(width) * height * 3ULL;
  const std::uint64_t offset = static_cast<std::uint64_t>(face.light_offset);
  if (offset + byte_count > geometry.raw_lighting.size()) {
    add_warning(report, DiagnosticCode::kInvalidLightingData, source_uri,
                source_uri + ": BSP face lightmap offset is outside the lighting lump",
                static_cast<std::size_t>(LumpIndex::kLighting), face_index);
    return {};
  }

  stellar::assets::ImageAsset image{};
  image.name = "lightmap_face_" + std::to_string(face_index);
  image.width = width;
  image.height = height;
  image.format = stellar::assets::ImageFormat::kR8G8B8;
  image.source_uri = source_uri + "#lightmap/face_" + std::to_string(face_index);
  image.pixels.reserve(static_cast<std::size_t>(byte_count));
  for (std::uint64_t i = 0; i < byte_count; ++i) {
    image.pixels.push_back(std::to_integer<std::uint8_t>(geometry.raw_lighting[static_cast<std::size_t>(offset + i)]));
  }
  const std::size_t image_index = geometry.images.size();
  geometry.images.push_back(std::move(image));

  const auto styles = active_light_styles(face);
  const std::size_t lightmap_index = geometry.lightmaps.size();
  geometry.lightmaps.push_back(stellar::assets::LevelLightmap{
      .image_index = image_index,
      .size = {width, height},
      .style = styles[0],
      .source_name = "face_" + std::to_string(face_index)});
  return LightmapBuildResult{.lightmap_index = lightmap_index,
                             .uv_min = std::array<float, 2>{static_cast<float>(min_texel_s * 16), static_cast<float>(min_texel_t * 16)},
                             .uv_extent = std::array<float, 2>{std::max(1.0F, static_cast<float>(width - 1) * 16.0F), std::max(1.0F, static_cast<float>(height - 1) * 16.0F)}};
}

[[nodiscard]] std::optional<stellar::assets::WorldScriptBinding>
script_binding_for(const Entity &entity) {
  const std::string *script =
      value_for_alias(entity, "stellar.script", "_stellar_script");
  if (script == nullptr) {
    return std::nullopt;
  }
  const std::string *table =
      value_for_alias(entity, "stellar.table", "_stellar_table");
  return stellar::assets::WorldScriptBinding{
      .script_id = *script,
      .table_name = table == nullptr ? std::string{} : *table};
}

[[nodiscard]] bool script_path_escape(std::string_view script) noexcept {
  if (script.empty()) {
    return false;
  }
  if (script.front() == '/' || script.front() == '\\') {
    return true;
  }
  if (script.size() >= 2 &&
      std::isalpha(static_cast<unsigned char>(script[0])) != 0 &&
      script[1] == ':') {
    return true;
  }
  std::size_t begin = 0;
  for (std::size_t i = 0; i <= script.size(); ++i) {
    if (i == script.size() || script[i] == '/' || script[i] == '\\') {
      if (script.substr(begin, i - begin) == "..") {
        return true;
      }
      begin = i + 1;
    }
  }
  return false;
}

void copy_properties(stellar::assets::WorldMarker &marker,
                     const Entity &entity) {
  for (const auto &pair : entity.pairs) {
    marker.properties.push_back(stellar::assets::WorldEntityProperty{
        .key = pair.key, .value = pair.value});
  }
}

void warn_canonical_name_aliases(const Entity &entity, ImportReport *report,
                                 const std::string &source_uri,
                                 std::size_t entity_index) {
  if (report == nullptr) {
    return;
  }
  const auto selected = canonical_entity_name_for(entity);
  if (!selected.has_value()) {
    return;
  }
  for (std::string_view key : kEntityNameKeys) {
    const std::string *value = value_for(entity, key);
    if (value == nullptr || value->empty() || key == selected->key) {
      continue;
    }
    if (*value != selected->value) {
      add_entity_warning(report, DiagnosticCode::kUnsupportedEntityKey, source_uri,
                         entity_index,
                         source_uri + ": BSP entity name alias '" + std::string(key) +
                             "' conflicts with canonical '" +
                             std::string(selected->key) + "'; using '" +
                             selected->value + "'");
    }
  }
  if (selected->key == "name") {
    add_entity_warning(report, DiagnosticCode::kUnsupportedEntityKey, source_uri,
                       entity_index,
                       source_uri +
                           ": BSP entity uses legacy name without targetname; "
                           "targetname is the canonical targetable identity");
  }
}

[[nodiscard]] bool is_brush_class(std::string_view classname) noexcept {
  return classname.starts_with("func_") || classname.starts_with("trigger_");
}

[[nodiscard]] std::string collision_name_for(const Entity &entity,
                                              int model_index) {
  if (const std::string *classname = value_for(entity, "classname");
      classname != nullptr && !classname->empty()) {
    return canonical_entity_name_or(entity,
                                    *classname + "_" + std::to_string(model_index));
  }
  return canonical_entity_name_or(entity, "bsp_model_" + std::to_string(model_index));
}

void add_visibility_warning(ImportReport *report, const std::string &source_uri,
                            std::string message) {
  if (report == nullptr) {
    return;
  }
  report->diagnostics.push_back(Diagnostic{
      .severity = DiagnosticSeverity::kWarning,
      .code = DiagnosticCode::kInvalidVisibilityData,
      .message = std::move(message),
      .source_uri = source_uri,
      .lump_index = static_cast<std::size_t>(LumpIndex::kVisibility)});
}

} // namespace

std::expected<stellar::assets::LevelAsset, stellar::platform::Error>
build_level_asset(BspMap map, std::vector<Entity> entities,
                  std::string source_uri, const LoadOptions &options) {
  return build_level_asset(std::move(map), std::move(entities),
                           std::move(source_uri), options, nullptr);
}

std::expected<stellar::assets::LevelAsset, stellar::platform::Error>
build_level_asset(BspMap map, std::vector<Entity> entities,
                  std::string source_uri, const LoadOptions &options,
                  ImportReport *report) {
  stellar::assets::LevelAsset level{};
  level.source_uri = std::move(source_uri);
  level.visibility.available = map.has_visibility;
  level.visibility.compressed_pvs = std::move(map.visibility_bytes);
  level.geometry.raw_lighting = std::move(map.lighting_bytes);
  std::map<std::string, std::size_t> material_indices;
  std::map<std::string, std::size_t> texture_indices;
  std::unordered_map<std::string, WadTextureAsset> wad_textures;
  if (!entities.empty()) {
    if (const std::string *wad_key = value_for(entities.front(), "wad");
        wad_key != nullptr && !wad_key->empty()) {
      WadResolveResult wad_result = resolve_wad_textures(
          *wad_key, make_wad_resolve_context(level.source_uri));
      wad_textures = std::move(wad_result.textures);
      for (std::string &message : wad_result.diagnostics) {
        add_warning(report, DiagnosticCode::kMissingTexture, level.source_uri,
                    level.source_uri + ": " + message,
                    static_cast<std::size_t>(LumpIndex::kTextures));
      }
    }
  }

  for (std::size_t entity_index = 0; entity_index < entities.size(); ++entity_index) {
    warn_canonical_name_aliases(entities[entity_index], report, level.source_uri,
                                entity_index);
  }

  stellar::assets::LevelCollisionAsset collision{};
  collision.bounds_min = {std::numeric_limits<float>::max(),
                          std::numeric_limits<float>::max(),
                          std::numeric_limits<float>::max()};
  collision.bounds_max = {std::numeric_limits<float>::lowest(),
                          std::numeric_limits<float>::lowest(),
                          std::numeric_limits<float>::lowest()};

  std::unordered_map<int, const Entity *> model_entities;
  for (const auto &entity : entities) {
    if (auto index = parse_model_index(value_for(entity, "model"))) {
      model_entities.emplace(*index, &entity);
    }
  }

  std::vector<std::optional<std::size_t>> face_to_surface(map.faces.size());
  const std::size_t model_count = map.models.empty() ? 1 : map.models.size();
  for (std::size_t model_index = 0; model_index < model_count; ++model_index) {
    const Model model =
        map.models.empty()
            ? Model{.first_face = 0,
                    .face_count = static_cast<std::int32_t>(map.faces.size())}
            : map.models[model_index];
    stellar::assets::MeshAsset mesh{};
    mesh.name = model_index == 0 ? "worldspawn"
                                 : "bsp_model_" + std::to_string(model_index);
    mesh.source_uri = level.source_uri;
    const std::size_t output_mesh_index = level.geometry.meshes.size();
    stellar::assets::CollisionMesh collision_mesh{};
    collision_mesh.name = model_index == 0
                              ? "worldspawn"
                              : "bsp_model_" + std::to_string(model_index);
    if (auto found = model_entities.find(static_cast<int>(model_index));
        found != model_entities.end()) {
      collision_mesh.name =
          collision_name_for(*found->second, static_cast<int>(model_index));
    }
    const Entity *owning_entity = nullptr;
    if (auto found = model_entities.find(static_cast<int>(model_index)); found != model_entities.end()) {
      owning_entity = found->second;
    }
    const std::string owning_class = owning_entity == nullptr
                                         ? std::string{}
                                         : string_or(*owning_entity, "classname");
    const bool build_model_collision = model_index == 0 ||
                                       (owning_class != "func_illusionary" &&
                                        !owning_class.starts_with("trigger_"));
    collision_mesh.bounds_min = {std::numeric_limits<float>::max(),
                                 std::numeric_limits<float>::max(),
                                 std::numeric_limits<float>::max()};
    collision_mesh.bounds_max = {std::numeric_limits<float>::lowest(),
                                 std::numeric_limits<float>::lowest(),
                                 std::numeric_limits<float>::lowest()};

    for (std::int32_t local_face = 0; local_face < model.face_count;
         ++local_face) {
      const std::int64_t face_index =
          static_cast<std::int64_t>(model.first_face) + local_face;
      if (face_index < 0 ||
          static_cast<std::size_t>(face_index) >= map.faces.size()) {
        return std::unexpected(stellar::platform::Error(
            level.source_uri +
            ": BSP model references a face outside the face lump"));
      }
      const Face &face = map.faces[static_cast<std::size_t>(face_index)];
      const std::vector<Vec3> polygon = face_vertices(map, face);
      if (polygon.size() < 3) {
        add_warning(report,
                    face.edge_count < 3 ? DiagnosticCode::kDegenerateFacePolygon
                                        : DiagnosticCode::kInvalidFaceReference,
                    level.source_uri,
                    level.source_uri +
                        ": BSP face cannot form a valid polygon from its references",
                    static_cast<std::size_t>(LumpIndex::kFaces),
                    static_cast<std::size_t>(face_index));
        continue;
      }
      const std::string texture = texture_name_for(map, face);
      const TextureBuildResult texture_asset = texture_asset_index(
          level.geometry, texture_indices, wad_textures, texture,
          texture_for_face(map, face), level.source_uri, report);
      const LightmapBuildResult lightmap = build_lightmap_for_face(
          level.geometry, map, face, polygon, static_cast<std::size_t>(face_index),
          level.source_uri, report);
      const std::size_t mat = material_index(level.geometry, material_indices,
                                             texture, texture_asset.texture_index,
                                             lightmap.lightmap_index);

      stellar::assets::MeshPrimitive primitive{};
      primitive.material_index = mat;
      primitive.bounds_min = {std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max()};
      primitive.bounds_max = {std::numeric_limits<float>::lowest(),
                              std::numeric_limits<float>::lowest(),
                              std::numeric_limits<float>::lowest()};
      const Vec3 normal = normalize(cross(subtract(polygon[1], polygon[0]),
                                          subtract(polygon[2], polygon[0])));
      for (const Vec3 &point : polygon) {
        stellar::assets::StaticVertex vertex{};
        vertex.position = point;
        vertex.normal = normal;
        if (face.texinfo < map.texinfos.size()) {
          const Texinfo &info = map.texinfos[face.texinfo];
          const float s = point[0] * info.s[0] + point[1] * info.s[1] +
                          point[2] * info.s[2] + info.s[3];
          const float t = point[0] * info.t[0] + point[1] * info.t[1] +
                          point[2] * info.t[2] + info.t[3];
          if (texture_asset.texel_size.has_value() &&
              (*texture_asset.texel_size)[0] > 0U &&
              (*texture_asset.texel_size)[1] > 0U) {
            vertex.uv0 = {s / static_cast<float>((*texture_asset.texel_size)[0]),
                          t / static_cast<float>((*texture_asset.texel_size)[1])};
          } else {
            vertex.uv0 = {s, t};
          }
          if (lightmap.uv_min.has_value() && lightmap.uv_extent.has_value()) {
            vertex.uv1 = {(s - (*lightmap.uv_min)[0]) / (*lightmap.uv_extent)[0],
                          (t - (*lightmap.uv_min)[1]) / (*lightmap.uv_extent)[1]};
          }
        }
        primitive.vertices.push_back(vertex);
        include_point(primitive.bounds_min, primitive.bounds_max, point);
        include_point(collision_mesh.bounds_min, collision_mesh.bounds_max,
                      point);
        include_point(collision.bounds_min, collision.bounds_max, point);
      }
      for (std::uint32_t i = 1; i + 1 < polygon.size(); ++i) {
        primitive.indices.push_back(0);
        primitive.indices.push_back(i);
        primitive.indices.push_back(i + 1);
        if (options.build_triangle_collision_from_faces && build_model_collision) {
          collision_mesh.triangles.push_back(
              stellar::assets::CollisionTriangle{.a = polygon[0],
                                                 .b = polygon[i],
                                                 .c = polygon[i + 1],
                                                 .normal = normal});
        }
      }
      const std::size_t primitive_index = mesh.primitives.size();
      mesh.primitives.push_back(std::move(primitive));
      const std::size_t surface_index = level.geometry.surfaces.size();
      face_to_surface[static_cast<std::size_t>(face_index)] = surface_index;
      level.geometry.surfaces.push_back(stellar::assets::LevelSurface{
          .name = "face_" + std::to_string(face_index),
          .mesh_index = output_mesh_index,
          .primitive_index = primitive_index,
          .material_index = mat,
          .bounds_min = mesh.primitives.back().bounds_min,
          .bounds_max = mesh.primitives.back().bounds_max,
          .source_flags =
              face.texinfo < map.texinfos.size()
                  ? static_cast<std::uint32_t>(map.texinfos[face.texinfo].flags)
                  : 0U,
          .brush_model_index = static_cast<std::uint32_t>(model_index)});
    }
    if (model_index > 0) {
      if (auto found = model_entities.find(static_cast<int>(model_index));
          found != model_entities.end()) {
        const Entity &entity = *found->second;
        stellar::assets::LevelBrushEntity brush{};
        brush.classname = string_or(entity, "classname", "func_unknown");
        brush.targetname = canonical_entity_name_or(entity, {});
        brush.target = string_or(entity, "target");
        brush.model = string_or(entity, "model");
        brush.name = brush.targetname.empty() ? collision_mesh.name : brush.targetname;
        brush.bsp_model_index = static_cast<std::uint32_t>(model_index);
        brush.mesh_index = output_mesh_index;
        brush.collision_mesh_name = collision_mesh.name;
        brush.origin = model.origin;
        brush.bounds_min = model.mins;
        brush.bounds_max = model.maxs;
        for (const auto &pair : entity.pairs) {
          brush.properties.push_back(stellar::assets::WorldEntityProperty{.key = pair.key,
                                                                          .value = pair.value});
        }
        level.geometry.brush_entities.push_back(std::move(brush));
      }
    }
    if (!collision_mesh.triangles.empty()) {
      collision.meshes.push_back(std::move(collision_mesh));
    }
    if (!mesh.primitives.empty()) {
      level.geometry.meshes.push_back(std::move(mesh));
    }
  }
  if (!collision.meshes.empty()) {
    level.level_collision = std::move(collision);
  }

  level.visibility.cluster_count = map.leaves.size();
  for (const Leaf &leaf : map.leaves) {
    stellar::assets::LevelLeaf output_leaf{};
    output_leaf.contents = leaf.contents;
    for (std::size_t i = 0; i < 3; ++i) {
      output_leaf.bounds_min[i] = static_cast<float>(leaf.mins[i]);
      output_leaf.bounds_max[i] = static_cast<float>(leaf.maxs[i]);
    }
    if (leaf.visibility_offset >= 0) {
      const auto offset = static_cast<std::size_t>(leaf.visibility_offset);
      if (offset < level.visibility.compressed_pvs.size()) {
        output_leaf.compressed_pvs_offset = offset;
      } else if (map.has_visibility) {
        add_visibility_warning(report, level.source_uri,
                               level.source_uri +
                                   ": BSP leaf visibility offset is outside the visibility lump");
        level.visibility.available = false;
      }
    }
    for (std::uint16_t i = 0; i < leaf.marksurface_count; ++i) {
      const std::size_t marksurface_index =
          static_cast<std::size_t>(leaf.first_marksurface) + i;
      if (marksurface_index >= map.marksurfaces.size()) {
        add_visibility_warning(report, level.source_uri,
                               level.source_uri +
                                   ": BSP leaf marksurface range is outside the marksurface lump");
        continue;
      }
      const std::size_t face_index = map.marksurfaces[marksurface_index];
      if (face_index < face_to_surface.size() &&
          face_to_surface[face_index].has_value()) {
        output_leaf.surface_indices.push_back(*face_to_surface[face_index]);
      } else {
        add_visibility_warning(report, level.source_uri,
                               level.source_uri +
                                   ": BSP marksurface references a skipped or invalid face");
      }
    }
    level.visibility.leaves.push_back(std::move(output_leaf));
  }
  if (level.visibility.available) {
    for (const stellar::assets::LevelLeaf &leaf : level.visibility.leaves) {
      if (!leaf.compressed_pvs_offset.has_value()) {
        continue;
      }
      if (!stellar::assets::detail::decompress_level_pvs_bits(
              level.visibility, *leaf.compressed_pvs_offset,
              level.visibility.leaves.size())) {
        add_visibility_warning(report, level.source_uri,
                               level.source_uri +
                                   ": BSP visibility PVS row is truncated or malformed");
        level.visibility.available = false;
        break;
      }
    }
  }

  bool has_player_spawn = false;
  bool has_worldspawn = false;
  for (std::size_t entity_index = 0; entity_index < entities.size(); ++entity_index) {
    const Entity &entity = entities[entity_index];
    const std::string classname = string_or(entity, "classname", "entity");
    stellar::assets::WorldMarker marker{};
    bool emit = true;
    if (classname == "worldspawn") {
      has_worldspawn = true;
      emit = false;
    } else if (classname == "light" || classname == "light_spot" ||
               classname == "light_environment" || classname == "info_null") {
      emit = false;
    } else if (classname == "info_player_start" ||
                classname == "info_player_deathmatch") {
      has_player_spawn = true;
      marker.type = stellar::assets::WorldMarkerType::kPlayerSpawn;
      marker.name = canonical_entity_name_or(entity, "Player");
    } else if (classname == "info_stellar_spawn") {
      marker.type = stellar::assets::WorldMarkerType::kEntitySpawn;
      marker.name = canonical_entity_name_or(entity, classname);
      marker.archetype = string_or(entity, "archetype");
    } else if (classname == "trigger_multiple" || classname == "trigger_once" ||
               classname == "trigger_stellar" ||
               classname == "trigger_stellar_point" ||
               classname == "trigger_multiple_point" ||
               classname == "trigger_once_point") {
      marker.type = stellar::assets::WorldMarkerType::kTrigger;
      marker.name = canonical_entity_name_or(entity, classname);
    } else if (classname == "env_sprite" || classname == "stellar_sprite" ||
               value_for_alias(entity, "stellar.sprite", "_stellar_sprite") != nullptr) {
      marker.type = stellar::assets::WorldMarkerType::kSprite;
      marker.name = canonical_entity_name_or(entity, classname);
      marker.archetype = string_or(
          entity, "archetype",
          string_or_alias(entity, "stellar.sprite", "_stellar_sprite"));
    } else if (classname == "stellar_object_collider" ||
               classname == "stellar_object_collider_point" ||
               string_or_alias(entity, "stellar.collider", "_stellar_collider") ==
                   "object") {
      marker.type = stellar::assets::WorldMarkerType::kObjectCollider;
      marker.name = canonical_entity_name_or(entity, classname);
      marker.archetype = string_or(entity, "archetype");
    } else if (is_brush_class(classname)) {
      marker.type = stellar::assets::WorldMarkerType::kEntitySpawn;
      marker.name = canonical_entity_name_or(entity, classname);
      marker.archetype = classname;
    } else {
      marker.type = stellar::assets::WorldMarkerType::kEntitySpawn;
      marker.name = canonical_entity_name_or(entity, classname);
      marker.archetype = classname;
    }
    if (!emit) {
      continue;
    }
    const std::string *origin_text = value_for(entity, "origin");
    if (auto origin = parse_vec3(origin_text)) {
      marker.position = *origin;
    } else if (origin_text != nullptr) {
      add_entity_warning(report, DiagnosticCode::kUnsupportedEntityKey,
                         level.source_uri, entity_index,
                         level.source_uri + ": BSP entity has malformed origin vector: " +
                             *origin_text);
    }

    bool has_brush_model_bounds = false;
    if (auto model_index = parse_model_index(value_for(entity, "model"));
        model_index && *model_index >= 0 &&
        static_cast<std::size_t>(*model_index) < map.models.size()) {
      const Model &model = map.models[static_cast<std::size_t>(*model_index)];
      marker.position = {(model.mins[0] + model.maxs[0]) * 0.5F,
                         (model.mins[1] + model.maxs[1]) * 0.5F,
                         (model.mins[2] + model.maxs[2]) * 0.5F};
      marker.scale = {(model.maxs[0] - model.mins[0]) * 0.5F,
                      (model.maxs[1] - model.mins[1]) * 0.5F,
                      (model.maxs[2] - model.mins[2]) * 0.5F};
      has_brush_model_bounds = true;
    }

    if ((marker.type == stellar::assets::WorldMarkerType::kTrigger ||
         marker.type == stellar::assets::WorldMarkerType::kObjectCollider) &&
        !has_brush_model_bounds) {
      const std::string *extents_text =
          value_for_alias(entity, "stellar.extents", "_stellar_extents");
      if (auto extents = parse_vec3(extents_text)) {
        marker.scale = *extents;
      } else if (extents_text != nullptr) {
        add_entity_warning(report, DiagnosticCode::kUnsupportedEntityKey,
                           level.source_uri, entity_index,
                           level.source_uri +
                               ": BSP entity has malformed stellar.extents vector: " +
                               *extents_text);
      }
    }

    if (marker.type == stellar::assets::WorldMarkerType::kSprite) {
      const std::string *size_text = value_for_alias(entity, "stellar.size", "_stellar_size");
      if (auto size = parse_vec2(size_text)) {
        marker.scale = {(*size)[0], (*size)[1], 1.0F};
      } else if (size_text != nullptr) {
        add_entity_warning(report, DiagnosticCode::kUnsupportedEntityKey,
                           level.source_uri, entity_index,
                           level.source_uri +
                               ": BSP sprite has malformed stellar.size vector: " +
                               *size_text);
      }
    }

    if (marker.type == stellar::assets::WorldMarkerType::kTrigger) {
      const std::string *once_text = value_for_alias(entity, "stellar.once", "_stellar_once");
      if (once_text != nullptr && !parse_bool_like(once_text).has_value()) {
        add_entity_warning(report, DiagnosticCode::kUnsupportedEntityKey,
                           level.source_uri, entity_index,
                           level.source_uri +
                               ": BSP trigger has malformed stellar.once boolean: " +
                               *once_text);
      }
    }

    if (marker.type == stellar::assets::WorldMarkerType::kObjectCollider) {
      const std::string *enabled_text =
          value_for_alias(entity, "stellar.enabled", "_stellar_enabled");
      if (enabled_text != nullptr && !parse_bool_like(enabled_text).has_value()) {
        add_entity_warning(report, DiagnosticCode::kUnsupportedEntityKey,
                           level.source_uri, entity_index,
                           level.source_uri +
                               ": BSP object collider has malformed stellar.enabled boolean: " +
                               *enabled_text);
      }
    }
    if (auto binding = script_binding_for(entity)) {
      if (script_path_escape(binding->script_id)) {
        return std::unexpected(stellar::platform::Error(
            level.source_uri + ": BSP entity script binding uses an absolute "
                               "path or parent path escape"));
      }
      if (marker.type == stellar::assets::WorldMarkerType::kSprite) {
        add_entity_warning(report, DiagnosticCode::kUnsupportedEntityKey,
                           level.source_uri, entity_index,
                           level.source_uri +
                               ": BSP sprite script bindings are not supported and were ignored");
      } else {
        marker.script = std::move(binding);
      }
    }
    if (options.preserve_raw_entities) {
      copy_properties(marker, entity);
    }
    level.world_metadata.markers.push_back(std::move(marker));
  }

  if (report != nullptr && !has_worldspawn) {
    report->diagnostics.push_back(Diagnostic{
        .severity = DiagnosticSeverity::kWarning,
        .code = DiagnosticCode::kMissingWorldspawn,
        .message = level.source_uri + ": BSP entities do not include worldspawn",
        .source_uri = level.source_uri});
  }
  if (report != nullptr && !has_player_spawn) {
    report->diagnostics.push_back(Diagnostic{
        .severity = DiagnosticSeverity::kWarning,
        .code = DiagnosticCode::kMissingPlayerSpawn,
        .message = level.source_uri + ": BSP entities do not include a player spawn",
        .source_uri = level.source_uri});
  }

  add_texture_diagnostics(level.geometry, map, face_to_surface, level.source_uri,
                          report);
  apply_lighting_policy(level, map, entities, report);
  add_lightmap_diagnostics(level.geometry, level.source_uri, report);

  return level;
}

} // namespace stellar::import::bsp::detail
