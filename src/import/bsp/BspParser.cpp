#include "BspBinary.hpp"

#include <cstring>
#include <limits>
#include <string>

namespace stellar::import::bsp::detail {
namespace {

template <typename T>
[[nodiscard]] T read_le(std::span<const std::byte> bytes,
                        std::size_t offset) noexcept {
  T value{};
  std::memcpy(&value, bytes.data() + offset, sizeof(T));
  return value;
}

[[nodiscard]] std::expected<std::span<const std::byte>,
                            stellar::platform::Error>
lump_bytes(std::span<const std::byte> bytes, const Lump &lump,
           std::string_view source_uri, std::string_view name) {
  const std::uint64_t offset = lump.offset;
  const std::uint64_t size = lump.size;
  if (offset > bytes.size() || size > bytes.size() ||
      offset + size > bytes.size()) {
    return std::unexpected(
        stellar::platform::Error(std::string(source_uri) + ": BSP lump " +
                                 std::string(name) + " is out of bounds"));
  }
  return bytes.subspan(static_cast<std::size_t>(offset),
                       static_cast<std::size_t>(size));
}

template <typename Fn>
[[nodiscard]] std::expected<void, stellar::platform::Error>
parse_records(std::span<const std::byte> bytes, const Lump &lump,
              std::size_t record_size, std::string_view source_uri,
              std::string_view name, Fn &&fn) {
  auto lump_data = lump_bytes(bytes, lump, source_uri, name);
  if (!lump_data) {
    return std::unexpected(lump_data.error());
  }
  if (lump_data->size() % record_size != 0) {
    return std::unexpected(stellar::platform::Error(
        std::string(source_uri) + ": BSP lump " + std::string(name) +
        " size is not a multiple of record size"));
  }
  const std::size_t count = lump_data->size() / record_size;
  for (std::size_t i = 0; i < count; ++i) {
    fn(*lump_data, i * record_size);
  }
  return {};
}

void parse_planes_into(BspMap &map, std::span<const std::byte> bytes,
                       std::size_t off) {
  map.planes.push_back(Plane{.normal = {read_le<float>(bytes, off + 0),
                                        read_le<float>(bytes, off + 4),
                                        read_le<float>(bytes, off + 8)},
                             .distance = read_le<float>(bytes, off + 12),
                             .type = read_le<std::int32_t>(bytes, off + 16)});
}

void parse_vertices_into(BspMap &map, std::span<const std::byte> bytes,
                         std::size_t off) {
  map.vertices.push_back(Vertex{.position = {read_le<float>(bytes, off + 0),
                                             read_le<float>(bytes, off + 4),
                                             read_le<float>(bytes, off + 8)}});
}

void parse_edges_into(BspMap &map, std::span<const std::byte> bytes,
                      std::size_t off) {
  map.edges.push_back(Edge{.first = read_le<std::uint16_t>(bytes, off + 0),
                           .second = read_le<std::uint16_t>(bytes, off + 2)});
}

void parse_texinfos_into(BspMap &map, std::span<const std::byte> bytes,
                         std::size_t off) {
  Texinfo texinfo{};
  for (std::size_t i = 0; i < 4; ++i) {
    texinfo.s[i] = read_le<float>(bytes, off + i * 4);
    texinfo.t[i] = read_le<float>(bytes, off + 16 + i * 4);
  }
  texinfo.miptex = read_le<std::int32_t>(bytes, off + 32);
  texinfo.flags = read_le<std::int32_t>(bytes, off + 36);
  map.texinfos.push_back(texinfo);
}

void parse_faces_into(BspMap &map, std::span<const std::byte> bytes,
                       std::size_t off) {
  Face face{};
  face.plane = read_le<std::uint16_t>(bytes, off + 0);
  face.side = read_le<std::uint16_t>(bytes, off + 2);
  face.first_edge = read_le<std::int32_t>(bytes, off + 4);
  face.edge_count = read_le<std::uint16_t>(bytes, off + 8);
  face.texinfo = read_le<std::uint16_t>(bytes, off + 10);
  for (std::size_t i = 0; i < 4; ++i) {
    face.styles[i] = read_le<std::uint8_t>(bytes, off + 12 + i);
  }
  face.light_offset = read_le<std::int32_t>(bytes, off + 16);
  map.faces.push_back(face);
}

void parse_nodes_into(BspMap &map, std::span<const std::byte> bytes,
                      std::size_t off) {
  Node node{};
  node.plane = read_le<std::int32_t>(bytes, off + 0);
  node.children = {read_le<std::int16_t>(bytes, off + 4),
                   read_le<std::int16_t>(bytes, off + 6)};
  for (std::size_t i = 0; i < 3; ++i) {
    node.mins[i] = read_le<std::int16_t>(bytes, off + 8 + i * 2);
    node.maxs[i] = read_le<std::int16_t>(bytes, off + 14 + i * 2);
  }
  node.first_face = read_le<std::uint16_t>(bytes, off + 20);
  node.face_count = read_le<std::uint16_t>(bytes, off + 22);
  map.nodes.push_back(node);
}

void parse_leaves_into(BspMap &map, std::span<const std::byte> bytes,
                       std::size_t off) {
  Leaf leaf{};
  leaf.contents = read_le<std::int32_t>(bytes, off + 0);
  leaf.visibility_offset = read_le<std::int32_t>(bytes, off + 4);
  for (std::size_t i = 0; i < 3; ++i) {
    leaf.mins[i] = read_le<std::int16_t>(bytes, off + 8 + i * 2);
    leaf.maxs[i] = read_le<std::int16_t>(bytes, off + 14 + i * 2);
  }
  leaf.first_marksurface = read_le<std::uint16_t>(bytes, off + 20);
  leaf.marksurface_count = read_le<std::uint16_t>(bytes, off + 22);
  for (std::size_t i = 0; i < 4; ++i) {
    leaf.ambient_levels[i] = read_le<std::uint8_t>(bytes, off + 24 + i);
  }
  map.leaves.push_back(leaf);
}

void parse_models_into(BspMap &map, std::span<const std::byte> bytes,
                       std::size_t off) {
  Model model{};
  for (std::size_t i = 0; i < 3; ++i) {
    model.mins[i] = read_le<float>(bytes, off + i * 4);
    model.maxs[i] = read_le<float>(bytes, off + 12 + i * 4);
    model.origin[i] = read_le<float>(bytes, off + 24 + i * 4);
  }
  for (std::size_t i = 0; i < 4; ++i) {
    model.headnodes[i] = read_le<std::int32_t>(bytes, off + 36 + i * 4);
  }
  model.visleafs = read_le<std::int32_t>(bytes, off + 52);
  model.first_face = read_le<std::int32_t>(bytes, off + 56);
  model.face_count = read_le<std::int32_t>(bytes, off + 60);
  map.models.push_back(model);
}

void parse_clipnodes_into(BspMap &map, std::span<const std::byte> bytes,
                          std::size_t off) {
  map.clipnodes.push_back(
      Clipnode{.plane = read_le<std::int32_t>(bytes, off + 0),
               .children = {read_le<std::int16_t>(bytes, off + 4),
                            read_le<std::int16_t>(bytes, off + 6)}});
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
parse_textures(BspMap &map, std::span<const std::byte> bytes,
               std::string_view source_uri) {
  auto lump = lump_bytes(
      bytes, map.lumps[static_cast<std::size_t>(LumpIndex::kTextures)],
      source_uri, "textures");
  if (!lump) {
    return std::unexpected(lump.error());
  }
  if (lump->empty()) {
    return {};
  }
  if (lump->size() < 4) {
    return std::unexpected(stellar::platform::Error(
        std::string(source_uri) + ": BSP texture lump is too small"));
  }
  const auto count = read_le<std::int32_t>(*lump, 0);
  if (count < 0 ||
      4ULL + static_cast<std::uint64_t>(count) * 4ULL > lump->size()) {
    return std::unexpected(stellar::platform::Error(
        std::string(source_uri) + ": BSP texture directory is invalid"));
  }
  map.texture_names.resize(static_cast<std::size_t>(count));
  map.textures.resize(static_cast<std::size_t>(count));
  for (std::int32_t i = 0; i < count; ++i) {
    const auto entry_offset =
        read_le<std::int32_t>(*lump, 4 + static_cast<std::size_t>(i) * 4);
    if (entry_offset < 0) {
      continue;
    }
    const std::uint64_t name_offset = static_cast<std::uint64_t>(entry_offset);
    constexpr std::uint64_t kMiptexHeaderSize = 40;
    if (name_offset + kMiptexHeaderSize > lump->size()) {
      return std::unexpected(stellar::platform::Error(
          std::string(source_uri) + ": BSP texture entry is out of bounds"));
    }
    Miptex miptex{};
    const char *name = reinterpret_cast<const char *>(lump->data() + name_offset);
    miptex.name = std::string(name, strnlen(name, 16));
    miptex.width = read_le<std::uint32_t>(*lump, name_offset + 16);
    miptex.height = read_le<std::uint32_t>(*lump, name_offset + 20);
    for (std::size_t level = 0; level < miptex.offsets.size(); ++level) {
      miptex.offsets[level] = read_le<std::uint32_t>(*lump, name_offset + 24 + level * 4);
    }
    map.texture_names[static_cast<std::size_t>(i)] = miptex.name;

    const std::uint64_t pixel_count =
        static_cast<std::uint64_t>(miptex.width) * static_cast<std::uint64_t>(miptex.height);
    if (miptex.width > 0 && miptex.height > 0 && miptex.offsets[0] > 0 &&
        pixel_count <= static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
      const std::uint64_t pixels_begin = name_offset + miptex.offsets[0];
      const std::uint64_t pixels_end = pixels_begin + pixel_count;
      if (pixels_begin <= lump->size() && pixels_end <= lump->size()) {
        const auto begin = lump->begin() + static_cast<std::ptrdiff_t>(pixels_begin);
        const auto end = lump->begin() + static_cast<std::ptrdiff_t>(pixels_end);
        miptex.pixels.reserve(static_cast<std::size_t>(pixel_count));
        for (auto iter = begin; iter != end; ++iter) {
          miptex.pixels.push_back(std::to_integer<std::uint8_t>(*iter));
        }
        miptex.has_embedded_pixels = true;
      }
    }
    map.textures[static_cast<std::size_t>(i)] = std::move(miptex);
  }
  return {};
}

} // namespace

std::expected<BspMap, stellar::platform::Error>
parse_bsp(std::span<const std::byte> bytes, std::string_view source_uri,
          const LoadOptions &options) {
  constexpr std::size_t kHeaderSize = 4 + kLumpCount * 8;
  if (bytes.size() < kHeaderSize) {
    return std::unexpected(stellar::platform::Error(
        std::string(source_uri) + ": BSP header is too small"));
  }

  BspMap map{};
  map.version = read_le<std::int32_t>(bytes, 0);
  if (map.version != kBspVersionQuake && map.version != kBspVersionGoldSrc) {
    return std::unexpected(stellar::platform::Error(
        std::string(source_uri) + ": unsupported BSP version " +
        std::to_string(map.version)));
  }

  for (std::size_t i = 0; i < kLumpCount; ++i) {
    const std::int32_t offset = read_le<std::int32_t>(bytes, 4 + i * 8);
    const std::int32_t size = read_le<std::int32_t>(bytes, 8 + i * 8);
    if (offset < 0 || size < 0) {
      return std::unexpected(stellar::platform::Error(
          std::string(source_uri) + ": BSP lump has negative offset or size"));
    }
    map.lumps[i] = Lump{.offset = static_cast<std::uint32_t>(offset),
                        .size = static_cast<std::uint32_t>(size)};
    auto bounds = lump_bytes(bytes, map.lumps[i], source_uri, "header");
    if (!bounds) {
      return std::unexpected(bounds.error());
    }
  }

  auto entities = lump_bytes(bytes, map.lumps[0], source_uri, "entities");
  if (!entities) {
    return std::unexpected(entities.error());
  }
  map.entity_text.assign(reinterpret_cast<const char *>(entities->data()),
                         entities->size());
  if (options.parse_visibility) {
    auto visibility = lump_bytes(bytes, map.lumps[4], source_uri, "visibility");
    if (!visibility) {
      return std::unexpected(visibility.error());
    }
    map.visibility_bytes.assign(visibility->begin(), visibility->end());
  }
  if (options.parse_lightmaps) {
    auto lighting = lump_bytes(bytes, map.lumps[8], source_uri, "lighting");
    if (!lighting) {
      return std::unexpected(lighting.error());
    }
    map.lighting_bytes.assign(lighting->begin(), lighting->end());
  }
  map.has_visibility = options.parse_visibility && !map.visibility_bytes.empty();
  map.has_lighting = options.parse_lightmaps && !map.lighting_bytes.empty();

  auto planes = parse_records(
      bytes, map.lumps[1], 20, source_uri, "planes",
      [&](auto data, auto off) { parse_planes_into(map, data, off); });
  if (!planes) {
    return std::unexpected(planes.error());
  }
  auto vertices = parse_records(
      bytes, map.lumps[3], 12, source_uri, "vertices",
      [&](auto data, auto off) { parse_vertices_into(map, data, off); });
  if (!vertices) {
    return std::unexpected(vertices.error());
  }
  auto texinfos = parse_records(
      bytes, map.lumps[6], 40, source_uri, "texinfo",
      [&](auto data, auto off) { parse_texinfos_into(map, data, off); });
  if (!texinfos) {
    return std::unexpected(texinfos.error());
  }
  auto faces = parse_records(
      bytes, map.lumps[7], 20, source_uri, "faces",
      [&](auto data, auto off) { parse_faces_into(map, data, off); });
  if (!faces) {
    return std::unexpected(faces.error());
  }
  auto clipnodes = parse_records(
      bytes, map.lumps[9], 8, source_uri, "clipnodes",
      [&](auto data, auto off) { parse_clipnodes_into(map, data, off); });
  if (!clipnodes) {
    return std::unexpected(clipnodes.error());
  }
  auto nodes = parse_records(bytes, map.lumps[5], 24, source_uri, "nodes",
                             [&](auto data, auto off) {
                               parse_nodes_into(map, data, off);
                             });
  if (!nodes) {
    return std::unexpected(nodes.error());
  }
  auto leaves = parse_records(bytes, map.lumps[10], 28, source_uri, "leaves",
                               [&](auto data, auto off) {
                                 parse_leaves_into(map, data, off);
                               });
  if (!leaves) {
    return std::unexpected(leaves.error());
  }
  auto marksurfaces = parse_records(bytes, map.lumps[11], 2, source_uri,
                                    "marksurfaces", [&](auto data, auto off) {
                                      map.marksurfaces.push_back(
                                          read_le<std::uint16_t>(data, off));
                                    });
  if (!marksurfaces) {
    return std::unexpected(marksurfaces.error());
  }
  auto edges = parse_records(
      bytes, map.lumps[12], 4, source_uri, "edges",
      [&](auto data, auto off) { parse_edges_into(map, data, off); });
  if (!edges) {
    return std::unexpected(edges.error());
  }
  auto surfedges =
      parse_records(bytes, map.lumps[13], 4, source_uri, "surfedges",
                    [&](auto data, auto off) {
                      map.surfedges.push_back(read_le<std::int32_t>(data, off));
                    });
  if (!surfedges) {
    return std::unexpected(surfedges.error());
  }
  auto models = parse_records(
      bytes, map.lumps[14], 64, source_uri, "models",
      [&](auto data, auto off) { parse_models_into(map, data, off); });
  if (!models) {
    return std::unexpected(models.error());
  }
  auto textures = parse_textures(map, bytes, source_uri);
  if (!textures) {
    return std::unexpected(textures.error());
  }

  return map;
}

} // namespace stellar::import::bsp::detail
