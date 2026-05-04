#include "BspBinary.hpp"
#include "BspAssetDebugNames.hpp"
#include "BspEntitySchema.hpp"
#include "BspGeometryBuilder.hpp"
#include "BspImportDiagnostics.hpp"
#include "BspLightmapBuilder.hpp"
#include "BspTextureResolver.hpp"
#include "MaterialSidecar.hpp"
#include "Wad3Reader.hpp"

#include "stellar/assets/LevelVisibilityQueries.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace stellar::import::bsp::detail {
namespace {

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

[[nodiscard]] bool debug_textures_enabled() noexcept {
  const char *value = std::getenv("STELLAR_DEBUG_TEXTURES");
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  const std::string_view text(value);
  return text == "1" || text == "true" || text == "TRUE" || text == "on" ||
         text == "ON";
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
  MaterialSidecarResolver material_sidecars(level.geometry, options,
                                            level.source_uri, report);
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
      auto resolved_material =
          material_sidecars.resolve(texture, texture_asset.texture_index);
      if (!resolved_material) {
        return std::unexpected(resolved_material.error());
      }
      const std::size_t mat = material_index(level.geometry, material_indices,
                                             texture, texture_asset.texture_index,
                                             lightmap.lightmap_index,
                                             std::move(*resolved_material));

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
      std::optional<std::array<float, 4>> tangent;
      if (face.texinfo < map.texinfos.size()) {
        tangent = tangent_for_texinfo(normal, map.texinfos[face.texinfo]);
      }
      primitive.has_tangents = tangent.has_value();
      for (const Vec3 &point : polygon) {
        stellar::assets::StaticVertex vertex{};
        vertex.position = point;
        vertex.normal = normal;
        if (tangent.has_value()) {
          vertex.tangent = *tangent;
        }
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
