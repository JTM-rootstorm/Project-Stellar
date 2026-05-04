#include "BspLightmapBuilder.hpp"

#include "BspEntitySchema.hpp"
#include "BspImportDiagnostics.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <sstream>
#include <utility>

namespace stellar::import::bsp::detail {
namespace {

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

[[nodiscard]] std::string lightmap_stats_message(
    const std::string &source_uri, std::string_view prefix,
    std::size_t raw_lighting_bytes, std::size_t imported_lightmap_count,
    std::size_t lightmap_index, std::string_view source_name,
    std::array<std::uint32_t, 2> size, const LightmapRgbStats &stats) {
  std::ostringstream message;
  message << source_uri << ": " << prefix
          << " raw_lighting_bytes=" << raw_lighting_bytes
          << " imported_lightmap_count=" << imported_lightmap_count
          << " lightmap_index=" << lightmap_index << " source=" << source_name
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
          << " material_index=" << material_index << " material=" << material.name
          << " source=" << material.source_name
          << " lightmap_index=" << *material.lightmap_index
          << " lightmap_source=" << lightmap.source_name
          << " lightmap_size=" << lightmap.size[0] << 'x' << lightmap.size[1]
          << " lightstyle=" << static_cast<int>(lightmap.style);
  return message.str();
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

} // namespace

std::size_t face_lightmap_reference_count(const BspMap &map) noexcept {
  std::size_t count = 0U;
  for (const Face &face : map.faces) {
    if (face.light_offset >= 0) {
      ++count;
    }
  }
  return count;
}

stellar::assets::LevelLightingMode infer_lighting_mode(
    const BspMap &map, const stellar::assets::LevelGeometryAsset &geometry) noexcept {
  if (!geometry.raw_lighting.empty() || has_face_lightmap_reference(map)) {
    return stellar::assets::LevelLightingMode::kBakedRequired;
  }
  return stellar::assets::LevelLightingMode::kFullbright;
}

std::size_t material_lightmap_count(
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

LightmapRgbStats rgb_stats_for_pixels(std::span<const std::uint8_t> pixels) noexcept {
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

void add_lightmap_diagnostics(const stellar::assets::LevelGeometryAsset &geometry,
                              const std::string &source_uri, ImportReport *report) {
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
                                    imported_lightmap_count, materials_with_lightmaps),
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
        stats.all_black ? "BSP imported all-black lightmap" : "BSP imported lightmap stats",
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
      aggregate.min_rgb[channel] = std::min(aggregate.min_rgb[channel], stats.min_rgb[channel]);
      aggregate.max_rgb[channel] = std::max(aggregate.max_rgb[channel], stats.max_rgb[channel]);
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
                           const std::vector<Entity> &entities, ImportReport *report) {
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
    auto parsed = parse_global_light_components(color_text, intensity_text, enabled_text,
                                                true, 0.0F);
    if (!parsed.has_value()) {
      add_entity_warning(report, DiagnosticCode::kUnsupportedEntityKey, level.source_uri,
                         entity_index,
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
        auto parsed = parse_global_light_components(color_text, intensity_text, nullptr,
                                                    true, 0.0F);
        if (parsed.has_value()) {
          level.lighting.global_ambient = *parsed;
        } else {
          add_warning(report, DiagnosticCode::kUnsupportedEntityKey, level.source_uri,
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

LightmapBuildResult build_lightmap_for_face(
    stellar::assets::LevelGeometryAsset &geometry, const BspMap &map, const Face &face,
    const std::vector<Vec3> &polygon, std::size_t face_index,
    const std::string &source_uri, ImportReport *report) {
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
    const float s = point[0] * info.s[0] + point[1] * info.s[1] +
                    point[2] * info.s[2] + info.s[3];
    const float t = point[0] * info.t[0] + point[1] * info.t[1] +
                    point[2] * info.t[2] + info.t[3];
    min_s = std::min(min_s, s);
    min_t = std::min(min_t, t);
    max_s = std::max(max_s, s);
    max_t = std::max(max_t, t);
  }
  const auto min_texel_s = static_cast<int>(std::floor(min_s / 16.0F));
  const auto min_texel_t = static_cast<int>(std::floor(min_t / 16.0F));
  const auto max_texel_s = static_cast<int>(std::ceil(max_s / 16.0F));
  const auto max_texel_t = static_cast<int>(std::ceil(max_t / 16.0F));
  const std::uint32_t width = static_cast<std::uint32_t>(
      std::max(1, max_texel_s - min_texel_s + 1));
  const std::uint32_t height = static_cast<std::uint32_t>(
      std::max(1, max_texel_t - min_texel_t + 1));
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
    image.pixels.push_back(std::to_integer<std::uint8_t>(
        geometry.raw_lighting[static_cast<std::size_t>(offset + i)]));
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
  return LightmapBuildResult{
      .lightmap_index = lightmap_index,
      .uv_min = std::array<float, 2>{static_cast<float>(min_texel_s * 16),
                                     static_cast<float>(min_texel_t * 16)},
      .uv_extent = std::array<float, 2>{
          std::max(1.0F, static_cast<float>(width - 1) * 16.0F),
          std::max(1.0F, static_cast<float>(height - 1) * 16.0F)}};
}

} // namespace stellar::import::bsp::detail
