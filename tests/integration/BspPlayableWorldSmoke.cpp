#include "stellar/import/bsp/Loader.hpp"
#include "stellar/scripting/ScriptRegistry.hpp"
#include "stellar/scripting/ScriptedWorldSession.hpp"
#include "stellar/world/CollisionValidation.hpp"
#include "stellar/world/ObjectCollider.hpp"
#include "stellar/world/RuntimeWorld.hpp"
#include "stellar/world/WorldMetadataValidation.hpp"

#include "../fixtures/BspFixture.hpp"

#include <cassert>
#include <cstddef>
#include <string_view>
#include <vector>

namespace {

stellar::server::WorldSessionConfig session_config() {
  stellar::server::WorldSessionConfig config;
  config.local_player_id = 1;
  config.movement.max_speed = 3.0F;
  config.movement.acceleration = 100.0F;
  config.movement.gravity = 0.0F;
  config.movement.terminal_fall_speed = 50.0F;
  config.movement.fixed_dt = 0.1F;
  config.movement.character.radius = 0.25F;
  config.movement.character.height = 1.0F;
  config.movement.character.skin_width = 0.0F;
  config.movement.character.ground_snap_distance = 0.0F;
  config.movement.character.max_slide_iterations = 4;
  return config;
}

stellar::scripting::ScriptRegistry script_registry() {
  stellar::scripting::ScriptRegistry registry;
  registry.set_script("scripts/door.lua",
                      "Door = {}\n"
                      "function Door.on_trigger_enter(event)\n"
                      "  stellar.emit_event('collision.set_mesh_enabled', "
                      "{mesh = 'DoorBlocker', enabled = false})\n"
                      "  stellar.emit_event('gameplay.door_open_requested', "
                      "{name = event.trigger_name})\n"
                      "end\n");
  registry.set_script("scripts/pickup.lua",
                      "PickupGem = {}\n"
                      "function PickupGem.on_object_collider_enter(event)\n"
                      "  stellar.emit_event('object_collider.set_enabled', "
                      "{id = event.collider_id, enabled = false})\n"
                      "  stellar.emit_event('gameplay.pickup_collected', "
                      "{name = event.collider_name})\n"
                      "end\n");
  return registry;
}

bool has_script_event(const stellar::scripting::ScriptedWorldFrame &frame,
                      std::string_view name) {
  for (const auto &event : frame.script_events) {
    if (event.name == name) {
      return true;
    }
  }
  return false;
}

bool has_applied_command(const stellar::scripting::ScriptedWorldFrame &frame,
                         std::string_view name) {
  for (const auto &result : frame.command_results) {
    if (result.event_name == name && result.applied) {
      return true;
    }
  }
  return false;
}

void assert_import_validation(const stellar::assets::LevelAsset &level,
                              const stellar::world::RuntimeWorld &world) {
  assert(level.level_collision.has_value());
  assert(level.level_collision->meshes.size() == 2);
  assert(level.level_collision->meshes[0].name == "worldspawn");
  assert(level.level_collision->meshes[1].name == "DoorBlocker");

  const auto collision_report =
      stellar::world::validate_level_collision(*level.level_collision);
  assert(!collision_report.has_errors);

  const auto metadata_report = stellar::world::validate_world_metadata(world);
  assert(!metadata_report.has_errors);

  assert(world.diagnostics.has_collision);
  assert(world.diagnostics.collision_mesh_count == 2);
  assert(world.diagnostics.marker_count == 5);
  assert(world.diagnostics.sprite_marker_count == 1);
  assert(world.diagnostics.object_collider_marker_count == 1);
  assert(world.diagnostics.has_player_spawn);
  assert(stellar::world::find_player_spawn(world) != nullptr);
  assert(stellar::world::find_trigger_markers(world).size() == 1);
  assert(stellar::world::find_object_collider_markers(world).size() == 1);
  assert(stellar::world::find_sprite_markers(world).size() == 1);

  const auto colliders = stellar::world::build_object_colliders(world);
  assert(colliders.size() == 1);
  assert(colliders[0].id == 4);
  assert(colliders[0].name == "PickupGem");
}

void assert_scripted_runtime_path(const stellar::world::RuntimeWorld &world) {
  auto created = stellar::scripting::ScriptedWorldSession::create(
      world, session_config(), script_registry());
  assert(created.has_value());

  const std::vector<stellar::server::PlayerCommand> move_right{{
      .player_id = 1,
      .movement = {.wish_direction = {1.0F, 0.0F, 0.0F}},
  }};

  bool saw_trigger = false;
  bool saw_collision_toggle = false;
  bool saw_object_enter = false;
  bool saw_object_disable = false;
  float final_x = -1.0F;

  for (int i = 0; i < 10; ++i) {
    const auto frame = created->tick(move_right);
    assert(frame.script_errors.empty());
    assert(!frame.snapshot.players.empty());
    final_x = frame.snapshot.players[0].position[0];

    if (has_script_event(frame, "gameplay.door_open_requested")) {
      saw_trigger = true;
    }
    if (has_applied_command(frame, "collision.set_mesh_enabled")) {
      saw_collision_toggle = true;
    }
    if (has_script_event(frame, "gameplay.pickup_collected")) {
      saw_object_enter = true;
    }
    if (has_applied_command(frame, "object_collider.set_enabled")) {
      saw_object_disable = true;
    }
  }

  assert(saw_trigger);
  assert(saw_collision_toggle);
  assert(saw_object_enter);
  assert(saw_object_disable);
  assert(final_x > 0.75F);
}

} // namespace

int main() {
  const auto bytes = stellar::tests::fixtures::build_bsp_playable_fixture();
  const auto level =
      stellar::import::bsp::load_level_from_memory(bytes, "bsp_playable.bsp");
  assert(level.has_value());

  const auto world = stellar::world::build_runtime_world(*level);
  assert_import_validation(*level, world);
  assert_scripted_runtime_path(world);
  return 0;
}
