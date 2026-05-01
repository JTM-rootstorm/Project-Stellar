#include "../../../src/import/bsp/BspBinary.hpp"

#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace {

stellar::import::bsp::detail::Entity entity(
    std::initializer_list<stellar::import::bsp::detail::EntityKeyValue> pairs) {
  stellar::import::bsp::detail::Entity result;
  result.pairs.assign(pairs.begin(), pairs.end());
  return result;
}

stellar::import::bsp::detail::BspMap map_with_models() {
  stellar::import::bsp::detail::BspMap map;
  map.version = stellar::import::bsp::detail::kBspVersionQuake;
  map.models.push_back(stellar::import::bsp::detail::Model{
      .mins = {-128.0F, -128.0F, -16.0F},
      .maxs = {128.0F, 128.0F, 64.0F},
      .first_face = 0,
      .face_count = 0});
  map.models.push_back(stellar::import::bsp::detail::Model{.mins = {-8.0F, -16.0F, 0.0F},
                                                           .maxs = {8.0F, 16.0F, 32.0F},
                                                           .first_face = 0,
                                                           .face_count = 0});
  map.models.push_back(stellar::import::bsp::detail::Model{.mins = {32.0F, 40.0F, 4.0F},
                                                           .maxs = {48.0F, 72.0F, 20.0F},
                                                           .first_face = 0,
                                                           .face_count = 0});
  return map;
}

bool has_diagnostic(const stellar::import::bsp::ImportReport &report,
                    std::string_view fragment) {
  for (const auto &diagnostic : report.diagnostics) {
    if (diagnostic.message.find(fragment) != std::string::npos) {
      return true;
    }
  }
  return false;
}

void documented_entities_map_to_world_markers_and_preserve_properties() {
  std::vector<stellar::import::bsp::detail::Entity> entities;
  entities.push_back(entity({{"classname", "worldspawn"}}));
  entities.push_back(entity({{"classname", "info_player_start"},
                             {"targetname", "Start"},
                             {"origin", "1 2 3"},
                             {"angle", "90"}}));
  entities.push_back(entity({{"classname", "info_stellar_spawn"},
                             {"targetname", "EnemySpawn"},
                             {"archetype", "enemy.grunt"},
                             {"origin", "4 5 6"},
                             {"stellar.script", "scripts/spawn.lua"},
                             {"stellar.table", "Spawner"}}));
  entities.push_back(entity({{"classname", "trigger_multiple"},
                             {"targetname", "DoorTrigger"},
                             {"model", "*1"},
                             {"stellar.script", "scripts/door.lua"},
                             {"stellar.table", "Door"},
                             {"stellar.once", "false"}}));
  entities.push_back(entity({{"classname", "stellar_sprite"},
                             {"targetname", "Torch"},
                             {"origin", "7 8 9"},
                             {"stellar.sprite", "torch"},
                             {"stellar.size", "1.5 2.5"},
                             {"stellar.alpha", "blend"}}));
  entities.push_back(entity({{"classname", "stellar_object_collider"},
                             {"targetname", "Pickup"},
                             {"model", "*2"},
                             {"stellar.collider", "object"},
                             {"stellar.script", "scripts/pickup.lua"},
                             {"stellar.table", "Pickup"},
                             {"stellar.enabled", "yes"}}));

  stellar::import::bsp::ImportReport report;
  auto level = stellar::import::bsp::detail::build_level_asset(
      map_with_models(), std::move(entities), "authoring.bsp", {}, &report);
  assert(level.has_value());
  assert(report.diagnostics.empty());
  assert(level->world_metadata.markers.size() == 5);

  const auto &spawn = level->world_metadata.markers[0];
  assert(spawn.type == stellar::assets::WorldMarkerType::kPlayerSpawn);
  assert(spawn.position[0] == 1.0F && spawn.position[1] == 2.0F && spawn.position[2] == 3.0F);
  assert(spawn.properties.size() == 4);
  assert(spawn.properties[3].key == "angle");
  assert(spawn.properties[3].value == "90");

  const auto &entity_spawn = level->world_metadata.markers[1];
  assert(entity_spawn.type == stellar::assets::WorldMarkerType::kEntitySpawn);
  assert(entity_spawn.archetype == "enemy.grunt");
  assert(entity_spawn.script.has_value());

  const auto &trigger = level->world_metadata.markers[2];
  assert(trigger.type == stellar::assets::WorldMarkerType::kTrigger);
  assert(trigger.position[0] == 0.0F && trigger.position[1] == 0.0F && trigger.position[2] == 16.0F);
  assert(trigger.scale[0] == 8.0F && trigger.scale[1] == 16.0F && trigger.scale[2] == 16.0F);
  assert(trigger.script.has_value());

  const auto &sprite = level->world_metadata.markers[3];
  assert(sprite.type == stellar::assets::WorldMarkerType::kSprite);
  assert(sprite.archetype == "torch");
  assert(sprite.scale[0] == 1.5F && sprite.scale[1] == 2.5F && sprite.scale[2] == 1.0F);

  const auto &collider = level->world_metadata.markers[4];
  assert(collider.type == stellar::assets::WorldMarkerType::kObjectCollider);
  assert(collider.position[0] == 40.0F && collider.position[1] == 56.0F && collider.position[2] == 12.0F);
  assert(collider.scale[0] == 8.0F && collider.scale[1] == 16.0F && collider.scale[2] == 8.0F);
  assert(collider.script.has_value());
}

void point_authored_trigger_and_object_collider_use_origin_extents_fallback() {
  std::vector<stellar::import::bsp::detail::Entity> entities;
  entities.push_back(entity({{"classname", "worldspawn"}}));
  entities.push_back(entity({{"classname", "info_player_start"}, {"origin", "0 0 0"}}));
  entities.push_back(entity({{"classname", "trigger_stellar"},
                             {"targetname", "PointTrigger"},
                             {"origin", "10 20 30"},
                             {"stellar.extents", "3 4 5"}}));
  entities.push_back(entity({{"classname", "stellar_object_collider"},
                             {"targetname", "PointCollider"},
                             {"origin", "40 50 60"},
                             {"stellar.extents", "6 7 8"}}));

  stellar::import::bsp::ImportReport report;
  auto level = stellar::import::bsp::detail::build_level_asset(
      map_with_models(), std::move(entities), "point_volumes.bsp", {}, &report);
  assert(level.has_value());
  assert(report.diagnostics.empty());
  assert(level->world_metadata.markers[1].position[0] == 10.0F);
  assert(level->world_metadata.markers[1].scale[2] == 5.0F);
  assert(level->world_metadata.markers[2].position[1] == 50.0F);
  assert(level->world_metadata.markers[2].scale[0] == 6.0F);
}

void malformed_vectors_and_booleans_emit_diagnostics() {
  std::vector<stellar::import::bsp::detail::Entity> entities;
  entities.push_back(entity({{"classname", "worldspawn"}}));
  entities.push_back(entity({{"classname", "info_player_start"}, {"origin", "bad 0 0"}}));
  entities.push_back(entity({{"classname", "trigger_stellar"},
                             {"targetname", "BadTrigger"},
                             {"origin", "1 2"},
                             {"stellar.extents", "1 2 nope"},
                             {"stellar.once", "maybe"}}));
  entities.push_back(entity({{"classname", "stellar_object_collider"},
                             {"targetname", "BadCollider"},
                             {"origin", "0 0 0"},
                             {"stellar.extents", "1 2 3 4"},
                             {"stellar.enabled", "enabled"}}));
  entities.push_back(entity({{"classname", "stellar_sprite"},
                             {"targetname", "BadSprite"},
                             {"origin", "0 0 0"},
                             {"stellar.sprite", "bad"},
                             {"stellar.size", "wide tall"}}));

  stellar::import::bsp::ImportReport report;
  auto level = stellar::import::bsp::detail::build_level_asset(
      map_with_models(), std::move(entities), "bad_authoring.bsp", {}, &report);
  assert(level.has_value());
  assert(has_diagnostic(report, "malformed origin vector"));
  assert(has_diagnostic(report, "malformed stellar.extents vector"));
  assert(has_diagnostic(report, "malformed stellar.once boolean"));
  assert(has_diagnostic(report, "malformed stellar.enabled boolean"));
  assert(has_diagnostic(report, "malformed stellar.size vector"));
}

void script_path_escape_is_rejected() {
  std::vector<stellar::import::bsp::detail::Entity> entities;
  entities.push_back(entity({{"classname", "worldspawn"}}));
  entities.push_back(entity({{"classname", "info_player_start"}, {"origin", "0 0 0"}}));
  entities.push_back(entity({{"classname", "trigger_multiple"},
                             {"targetname", "BadScript"},
                             {"origin", "0 0 0"},
                             {"stellar.extents", "1 1 1"},
                             {"stellar.script", "../escape.lua"},
                             {"stellar.table", "Bad"}}));

  stellar::import::bsp::ImportReport report;
  auto level = stellar::import::bsp::detail::build_level_asset(
      map_with_models(), std::move(entities), "script_escape.bsp", {}, &report);
  assert(!level.has_value());
  assert(level.error().message.find("parent path escape") != std::string::npos);
}

void unsupported_sprite_script_binding_is_diagnosed_and_ignored() {
  std::vector<stellar::import::bsp::detail::Entity> entities;
  entities.push_back(entity({{"classname", "worldspawn"}}));
  entities.push_back(entity({{"classname", "info_player_start"}, {"origin", "0 0 0"}}));
  entities.push_back(entity({{"classname", "stellar_sprite"},
                             {"targetname", "ScriptedSprite"},
                             {"origin", "0 0 0"},
                             {"stellar.sprite", "spark"},
                             {"stellar.script", "scripts/sprite.lua"},
                             {"stellar.table", "Sprite"}}));

  stellar::import::bsp::ImportReport report;
  auto level = stellar::import::bsp::detail::build_level_asset(
      map_with_models(), std::move(entities), "sprite_script.bsp", {}, &report);
  assert(level.has_value());
  assert(has_diagnostic(report, "sprite script bindings are not supported"));
  assert(level->world_metadata.markers[1].type == stellar::assets::WorldMarkerType::kSprite);
  assert(!level->world_metadata.markers[1].script.has_value());
  assert(level->world_metadata.markers[1].properties.size() == 6);
}

} // namespace

int main() {
  documented_entities_map_to_world_markers_and_preserve_properties();
  point_authored_trigger_and_object_collider_use_origin_extents_fallback();
  malformed_vectors_and_booleans_emit_diagnostics();
  script_path_escape_is_rejected();
  unsupported_sprite_script_binding_is_diagnosed_and_ignored();
  return 0;
}
