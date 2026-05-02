#pragma once

#include "../../src/import/bsp/BspBinary.hpp"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace stellar::tests::fixtures {

using BspVec3 = std::array<float, 3>;

template <typename T>
void append_bsp_value(std::vector<std::byte> &bytes, T value) {
  const auto *raw = reinterpret_cast<const std::byte *>(&value);
  bytes.insert(bytes.end(), raw, raw + sizeof(T));
}

inline void patch_bsp_i32(std::vector<std::byte> &bytes, std::size_t offset,
                          std::int32_t value) {
  std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

inline std::size_t
bsp_lump_header_offset(stellar::import::bsp::detail::LumpIndex lump) {
  return 4 + static_cast<std::size_t>(lump) * 8;
}

inline void set_bsp_lump(std::vector<std::byte> &bytes,
                         stellar::import::bsp::detail::LumpIndex lump,
                         std::size_t offset, std::size_t size) {
  patch_bsp_i32(bytes, bsp_lump_header_offset(lump),
                static_cast<std::int32_t>(offset));
  patch_bsp_i32(bytes, bsp_lump_header_offset(lump) + 4,
                static_cast<std::int32_t>(size));
}

inline void append_bsp_vec3(std::vector<std::byte> &bytes, BspVec3 value) {
  append_bsp_value(bytes, value[0]);
  append_bsp_value(bytes, value[1]);
  append_bsp_value(bytes, value[2]);
}

inline void append_bsp_plane(std::vector<std::byte> &bytes, BspVec3 normal) {
  append_bsp_vec3(bytes, normal);
  append_bsp_value(bytes, 0.0F);
  append_bsp_value<std::int32_t>(bytes, 0);
}

inline void append_bsp_texinfo(std::vector<std::byte> &bytes,
                               std::int32_t miptex = 0) {
  append_bsp_value(bytes, 1.0F);
  append_bsp_value(bytes, 0.0F);
  append_bsp_value(bytes, 0.0F);
  append_bsp_value(bytes, 0.0F);
  append_bsp_value(bytes, 0.0F);
  append_bsp_value(bytes, 1.0F);
  append_bsp_value(bytes, 0.0F);
  append_bsp_value(bytes, 0.0F);
  append_bsp_value<std::int32_t>(bytes, miptex);
  append_bsp_value<std::int32_t>(bytes, 0);
}

inline void append_bsp_miptex_header(std::vector<std::byte> &bytes,
                                     std::string_view texture_name,
                                     std::uint32_t width = 64,
                                     std::uint32_t height = 64) {
  char name[16]{};
  const std::size_t copy_size = std::min<std::size_t>(texture_name.size(), 15);
  std::memcpy(name, texture_name.data(), copy_size);
  bytes.insert(bytes.end(), reinterpret_cast<const std::byte *>(name),
               reinterpret_cast<const std::byte *>(name + 16));
  append_bsp_value<std::uint32_t>(bytes, width);
  append_bsp_value<std::uint32_t>(bytes, height);
  append_bsp_value<std::uint32_t>(bytes, 0);
  append_bsp_value<std::uint32_t>(bytes, 0);
  append_bsp_value<std::uint32_t>(bytes, 0);
  append_bsp_value<std::uint32_t>(bytes, 0);
}

inline void append_bsp_texture_lump(std::vector<std::byte> &bytes,
                                    std::span<const std::string_view> names) {
  append_bsp_value<std::int32_t>(bytes, static_cast<std::int32_t>(names.size()));
  const std::size_t offsets_offset = bytes.size();
  for (std::size_t i = 0; i < names.size(); ++i) {
    append_bsp_value<std::int32_t>(bytes, 0);
  }
  for (std::size_t i = 0; i < names.size(); ++i) {
    const std::size_t texture_lump_offset = offsets_offset - sizeof(std::int32_t);
    const std::size_t miptex_offset = bytes.size() - texture_lump_offset;
    patch_bsp_i32(bytes, offsets_offset + i * 4,
                  static_cast<std::int32_t>(miptex_offset));
    append_bsp_miptex_header(bytes, names[i]);
  }
}

inline void append_bsp_face(std::vector<std::byte> &bytes,
                            std::int32_t first_edge,
                            std::uint16_t texinfo = 0) {
  append_bsp_value<std::uint16_t>(bytes, 0);
  append_bsp_value<std::uint16_t>(bytes, 0);
  append_bsp_value<std::int32_t>(bytes, first_edge);
  append_bsp_value<std::uint16_t>(bytes, 4);
  append_bsp_value<std::uint16_t>(bytes, texinfo);
  append_bsp_value<std::uint8_t>(bytes, 0);
  append_bsp_value<std::uint8_t>(bytes, 0);
  append_bsp_value<std::uint8_t>(bytes, 0);
  append_bsp_value<std::uint8_t>(bytes, 0);
  append_bsp_value<std::int32_t>(bytes, -1);
}

inline void append_bsp_model(std::vector<std::byte> &bytes, BspVec3 mins,
                             BspVec3 maxs, std::int32_t first_face,
                             std::int32_t face_count) {
  append_bsp_vec3(bytes, mins);
  append_bsp_vec3(bytes, maxs);
  append_bsp_vec3(bytes, {0.0F, 0.0F, 0.0F});
  append_bsp_value<std::int32_t>(bytes, 0);
  append_bsp_value<std::int32_t>(bytes, 0);
  append_bsp_value<std::int32_t>(bytes, 0);
  append_bsp_value<std::int32_t>(bytes, 0);
  append_bsp_value<std::int32_t>(bytes, 0);
  append_bsp_value<std::int32_t>(bytes, first_face);
  append_bsp_value<std::int32_t>(bytes, face_count);
}

inline std::vector<std::byte> build_bsp_playable_fixture() {
  using stellar::import::bsp::detail::LumpIndex;

  std::vector<std::byte> bytes(4 + 15 * 8);
  patch_bsp_i32(bytes, 0, 29);

  const std::string entities =
      "{\n\"classname\" \"worldspawn\"\n}\n"
      "{\n\"classname\" \"info_player_start\"\n\"targetname\" \"Start\"\n"
      "\"origin\" \"-1 0 0.6\"\n}\n"
      "{\n\"classname\" \"func_wall\"\n\"targetname\" \"DoorBlocker\"\n"
      "\"model\" \"*1\"\n}\n"
      "{\n\"classname\" \"trigger_multiple\"\n\"targetname\" \"DoorTrigger\"\n"
      "\"model\" \"*2\"\n\"stellar.script\" \"scripts/door.lua\"\n"
      "\"stellar.table\" \"Door\"\n}\n"
      "{\n\"classname\" \"stellar_object_collider\"\n\"targetname\" "
      "\"PickupGem\"\n"
      "\"model\" \"*3\"\n\"archetype\" \"pickup\"\n"
      "\"stellar.script\" \"scripts/pickup.lua\"\n"
      "\"stellar.table\" \"PickupGem\"\n}\n"
      "{\n\"classname\" \"env_sprite\"\n\"targetname\" \"Torch\"\n"
      "\"origin\" \"0 -1 1\"\n\"stellar.sprite\" \"torch\"\n}\n";
  const std::size_t entity_offset = bytes.size();
  bytes.insert(
      bytes.end(), reinterpret_cast<const std::byte *>(entities.data()),
      reinterpret_cast<const std::byte *>(entities.data() + entities.size()));
  set_bsp_lump(bytes, LumpIndex::kEntities, entity_offset, entities.size());

  const std::size_t plane_offset = bytes.size();
  append_bsp_plane(bytes, {0.0F, 0.0F, 1.0F});
  set_bsp_lump(bytes, LumpIndex::kPlanes, plane_offset, 20);

  const std::size_t texture_offset = bytes.size();
  append_bsp_value<std::int32_t>(bytes, 1);
  append_bsp_value<std::int32_t>(bytes, 8);
  const char name[16] = {'s', 't', 'o', 'n', 'e', 0};
  bytes.insert(bytes.end(), reinterpret_cast<const std::byte *>(name),
               reinterpret_cast<const std::byte *>(name + 16));
  append_bsp_value<std::uint32_t>(bytes, 64);
  append_bsp_value<std::uint32_t>(bytes, 64);
  append_bsp_value<std::uint32_t>(bytes, 0);
  append_bsp_value<std::uint32_t>(bytes, 0);
  append_bsp_value<std::uint32_t>(bytes, 0);
  append_bsp_value<std::uint32_t>(bytes, 0);
  set_bsp_lump(bytes, LumpIndex::kTextures, texture_offset,
               bytes.size() - texture_offset);

  const std::size_t vertex_offset = bytes.size();
  const std::array<BspVec3, 8> vertices{{
      {-3.0F, -2.0F, 0.0F},
      {3.0F, -2.0F, 0.0F},
      {3.0F, 2.0F, 0.0F},
      {-3.0F, 2.0F, 0.0F},
      {0.75F, -1.0F, 0.0F},
      {0.75F, -1.0F, 2.0F},
      {0.75F, 1.0F, 2.0F},
      {0.75F, 1.0F, 0.0F},
  }};
  for (const auto &vertex : vertices) {
    append_bsp_vec3(bytes, vertex);
  }
  set_bsp_lump(bytes, LumpIndex::kVertices, vertex_offset,
               vertices.size() * 12);

  const std::size_t texinfo_offset = bytes.size();
  append_bsp_texinfo(bytes);
  set_bsp_lump(bytes, LumpIndex::kTexinfo, texinfo_offset, 40);

  const std::size_t face_offset = bytes.size();
  append_bsp_face(bytes, 0);
  append_bsp_face(bytes, 4);
  set_bsp_lump(bytes, LumpIndex::kFaces, face_offset, 2 * 20);

  const std::size_t edge_offset = bytes.size();
  const std::array<std::uint16_t, 16> edge_vertices{
      {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4}};
  for (const auto edge_vertex : edge_vertices) {
    append_bsp_value<std::uint16_t>(bytes, edge_vertex);
  }
  set_bsp_lump(bytes, LumpIndex::kEdges, edge_offset, edge_vertices.size() * 2);

  const std::size_t surfedge_offset = bytes.size();
  for (std::int32_t i = 0; i < 8; ++i) {
    append_bsp_value<std::int32_t>(bytes, i);
  }
  set_bsp_lump(bytes, LumpIndex::kSurfedges, surfedge_offset, 8 * 4);

  const std::size_t model_offset = bytes.size();
  append_bsp_model(bytes, {-3.0F, -2.0F, 0.0F}, {3.0F, 2.0F, 0.0F}, 0, 1);
  append_bsp_model(bytes, {0.75F, -1.0F, 0.0F}, {0.75F, 1.0F, 2.0F}, 1, 1);
  append_bsp_model(bytes, {-0.45F, -0.5F, 0.0F}, {-0.05F, 0.5F, 1.2F}, 0, 0);
  append_bsp_model(bytes, {0.05F, -0.5F, 0.0F}, {0.45F, 0.5F, 1.2F}, 0, 0);
  set_bsp_lump(bytes, LumpIndex::kModels, model_offset, 4 * 64);

  return bytes;
}

inline std::vector<std::byte> build_bsp_gameplay_room_fixture() {
  using stellar::import::bsp::detail::LumpIndex;

  std::vector<std::byte> bytes(4 + 15 * 8);
  patch_bsp_i32(bytes, 0, 29);

  const std::string entities =
      "{\n\"classname\" \"worldspawn\"\n}\n"
      "{\n\"classname\" \"info_player_start\"\n\"targetname\" \"RoomStart\"\n"
      "\"origin\" \"0 0 36\"\n}\n"
      "{\n\"classname\" \"stellar_sprite\"\n\"targetname\" \"RoomSprite\"\n"
      "\"origin\" \"48 -48 48\"\n\"stellar.sprite\" \"test_sprite\"\n"
      "\"stellar.texture\" \"sprites/test_sprite.png\"\n\"stellar.size\" \"24 48\"\n}\n"
      "{\n\"classname\" \"stellar_object_collider\"\n\"targetname\" \"FuturePickup\"\n"
      "\"origin\" \"-48 48 36\"\n\"stellar.extents\" \"8 8 8\"\n"
      "\"stellar.collider\" \"object\"\n\"archetype\" \"pickup_future\"\n"
      "\"stellar.enabled\" \"yes\"\n}\n"
      "{\n\"classname\" \"trigger_stellar\"\n\"targetname\" \"FutureDoorTrigger\"\n"
      "\"origin\" \"0 -64 36\"\n\"stellar.extents\" \"24 8 36\"\n"
      "\"stellar.once\" \"false\"\n}\n";
  const std::size_t entity_offset = bytes.size();
  bytes.insert(bytes.end(), reinterpret_cast<const std::byte *>(entities.data()),
               reinterpret_cast<const std::byte *>(entities.data() + entities.size()));
  set_bsp_lump(bytes, LumpIndex::kEntities, entity_offset, entities.size());

  const std::size_t plane_offset = bytes.size();
  append_bsp_plane(bytes, {0.0F, 0.0F, 1.0F});
  set_bsp_lump(bytes, LumpIndex::kPlanes, plane_offset, 20);

  const std::size_t texture_offset = bytes.size();
  const std::array<std::string_view, 3> textures{
      "dev/grid_32", "dev/wall_96", "dev/grid_64"};
  append_bsp_texture_lump(bytes, textures);
  set_bsp_lump(bytes, LumpIndex::kTextures, texture_offset,
               bytes.size() - texture_offset);

  const std::size_t vertex_offset = bytes.size();
  const std::array<BspVec3, 24> vertices{{
      {-96.0F, -96.0F, 0.0F}, {-96.0F, 96.0F, 0.0F}, {96.0F, 96.0F, 0.0F},
      {96.0F, -96.0F, 0.0F},  {-96.0F, -96.0F, 96.0F}, {96.0F, -96.0F, 96.0F},
      {96.0F, 96.0F, 96.0F},  {-96.0F, 96.0F, 96.0F}, {96.0F, -96.0F, 0.0F},
      {96.0F, 96.0F, 0.0F},   {96.0F, 96.0F, 96.0F},  {96.0F, -96.0F, 96.0F},
      {-96.0F, -96.0F, 0.0F}, {-96.0F, -96.0F, 96.0F}, {-96.0F, 96.0F, 96.0F},
      {-96.0F, 96.0F, 0.0F},  {-96.0F, 96.0F, 0.0F},  {-96.0F, 96.0F, 96.0F},
      {96.0F, 96.0F, 96.0F},  {96.0F, 96.0F, 0.0F},   {-96.0F, -96.0F, 0.0F},
      {96.0F, -96.0F, 0.0F},  {96.0F, -96.0F, 96.0F}, {-96.0F, -96.0F, 96.0F},
  }};
  for (const auto &vertex : vertices) {
    append_bsp_vec3(bytes, vertex);
  }
  set_bsp_lump(bytes, LumpIndex::kVertices, vertex_offset,
               vertices.size() * 12);

  const std::size_t texinfo_offset = bytes.size();
  append_bsp_texinfo(bytes, 0);
  append_bsp_texinfo(bytes, 2);
  append_bsp_texinfo(bytes, 1);
  set_bsp_lump(bytes, LumpIndex::kTexinfo, texinfo_offset, 3 * 40);

  const std::size_t face_offset = bytes.size();
  append_bsp_face(bytes, 0, 0);
  append_bsp_face(bytes, 4, 1);
  append_bsp_face(bytes, 8, 2);
  append_bsp_face(bytes, 12, 2);
  append_bsp_face(bytes, 16, 2);
  append_bsp_face(bytes, 20, 2);
  set_bsp_lump(bytes, LumpIndex::kFaces, face_offset, 6 * 20);

  const std::size_t edge_offset = bytes.size();
  for (std::uint16_t i = 0; i < vertices.size(); ++i) {
    append_bsp_value<std::uint16_t>(bytes, i);
    append_bsp_value<std::uint16_t>(bytes,
                                   static_cast<std::uint16_t>((i / 4) * 4 + (i + 1) % 4));
  }
  set_bsp_lump(bytes, LumpIndex::kEdges, edge_offset, vertices.size() * 4);

  const std::size_t surfedge_offset = bytes.size();
  for (std::int32_t i = 0; i < static_cast<std::int32_t>(vertices.size()); ++i) {
    append_bsp_value<std::int32_t>(bytes, i);
  }
  set_bsp_lump(bytes, LumpIndex::kSurfedges, surfedge_offset,
               vertices.size() * 4);

  const std::size_t model_offset = bytes.size();
  append_bsp_model(bytes, {-96.0F, -96.0F, 0.0F}, {96.0F, 96.0F, 96.0F}, 0, 6);
  set_bsp_lump(bytes, LumpIndex::kModels, model_offset, 64);

  return bytes;
}

inline void set_bsp_entities(std::vector<std::byte> &bytes,
                             std::string_view entities) {
  const std::size_t entity_offset = bytes.size();
  bytes.insert(bytes.end(), reinterpret_cast<const std::byte *>(entities.data()),
               reinterpret_cast<const std::byte *>(entities.data() + entities.size()));
  set_bsp_lump(bytes, stellar::import::bsp::detail::LumpIndex::kEntities,
               entity_offset, entities.size());
}

inline std::vector<std::byte> build_bsp_minimal_valid_fixture() {
  return build_bsp_playable_fixture();
}

inline std::vector<std::byte> build_bsp_pvs_fixture() {
  using stellar::import::bsp::detail::LumpIndex;
  auto bytes = build_bsp_playable_fixture();

  const std::size_t visibility_offset = bytes.size();
  append_bsp_value<std::uint8_t>(bytes, 0x01U);
  set_bsp_lump(bytes, LumpIndex::kVisibility, visibility_offset, 1);

  const std::size_t marksurface_offset = bytes.size();
  append_bsp_value<std::uint16_t>(bytes, 0);
  set_bsp_lump(bytes, LumpIndex::kMarksurfaces, marksurface_offset, 2);

  const std::size_t leaf_offset = bytes.size();
  append_bsp_value<std::int32_t>(bytes, 0);
  append_bsp_value<std::int32_t>(bytes, 0);
  append_bsp_value<std::int16_t>(bytes, -4);
  append_bsp_value<std::int16_t>(bytes, -4);
  append_bsp_value<std::int16_t>(bytes, -4);
  append_bsp_value<std::int16_t>(bytes, 4);
  append_bsp_value<std::int16_t>(bytes, 4);
  append_bsp_value<std::int16_t>(bytes, 4);
  append_bsp_value<std::uint16_t>(bytes, 0);
  append_bsp_value<std::uint16_t>(bytes, 1);
  append_bsp_value<std::uint8_t>(bytes, 0);
  append_bsp_value<std::uint8_t>(bytes, 0);
  append_bsp_value<std::uint8_t>(bytes, 0);
  append_bsp_value<std::uint8_t>(bytes, 0);
  set_bsp_lump(bytes, LumpIndex::kLeaves, leaf_offset, 28);
  return bytes;
}

inline std::vector<std::byte> build_bsp_malformed_lump_bounds_fixture() {
  using stellar::import::bsp::detail::LumpIndex;
  auto bytes = build_bsp_playable_fixture();
  set_bsp_lump(bytes, LumpIndex::kVertices, bytes.size() + 64, 12);
  return bytes;
}

inline std::vector<std::byte> build_bsp_invalid_face_reference_fixture() {
  auto bytes = build_bsp_playable_fixture();
  std::int32_t face_lump_offset = 0;
  std::memcpy(&face_lump_offset,
              bytes.data() + bsp_lump_header_offset(
                                 stellar::import::bsp::detail::LumpIndex::kFaces),
              sizeof(face_lump_offset));
  const std::size_t face_offset = static_cast<std::size_t>(face_lump_offset);
  patch_bsp_i32(bytes, face_offset + 4, 1000);
  patch_bsp_i32(bytes, face_offset + 20 + 4, 1000);
  return bytes;
}

inline std::vector<std::byte> build_bsp_invalid_entity_vector_fixture() {
  auto bytes = build_bsp_playable_fixture();
  set_bsp_entities(bytes,
                   "{\n\"classname\" \"worldspawn\"\n}\n"
                   "{\n\"classname\" \"info_player_start\"\n\"origin\" \"bad 0 0\"\n}\n");
  return bytes;
}

inline std::vector<std::byte> build_bsp_invalid_script_path_fixture() {
  auto bytes = build_bsp_playable_fixture();
  set_bsp_entities(bytes,
                   "{\n\"classname\" \"worldspawn\"\n}\n"
                   "{\n\"classname\" \"info_player_start\"\n\"origin\" \"0 0 0\"\n}\n"
                   "{\n\"classname\" \"trigger_stellar\"\n\"targetname\" \"Bad\"\n"
                   "\"origin\" \"0 0 0\"\n\"stellar.extents\" \"1 1 1\"\n"
                   "\"stellar.script\" \"../escape.lua\"\n}\n");
  return bytes;
}

inline std::vector<std::byte> build_bsp_missing_player_spawn_fixture() {
  auto bytes = build_bsp_playable_fixture();
  set_bsp_entities(bytes, "{\n\"classname\" \"worldspawn\"\n}\n");
  return bytes;
}

inline std::vector<std::byte> build_named_bsp_fixture(std::string_view name) {
  if (name == "malformed_lump_bounds") {
    return build_bsp_malformed_lump_bounds_fixture();
  }
  if (name == "invalid_script_path") {
    return build_bsp_invalid_script_path_fixture();
  }
  if (name == "pvs") {
    return build_bsp_pvs_fixture();
  }
  if (name == "gameplay_room") {
    return build_bsp_gameplay_room_fixture();
  }
  return build_bsp_playable_fixture();
}

inline void write_bsp_fixture(const std::filesystem::path &path,
                              std::string_view name = "playable") {
  std::filesystem::create_directories(path.parent_path());
  const auto bytes = build_named_bsp_fixture(name);
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
}

} // namespace stellar::tests::fixtures
