#include "stellar/import/bsp/Loader.hpp"

#include "../../../src/import/bsp/BspBinary.hpp"
#include "../../fixtures/BspFixture.hpp"

#include <array>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

using stellar::import::bsp::detail::LumpIndex;

template <typename T> void append(std::vector<std::byte> &bytes, T value) {
  const auto *raw = reinterpret_cast<const std::byte *>(&value);
  bytes.insert(bytes.end(), raw, raw + sizeof(T));
}

void patch_i32(std::vector<std::byte> &bytes, std::size_t offset,
               std::int32_t value) {
  std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::size_t lump_header_offset(LumpIndex lump) {
  return 4 + static_cast<std::size_t>(lump) * 8;
}

void set_lump(std::vector<std::byte> &bytes, LumpIndex lump, std::size_t offset,
              std::size_t size) {
  patch_i32(bytes, lump_header_offset(lump), static_cast<std::int32_t>(offset));
  patch_i32(bytes, lump_header_offset(lump) + 4,
            static_cast<std::int32_t>(size));
}

void append_vec3(std::vector<std::byte> &bytes, float x, float y, float z) {
  append(bytes, x);
  append(bytes, y);
  append(bytes, z);
}

void append_plane(std::vector<std::byte> &bytes) {
  append_vec3(bytes, 0.0F, 0.0F, 1.0F);
  append(bytes, 0.0F);
  append<std::int32_t>(bytes, 0);
}

void append_texinfo(std::vector<std::byte> &bytes) {
  append(bytes, 1.0F);
  append(bytes, 0.0F);
  append(bytes, 0.0F);
  append(bytes, 0.0F);
  append(bytes, 0.0F);
  append(bytes, 1.0F);
  append(bytes, 0.0F);
  append(bytes, 0.0F);
  append<std::int32_t>(bytes, 0);
  append<std::int32_t>(bytes, 0);
}

void append_face(std::vector<std::byte> &bytes, std::int32_t first_edge) {
  append<std::uint16_t>(bytes, 0);
  append<std::uint16_t>(bytes, 0);
  append<std::int32_t>(bytes, first_edge);
  append<std::uint16_t>(bytes, 4);
  append<std::uint16_t>(bytes, 0);
  append<std::uint8_t>(bytes, 0);
  append<std::uint8_t>(bytes, 0);
  append<std::uint8_t>(bytes, 0);
  append<std::uint8_t>(bytes, 0);
  append<std::int32_t>(bytes, -1);
}

void append_model(std::vector<std::byte> &bytes, std::array<float, 3> mins,
                  std::array<float, 3> maxs, std::int32_t first_face,
                  std::int32_t face_count) {
  append_vec3(bytes, mins[0], mins[1], mins[2]);
  append_vec3(bytes, maxs[0], maxs[1], maxs[2]);
  append_vec3(bytes, 0.0F, 0.0F, 0.0F);
  append<std::int32_t>(bytes, 0);
  append<std::int32_t>(bytes, 0);
  append<std::int32_t>(bytes, 0);
  append<std::int32_t>(bytes, 0);
  append<std::int32_t>(bytes, 0);
  append<std::int32_t>(bytes, first_face);
  append<std::int32_t>(bytes, face_count);
}

void append_leaf(std::vector<std::byte> &bytes, std::int32_t visibility_offset,
                 std::uint16_t first_marksurface,
                 std::uint16_t marksurface_count) {
  append<std::int32_t>(bytes, 0);
  append<std::int32_t>(bytes, visibility_offset);
  append<std::int16_t>(bytes, 0);
  append<std::int16_t>(bytes, 0);
  append<std::int16_t>(bytes, 0);
  append<std::int16_t>(bytes, 64);
  append<std::int16_t>(bytes, 64);
  append<std::int16_t>(bytes, 64);
  append<std::uint16_t>(bytes, first_marksurface);
  append<std::uint16_t>(bytes, marksurface_count);
  append<std::uint8_t>(bytes, 0);
  append<std::uint8_t>(bytes, 0);
  append<std::uint8_t>(bytes, 0);
  append<std::uint8_t>(bytes, 0);
}

std::vector<std::byte> minimal_bsp(std::int32_t version = 29,
                                   bool rich_entities = false) {
  std::vector<std::byte> bytes(4 + 15 * 8);
  patch_i32(bytes, 0, version);

  const std::string entities =
      rich_entities
          ? "{\n\"classname\" \"worldspawn\"\n}\n"
            "{\n\"classname\" \"info_player_start\"\n\"targetname\" "
            "\"Start\"\n\"origin\" \"1 2 3\"\n}\n"
            "{\n\"classname\" \"trigger_multiple\"\n\"targetname\" "
            "\"DoorTrigger\"\n\"model\" \"*1\"\n\"stellar.script\" "
            "\"scripts/door.lua\"\n\"stellar.table\" \"Door\"\n}\n"
            "{\n\"classname\" \"env_sprite\"\n\"targetname\" "
            "\"Torch\"\n\"stellar.sprite\" \"torch\"\n}\n"
            "{\n\"classname\" \"stellar_object_collider\"\n\"targetname\" "
            "\"Sensor\"\n\"model\" \"*1\"\n}\n"
          : "{\n\"classname\" \"worldspawn\"\n\"message\" \"hi\\\"there\"\n}\n";
  const std::size_t entity_offset = bytes.size();
  bytes.insert(
      bytes.end(), reinterpret_cast<const std::byte *>(entities.data()),
      reinterpret_cast<const std::byte *>(entities.data() + entities.size()));
  set_lump(bytes, LumpIndex::kEntities, entity_offset, entities.size());

  const std::size_t plane_offset = bytes.size();
  append_plane(bytes);
  set_lump(bytes, LumpIndex::kPlanes, plane_offset, 20);

  const std::size_t texture_offset = bytes.size();
  append<std::int32_t>(bytes, 1);
  append<std::int32_t>(bytes, 8);
  const char name[16] = {'s', 't', 'o', 'n', 'e', 0};
  bytes.insert(bytes.end(), reinterpret_cast<const std::byte *>(name),
               reinterpret_cast<const std::byte *>(name + 16));
  append<std::uint32_t>(bytes, 64);
  append<std::uint32_t>(bytes, 64);
  append<std::uint32_t>(bytes, 0);
  append<std::uint32_t>(bytes, 0);
  append<std::uint32_t>(bytes, 0);
  append<std::uint32_t>(bytes, 0);
  set_lump(bytes, LumpIndex::kTextures, texture_offset,
           bytes.size() - texture_offset);

  const std::size_t vertex_offset = bytes.size();
  append_vec3(bytes, 0.0F, 0.0F, 0.0F);
  append_vec3(bytes, 64.0F, 0.0F, 0.0F);
  append_vec3(bytes, 64.0F, 64.0F, 0.0F);
  append_vec3(bytes, 0.0F, 64.0F, 0.0F);
  set_lump(bytes, LumpIndex::kVertices, vertex_offset, 4 * 12);

  const std::size_t texinfo_offset = bytes.size();
  append_texinfo(bytes);
  set_lump(bytes, LumpIndex::kTexinfo, texinfo_offset, 40);

  const std::size_t face_offset = bytes.size();
  append_face(bytes, 0);
  set_lump(bytes, LumpIndex::kFaces, face_offset, 20);

  const std::size_t edge_offset = bytes.size();
  append<std::uint16_t>(bytes, 0);
  append<std::uint16_t>(bytes, 1);
  append<std::uint16_t>(bytes, 1);
  append<std::uint16_t>(bytes, 2);
  append<std::uint16_t>(bytes, 2);
  append<std::uint16_t>(bytes, 3);
  append<std::uint16_t>(bytes, 3);
  append<std::uint16_t>(bytes, 0);
  set_lump(bytes, LumpIndex::kEdges, edge_offset, 16);

  const std::size_t surfedge_offset = bytes.size();
  append<std::int32_t>(bytes, 0);
  append<std::int32_t>(bytes, 1);
  append<std::int32_t>(bytes, 2);
  append<std::int32_t>(bytes, 3);
  set_lump(bytes, LumpIndex::kSurfedges, surfedge_offset, 16);

  const std::size_t model_offset = bytes.size();
  append_model(bytes, {0.0F, 0.0F, 0.0F}, {64.0F, 64.0F, 0.0F}, 0, 1);
  append_model(bytes, {-8.0F, -8.0F, -8.0F}, {8.0F, 8.0F, 8.0F}, 0, 0);
  set_lump(bytes, LumpIndex::kModels, model_offset, 2 * 64);
  return bytes;
}

void bsp_parser_rejects_invalid_header() {
  std::vector<std::byte> bytes(8);
  auto result = stellar::import::bsp::load_level_from_memory(bytes, "tiny.bsp");
  assert(!result);
  assert(result.error().message.find("header") != std::string::npos);
}

void bsp_parser_rejects_unknown_version() {
  auto bytes = minimal_bsp(31);
  auto result =
      stellar::import::bsp::load_level_from_memory(bytes, "bad_version.bsp");
  assert(!result);
  assert(result.error().message.find("31") != std::string::npos);
  assert(result.error().message.find("BSP29") != std::string::npos);
  assert(result.error().message.find("BSP30") != std::string::npos);
}

void bsp_parser_rejects_source_vbsp_header() {
  auto bytes = minimal_bsp();
  patch_i32(bytes, 0, 0x50534256); // "VBSP" little-endian, not classic BSP30.
  auto result =
      stellar::import::bsp::load_level_from_memory(bytes, "source_like.bsp");
  assert(!result);
  assert(result.error().message.find("unsupported BSP version") !=
         std::string::npos);
  assert(result.error().message.find("Source/VBSP") != std::string::npos);
}

void bsp_parser_checks_lump_bounds() {
  auto bytes = minimal_bsp();
  set_lump(bytes, LumpIndex::kVertices, bytes.size() + 100, 12);
  auto result =
      stellar::import::bsp::load_level_from_memory(bytes, "bad_lump.bsp");
  assert(!result);
  assert(result.error().message.find("out of bounds") != std::string::npos);
}

void bsp30_parser_rejects_malformed_lump_size_safely() {
  auto bytes = minimal_bsp(30);
  std::int32_t face_lump_offset = 0;
  std::memcpy(&face_lump_offset,
              bytes.data() + lump_header_offset(LumpIndex::kFaces),
              sizeof(face_lump_offset));
  set_lump(bytes, LumpIndex::kFaces,
           static_cast<std::size_t>(face_lump_offset), 19);
  auto result = stellar::import::bsp::load_level_from_memory(
      bytes, "malformed_faces_bsp30.bsp");
  assert(!result);
  assert(result.error().message.find("multiple of record size") !=
         std::string::npos);
}

void bsp_parser_loads_minimal_worldspawn_geometry() {
  auto bytes = minimal_bsp(30);
  auto result =
      stellar::import::bsp::load_level_from_memory(bytes, "world.bsp");
  assert(result);
  assert(result->geometry.meshes.size() == 1);
  assert(result->geometry.meshes[0].primitives.size() == 1);
  assert(result->geometry.materials.size() == 1);
  assert(result->geometry.materials[0].name == "stone");
  assert(result->geometry.meshes[0].primitives[0].indices.size() == 6);
}

void bsp_parser_preserves_legacy_bsp29_support() {
  auto bytes = minimal_bsp(29);
  auto result =
      stellar::import::bsp::load_level_from_memory(bytes, "legacy_bsp29.bsp");
  assert(result);
  assert(result->geometry.meshes.size() == 1);
}

void bsp30_zup_room_imports_coordinates_without_axis_swaps() {
  auto bytes = stellar::tests::fixtures::build_bsp_gameplay_room_fixture();
  auto result = stellar::import::bsp::load_level_from_memory(
      bytes, "minimal_zup_room_bsp30.bsp");
  assert(result);
  assert(result->world_metadata.markers.size() == 4);
  const auto &spawn = result->world_metadata.markers[0];
  assert(spawn.type == stellar::assets::WorldMarkerType::kPlayerSpawn);
  assert(spawn.position[0] == 0.0F);
  assert(spawn.position[1] == 0.0F);
  assert(spawn.position[2] == 36.0F);

  assert(result->level_collision.has_value());
  const auto &collision = *result->level_collision;
  assert(collision.bounds_min[2] == 0.0F);
  assert(collision.bounds_max[2] == 96.0F);
  assert(!collision.meshes.empty());
  assert(!collision.meshes[0].triangles.empty());
  const auto &floor = collision.meshes[0].triangles[0];
  assert(floor.a[2] == 0.0F);
  assert(floor.b[2] == 0.0F);
  assert(floor.c[2] == 0.0F);
  assert(floor.normal[0] == 0.0F);
  assert(floor.normal[1] == 0.0F);
  assert(floor.normal[2] == 1.0F);
}

void bsp30_entity_lump_preserves_dotted_and_alias_keys() {
  auto bytes = minimal_bsp(30, true);
  auto result = stellar::import::bsp::load_level_from_memory(
      bytes, "entity_keys_bsp30.bsp");
  assert(result);
  const auto &trigger = result->world_metadata.markers[1];
  assert(trigger.script.has_value());
  assert(trigger.script->script_id == "scripts/door.lua");
  assert(trigger.script->table_name == "Door");
  bool saw_dotted = false;
  for (const auto &property : trigger.properties) {
    saw_dotted = saw_dotted || property.key == "stellar.script";
  }
  assert(saw_dotted);

  const std::string alias_entities =
      "{\n\"classname\" \"worldspawn\"\n}\n"
      "{\n\"classname\" \"info_player_start\"\n\"origin\" \"0 0 36\"\n}\n"
      "{\n\"classname\" \"trigger_multiple\"\n\"targetname\" \"AliasTrigger\"\n"
      "\"origin\" \"8 9 10\"\n\"_stellar_extents\" \"4 5 6\"\n"
      "\"_stellar_script\" \"scripts/alias.lua\"\n"
      "\"_stellar_table\" \"Alias\"\n}\n";
  auto alias_bytes = minimal_bsp(30, true);
  const std::size_t entity_offset = alias_bytes.size();
  alias_bytes.insert(
      alias_bytes.end(),
      reinterpret_cast<const std::byte *>(alias_entities.data()),
      reinterpret_cast<const std::byte *>(alias_entities.data() + alias_entities.size()));
  set_lump(alias_bytes, LumpIndex::kEntities, entity_offset,
           alias_entities.size());
  auto alias_result = stellar::import::bsp::load_level_from_memory(
      alias_bytes, "entity_alias_keys_bsp30.bsp");
  assert(alias_result);
  const auto &alias_trigger = alias_result->world_metadata.markers[1];
  assert(alias_trigger.script.has_value());
  assert(alias_trigger.script->script_id == "scripts/alias.lua");
  assert(alias_trigger.script->table_name == "Alias");
  assert(alias_trigger.position[0] == 8.0F);
  assert(alias_trigger.position[1] == 9.0F);
  assert(alias_trigger.position[2] == 10.0F);
  assert(alias_trigger.scale[0] == 4.0F);
  assert(alias_trigger.scale[1] == 5.0F);
  assert(alias_trigger.scale[2] == 6.0F);
}

void bsp30_developer_material_fallback_is_deterministic() {
  auto bytes = stellar::tests::fixtures::build_bsp_gameplay_room_fixture();
  auto result = stellar::import::bsp::load_level_from_memory(
      bytes, "dev_materials_bsp30.bsp");
  assert(result);
  assert(result->geometry.images.size() == 3);
  assert(result->geometry.images[0].name == "stellar_dev_grid_32");
  assert(result->geometry.images[1].name == "stellar_dev_grid_64");
  assert(result->geometry.images[2].name == "stellar_dev_wall_96");
  assert(result->geometry.images[0].pixels.size() == 32U * 32U * 4U);
  assert(result->geometry.images[0].pixels[0] == 255U);
}

void trenchbroom_entity_matrix_imports_expected_metadata() {
  auto bytes =
      stellar::tests::fixtures::build_bsp_trenchbroom_entity_matrix_fixture();
  auto result = stellar::import::bsp::load_level_from_memory(
      bytes, "tests/fixtures/trenchbroom/compiled/entity_matrix_zup.bsp");
  assert(result);

  std::map<std::string, stellar::assets::WorldMarkerType> markers;
  for (const auto &marker : result->world_metadata.markers) {
    markers.emplace(marker.name, marker.type);
  }
  assert(markers["MatrixStart"] == stellar::assets::WorldMarkerType::kPlayerSpawn);
  assert(markers["SpawnCrate"] == stellar::assets::WorldMarkerType::kEntitySpawn);
  assert(markers["TriggerStellar"] == stellar::assets::WorldMarkerType::kTrigger);
  assert(markers["TriggerMultiple"] == stellar::assets::WorldMarkerType::kTrigger);
  assert(markers["TriggerOnce"] == stellar::assets::WorldMarkerType::kTrigger);
  assert(markers["SpriteStellar"] == stellar::assets::WorldMarkerType::kSprite);
  assert(markers["SpriteEnv"] == stellar::assets::WorldMarkerType::kSprite);
  assert(markers["PickupSensor"] == stellar::assets::WorldMarkerType::kObjectCollider);
  assert(markers["StaticWall"] == stellar::assets::WorldMarkerType::kEntitySpawn);
  assert(markers["GateDoor"] == stellar::assets::WorldMarkerType::kEntitySpawn);
  assert(markers["GateButton"] == stellar::assets::WorldMarkerType::kEntitySpawn);

  const auto trigger = std::ranges::find_if(
      result->world_metadata.markers, [](const auto &marker) {
        return marker.name == "TriggerStellar";
      });
  assert(trigger != result->world_metadata.markers.end());
  assert(trigger->script.has_value());
  assert(trigger->script->script_id == "scripts/gate.lua");
  assert(trigger->script->table_name == "GateTrigger");
  assert(trigger->scale[0] == 16.0F);
  bool saw_alias_script = false;
  for (const auto &property : trigger->properties) {
    saw_alias_script = saw_alias_script || property.key == "_stellar_script";
  }
  assert(saw_alias_script);
}

void trenchbroom_invalid_script_escape_fails_import() {
  auto bytes = stellar::tests::fixtures::
      build_bsp_trenchbroom_invalid_script_escape_fixture();
  auto result = stellar::import::bsp::load_level_from_memory(
      bytes, "tests/fixtures/trenchbroom/compiled/invalid_script_escape_zup.bsp");
  assert(!result);
  assert(result.error().message.find("script binding") != std::string::npos);
}

void bsp_parser_builds_collision_triangles() {
  auto bytes = minimal_bsp();
  auto result =
      stellar::import::bsp::load_level_from_memory(bytes, "collision.bsp");
  assert(result);
  assert(result->level_collision.has_value());
  assert(result->level_collision->meshes.size() == 1);
  assert(result->level_collision->meshes[0].name == "worldspawn");
  assert(result->level_collision->meshes[0].triangles.size() == 2);
}

void bsp_entity_parser_preserves_key_values() {
  const std::string text =
      "{\"classname\" \"worldspawn\" \"message\" \"a\\\"b\\\\c\"}";
  auto entities =
      stellar::import::bsp::detail::parse_entities(text, "entities");
  assert(entities);
  assert((*entities)[0].pairs[1].key == "message");
  assert((*entities)[0].pairs[1].value == "a\"b\\c");
}

void bsp_entity_parser_rejects_malformed_entity_text() {
  auto entities = stellar::import::bsp::detail::parse_entities(
      "{\"classname\" \"worldspawn\"", "bad");
  assert(!entities);
  assert(entities.error().message.find("unterminated") != std::string::npos);
}

void bsp_entities_build_player_spawn_trigger_sprite_object_collider_markers() {
  auto bytes = minimal_bsp(29, true);
  auto result =
      stellar::import::bsp::load_level_from_memory(bytes, "metadata.bsp");
  assert(result);
  assert(result->world_metadata.markers.size() == 4);
  assert(result->world_metadata.markers[0].type ==
         stellar::assets::WorldMarkerType::kPlayerSpawn);
  assert(result->world_metadata.markers[1].type ==
         stellar::assets::WorldMarkerType::kTrigger);
  assert(result->world_metadata.markers[1].scale[0] == 8.0F);
  assert(result->world_metadata.markers[2].type ==
         stellar::assets::WorldMarkerType::kSprite);
  assert(result->world_metadata.markers[3].type ==
         stellar::assets::WorldMarkerType::kObjectCollider);
}

void bsp_script_bindings_import_to_world_metadata() {
  auto bytes = minimal_bsp(29, true);
  auto result =
      stellar::import::bsp::load_level_from_memory(bytes, "scripts.bsp");
  assert(result);
  const auto &marker = result->world_metadata.markers[1];
  assert(marker.script.has_value());
  assert(marker.script->script_id == "scripts/door.lua");
  assert(marker.script->table_name == "Door");
}

void bsp_loader_is_display_free() {
  auto bytes = minimal_bsp();
  auto result =
      stellar::import::bsp::load_level_from_memory(bytes, "display_free.bsp");
  assert(result);
}

void bsp_import_report_collects_missing_player_spawn_warning() {
  auto bytes = minimal_bsp();
  auto result = stellar::import::bsp::load_level_from_memory_with_report(
      bytes, "missing_spawn.bsp");
  assert(result);
  bool found = false;
  for (const auto &diagnostic : result->report.diagnostics) {
    found = found ||
            diagnostic.code ==
                stellar::import::bsp::DiagnosticCode::kMissingPlayerSpawn;
  }
  assert(found);
  assert(!result->report.has_errors);
}

void bsp_import_report_keeps_fatal_errors_unexpected() {
  std::vector<std::byte> bytes(8);
  auto result = stellar::import::bsp::load_level_from_memory_with_report(
      bytes, "fatal_tiny.bsp");
  assert(!result);
  assert(result.error().message.find("header") != std::string::npos);
}

void bsp_visibility_placeholder_survives_no_vis_fixture() {
  auto bytes = minimal_bsp();
  auto result =
      stellar::import::bsp::load_level_from_memory(bytes, "no_vis.bsp");
  assert(result);
  assert(!result->visibility.available);
  assert(result->visibility.compressed_pvs.empty());
  assert(result->visibility.leaves.empty());
  assert(result->visibility.cluster_count == 0);
}

void bsp_raw_lighting_and_visibility_lumps_import_without_renderer() {
  auto bytes = minimal_bsp();

  const std::size_t visibility_offset = bytes.size();
  append<std::uint8_t>(bytes, 0x01U);
  append<std::uint8_t>(bytes, 0x00U);
  set_lump(bytes, LumpIndex::kVisibility, visibility_offset, 2);

  const std::size_t marksurface_offset = bytes.size();
  append<std::uint16_t>(bytes, 0);
  set_lump(bytes, LumpIndex::kMarksurfaces, marksurface_offset, 2);

  const std::size_t leaf_offset = bytes.size();
  append_leaf(bytes, 0, 0, 1);
  set_lump(bytes, LumpIndex::kLeaves, leaf_offset, 28);

  const std::size_t lighting_offset = bytes.size();
  append<std::uint8_t>(bytes, 11U);
  append<std::uint8_t>(bytes, 22U);
  append<std::uint8_t>(bytes, 33U);
  set_lump(bytes, LumpIndex::kLighting, lighting_offset, 3);

  auto result = stellar::import::bsp::load_level_from_memory_with_report(
      bytes, "raw_lumps.bsp");
  assert(result);
  assert(result->asset.visibility.available);
  assert(result->asset.visibility.compressed_pvs.size() == 2);
  assert(result->asset.visibility.leaves.size() == 1);
  assert(result->asset.visibility.leaves[0].compressed_pvs_offset == 0);
  assert(result->asset.visibility.leaves[0].surface_indices.size() == 1);
  assert(result->asset.visibility.leaves[0].surface_indices[0] == 0);
  assert(result->asset.geometry.raw_lighting.size() == 3);
  assert(result->asset.geometry.lightmaps.empty());
}

} // namespace

int main() {
  bsp_parser_rejects_invalid_header();
  bsp_parser_rejects_unknown_version();
  bsp_parser_rejects_source_vbsp_header();
  bsp_parser_checks_lump_bounds();
  bsp30_parser_rejects_malformed_lump_size_safely();
  bsp_parser_loads_minimal_worldspawn_geometry();
  bsp_parser_preserves_legacy_bsp29_support();
  bsp30_zup_room_imports_coordinates_without_axis_swaps();
  bsp30_entity_lump_preserves_dotted_and_alias_keys();
  bsp30_developer_material_fallback_is_deterministic();
  trenchbroom_entity_matrix_imports_expected_metadata();
  trenchbroom_invalid_script_escape_fails_import();
  bsp_parser_builds_collision_triangles();
  bsp_entity_parser_preserves_key_values();
  bsp_entity_parser_rejects_malformed_entity_text();
  bsp_entities_build_player_spawn_trigger_sprite_object_collider_markers();
  bsp_script_bindings_import_to_world_metadata();
  bsp_loader_is_display_free();
  bsp_import_report_collects_missing_player_spawn_warning();
  bsp_import_report_keeps_fatal_errors_unexpected();
  bsp_visibility_placeholder_survives_no_vis_fixture();
  bsp_raw_lighting_and_visibility_lumps_import_without_renderer();
  return 0;
}
