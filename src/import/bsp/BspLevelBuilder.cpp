#include "BspBinary.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
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

[[nodiscard]] std::string string_or(const Entity &entity, std::string_view key,
                                    std::string fallback = {}) {
  if (const std::string *value = value_for(entity, key)) {
    return *value;
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
    return value;
  }
  return std::nullopt;
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

[[nodiscard]] std::size_t
material_index(stellar::assets::LevelGeometryAsset &geometry,
               std::map<std::string, std::size_t> &materials,
               const std::string &name) {
  const auto existing = materials.find(name);
  if (existing != materials.end()) {
    return existing->second;
  }
  const std::size_t index = geometry.materials.size();
  geometry.materials.push_back(
      stellar::assets::LevelSurfaceMaterial{.name = name, .source_name = name});
  materials.emplace(name, index);
  return index;
}

[[nodiscard]] std::optional<stellar::assets::WorldScriptBinding>
script_binding_for(const Entity &entity) {
  const std::string *script = value_for(entity, "stellar.script");
  if (script == nullptr) {
    script = value_for(entity, "_stellar_script");
  }
  if (script == nullptr) {
    return std::nullopt;
  }
  const std::string *table = value_for(entity, "stellar.table");
  if (table == nullptr) {
    table = value_for(entity, "_stellar_table");
  }
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

[[nodiscard]] bool is_brush_class(std::string_view classname) noexcept {
  return classname.starts_with("func_") || classname.starts_with("trigger_");
}

[[nodiscard]] std::string collision_name_for(const Entity &entity,
                                             int model_index) {
  if (const std::string *targetname = value_for(entity, "targetname");
      targetname != nullptr && !targetname->empty()) {
    return *targetname;
  }
  if (const std::string *classname = value_for(entity, "classname");
      classname != nullptr && !classname->empty()) {
    return *classname + "_" + std::to_string(model_index);
  }
  return "bsp_model_" + std::to_string(model_index);
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
  level.visibility.cluster_count = map.leaves.size();
  level.geometry.raw_lighting = std::move(map.lighting_bytes);
  if (map.has_lighting) {
    level.geometry.lightmaps.push_back(stellar::assets::LevelLightmap{
        .image_index = 0,
        .size = {0U, 0U},
        .style = 0,
        .source_name = "lighting"});
  }
  for (const Leaf &leaf : map.leaves) {
    stellar::assets::LevelLeaf output_leaf{};
    output_leaf.contents = leaf.contents;
    for (std::size_t i = 0; i < 3; ++i) {
      output_leaf.bounds_min[i] = static_cast<float>(leaf.mins[i]);
      output_leaf.bounds_max[i] = static_cast<float>(leaf.maxs[i]);
    }
    if (leaf.visibility_offset >= 0 &&
        static_cast<std::size_t>(leaf.visibility_offset) <
            level.visibility.compressed_pvs.size()) {
      output_leaf.compressed_pvs_offset =
          static_cast<std::size_t>(leaf.visibility_offset);
    }
    for (std::uint16_t i = 0; i < leaf.marksurface_count; ++i) {
      const std::size_t marksurface_index =
          static_cast<std::size_t>(leaf.first_marksurface) + i;
      if (marksurface_index < map.marksurfaces.size()) {
        output_leaf.surface_indices.push_back(
            map.marksurfaces[marksurface_index]);
      }
    }
    level.visibility.leaves.push_back(std::move(output_leaf));
  }

  stellar::assets::MeshAsset mesh{};
  mesh.name = "worldspawn";
  mesh.source_uri = level.source_uri;
  std::map<std::string, std::size_t> material_indices;

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

  const std::size_t model_count = map.models.empty() ? 1 : map.models.size();
  for (std::size_t model_index = 0; model_index < model_count; ++model_index) {
    const Model model =
        map.models.empty()
            ? Model{.first_face = 0,
                    .face_count = static_cast<std::int32_t>(map.faces.size())}
            : map.models[model_index];
    stellar::assets::CollisionMesh collision_mesh{};
    collision_mesh.name = model_index == 0
                              ? "worldspawn"
                              : "bsp_model_" + std::to_string(model_index);
    if (auto found = model_entities.find(static_cast<int>(model_index));
        found != model_entities.end()) {
      collision_mesh.name =
          collision_name_for(*found->second, static_cast<int>(model_index));
    }
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
        continue;
      }
      const std::string texture = texture_name_for(map, face);
      const std::size_t mat =
          material_index(level.geometry, material_indices, texture);

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
          vertex.uv0 = {point[0] * info.s[0] + point[1] * info.s[1] +
                            point[2] * info.s[2] + info.s[3],
                        point[0] * info.t[0] + point[1] * info.t[1] +
                            point[2] * info.t[2] + info.t[3]};
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
        if (options.build_triangle_collision_from_faces) {
          collision_mesh.triangles.push_back(
              stellar::assets::CollisionTriangle{.a = polygon[0],
                                                 .b = polygon[i],
                                                 .c = polygon[i + 1],
                                                 .normal = normal});
        }
      }
      const std::size_t primitive_index = mesh.primitives.size();
      mesh.primitives.push_back(std::move(primitive));
      level.geometry.surfaces.push_back(stellar::assets::LevelSurface{
          .name = "face_" + std::to_string(face_index),
          .mesh_index = 0,
          .primitive_index = primitive_index,
          .material_index = mat,
          .bounds_min = mesh.primitives.back().bounds_min,
          .bounds_max = mesh.primitives.back().bounds_max,
          .source_flags =
              face.texinfo < map.texinfos.size()
                  ? static_cast<std::uint32_t>(map.texinfos[face.texinfo].flags)
                  : 0U});
    }
    if (!collision_mesh.triangles.empty()) {
      collision.meshes.push_back(std::move(collision_mesh));
    }
  }

  if (!mesh.primitives.empty()) {
    level.geometry.meshes.push_back(std::move(mesh));
  }
  if (!collision.meshes.empty()) {
    level.level_collision = std::move(collision);
  }

  bool has_player_spawn = false;
  bool has_worldspawn = false;
  for (const Entity &entity : entities) {
    const std::string classname = string_or(entity, "classname", "entity");
    stellar::assets::WorldMarker marker{};
    bool emit = true;
    if (classname == "worldspawn") {
      has_worldspawn = true;
      emit = false;
    } else if (classname == "info_player_start" ||
               classname == "info_player_deathmatch") {
      has_player_spawn = true;
      marker.type = stellar::assets::WorldMarkerType::kPlayerSpawn;
      marker.name = string_or(entity, "targetname", "Player");
    } else if (classname == "info_stellar_spawn") {
      marker.type = stellar::assets::WorldMarkerType::kEntitySpawn;
      marker.name = string_or(entity, "targetname", classname);
      marker.archetype = string_or(entity, "archetype");
    } else if (classname == "trigger_multiple" || classname == "trigger_once" ||
               classname == "trigger_stellar") {
      marker.type = stellar::assets::WorldMarkerType::kTrigger;
      marker.name = string_or(entity, "targetname", classname);
    } else if (classname == "env_sprite" || classname == "stellar_sprite" ||
               value_for(entity, "stellar.sprite") != nullptr) {
      marker.type = stellar::assets::WorldMarkerType::kSprite;
      marker.name = string_or(entity, "targetname", classname);
      marker.archetype =
          string_or(entity, "archetype", string_or(entity, "stellar.sprite"));
    } else if (classname == "stellar_object_collider" ||
               string_or(entity, "stellar.collider") == "object") {
      marker.type = stellar::assets::WorldMarkerType::kObjectCollider;
      marker.name = string_or(entity, "targetname", classname);
    } else if (is_brush_class(classname)) {
      marker.type = stellar::assets::WorldMarkerType::kEntitySpawn;
      marker.name = string_or(entity, "targetname", classname);
      marker.archetype = classname;
    } else {
      marker.type = stellar::assets::WorldMarkerType::kEntitySpawn;
      marker.name = string_or(entity, "targetname", classname);
      marker.archetype = classname;
    }
    if (!emit) {
      continue;
    }
    if (auto origin = parse_vec3(value_for(entity, "origin"))) {
      marker.position = *origin;
    }
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
    }
    if (auto binding = script_binding_for(entity)) {
      if (script_path_escape(binding->script_id)) {
        return std::unexpected(stellar::platform::Error(
            level.source_uri + ": BSP entity script binding uses an absolute "
                               "path or parent path escape"));
      }
      marker.script = std::move(binding);
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

  return level;
}

} // namespace stellar::import::bsp::detail
