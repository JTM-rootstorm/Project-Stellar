#pragma once

#include "stellar/assets/LevelAsset.hpp"
#include "stellar/import/bsp/Diagnostics.hpp"
#include "stellar/import/bsp/Loader.hpp"
#include "stellar/platform/Error.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace stellar::import::bsp::detail {

constexpr std::int32_t kBspVersionQuake = 29;
constexpr std::int32_t kBspVersionGoldSrc = 30;
constexpr std::size_t kLumpCount = 15;

enum class LumpIndex : std::size_t {
  kEntities = 0,
  kPlanes = 1,
  kTextures = 2,
  kVertices = 3,
  kVisibility = 4,
  kNodes = 5,
  kTexinfo = 6,
  kFaces = 7,
  kLighting = 8,
  kClipnodes = 9,
  kLeaves = 10,
  kMarksurfaces = 11,
  kEdges = 12,
  kSurfedges = 13,
  kModels = 14,
};

struct Lump {
  std::uint32_t offset = 0;
  std::uint32_t size = 0;
};
struct Plane {
  std::array<float, 3> normal{};
  float distance = 0.0F;
  std::int32_t type = 0;
};
struct Vertex {
  std::array<float, 3> position{};
};
struct Edge {
  std::uint16_t first = 0;
  std::uint16_t second = 0;
};
struct Texinfo {
  std::array<float, 4> s{};
  std::array<float, 4> t{};
  std::int32_t miptex = 0;
  std::int32_t flags = 0;
};
struct Face {
  std::uint16_t plane = 0;
  std::uint16_t side = 0;
  std::int32_t first_edge = 0;
  std::uint16_t edge_count = 0;
  std::uint16_t texinfo = 0;
  std::array<std::uint8_t, 4> styles{};
  std::int32_t light_offset = 0;
};
struct Node {
  std::int32_t plane = 0;
  std::array<std::int16_t, 2> children{};
  std::array<std::int16_t, 3> mins{};
  std::array<std::int16_t, 3> maxs{};
  std::uint16_t first_face = 0;
  std::uint16_t face_count = 0;
};
struct Leaf {
  std::int32_t contents = 0;
  std::int32_t visibility_offset = -1;
  std::array<std::int16_t, 3> mins{};
  std::array<std::int16_t, 3> maxs{};
  std::uint16_t first_marksurface = 0;
  std::uint16_t marksurface_count = 0;
  std::array<std::uint8_t, 4> ambient_levels{};
};
struct Model {
  std::array<float, 3> mins{};
  std::array<float, 3> maxs{};
  std::array<float, 3> origin{};
  std::array<std::int32_t, 4> headnodes{};
  std::int32_t visleafs = 0;
  std::int32_t first_face = 0;
  std::int32_t face_count = 0;
};
struct Clipnode {
  std::int32_t plane = 0;
  std::array<std::int16_t, 2> children{};
};

struct EntityKeyValue {
  std::string key;
  std::string value;
};
struct Entity {
  std::vector<EntityKeyValue> pairs;
};

struct BspMap {
  std::int32_t version = 0;
  std::array<Lump, kLumpCount> lumps{};
  std::vector<Plane> planes;
  std::vector<Vertex> vertices;
  std::vector<Edge> edges;
  std::vector<std::int32_t> surfedges;
  std::vector<Texinfo> texinfos;
  std::vector<Face> faces;
  std::vector<Node> nodes;
  std::vector<Leaf> leaves;
  std::vector<std::uint16_t> marksurfaces;
  std::vector<Model> models;
  std::vector<Clipnode> clipnodes;
  std::vector<std::string> texture_names;
  std::vector<std::byte> visibility_bytes;
  std::vector<std::byte> lighting_bytes;
  std::string entity_text;
  bool has_visibility = false;
  bool has_lighting = false;
};

[[nodiscard]] std::expected<BspMap, stellar::platform::Error>
parse_bsp(std::span<const std::byte> bytes, std::string_view source_uri,
          const LoadOptions &options);

[[nodiscard]] std::expected<std::vector<Entity>, stellar::platform::Error>
parse_entities(std::string_view text, std::string_view source_uri);

[[nodiscard]] std::expected<stellar::assets::LevelAsset,
                            stellar::platform::Error>
build_level_asset(BspMap map, std::vector<Entity> entities,
                  std::string source_uri, const LoadOptions &options);

[[nodiscard]] std::expected<stellar::assets::LevelAsset,
                            stellar::platform::Error>
build_level_asset(BspMap map, std::vector<Entity> entities,
                  std::string source_uri, const LoadOptions &options,
                  ImportReport *report);

} // namespace stellar::import::bsp::detail
