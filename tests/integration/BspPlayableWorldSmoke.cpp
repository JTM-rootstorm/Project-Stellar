#include "stellar/import/bsp/Loader.hpp"
#include "stellar/scripting/ScriptRegistry.hpp"
#include "stellar/scripting/ScriptedWorldSession.hpp"
#include "stellar/server/WorldSession.hpp"
#include "stellar/world/CollisionValidation.hpp"
#include "stellar/world/ObjectCollider.hpp"
#include "stellar/world/RuntimeWorld.hpp"
#include "stellar/world/WorldMetadataValidation.hpp"

#include "../fixtures/BspFixture.hpp"

#include <cassert>
#include <cstddef>
#include <cmath>
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

void assert_same_snapshot(const stellar::server::WorldSnapshot &a,
                          const stellar::server::WorldSnapshot &b) {
  assert(a.tick == b.tick);
  assert(a.players.size() == b.players.size());
  for (std::size_t i = 0; i < a.players.size(); ++i) {
    assert(a.players[i].player_id == b.players[i].player_id);
    assert(a.players[i].position == b.players[i].position);
    assert(a.players[i].velocity == b.players[i].velocity);
    assert(a.players[i].rotation == b.players[i].rotation);
    assert(a.players[i].grounded == b.players[i].grounded);
  }
  assert(a.trigger_events.size() == b.trigger_events.size());
  assert(a.object_collider_events.size() == b.object_collider_events.size());
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
    if (has_applied_command(frame, "gameplay.collect_pickup")) {
      saw_object_disable = true;
    }
  }

  assert(saw_trigger);
  assert(saw_collision_toggle);
  assert(saw_object_enter);
  assert(saw_object_disable);
  assert(final_x > 0.75F);
}

void assert_gameplay_room_import(const stellar::assets::LevelAsset &level,
                                 const stellar::world::RuntimeWorld &world) {
  assert(level.level_collision.has_value());
  assert(level.level_collision->meshes.size() == 1);
  assert(level.level_collision->meshes[0].name == "worldspawn");
  assert(level.level_collision->meshes[0].triangles.size() == 12);
  assert(level.geometry.materials.size() == 3);
  assert(level.geometry.materials[0].source_name == "dev/grid_32");
  assert(level.geometry.materials[1].source_name == "dev/grid_64");
  assert(level.geometry.materials[2].source_name == "dev/wall_96");

  const auto collision_report =
      stellar::world::validate_level_collision(*level.level_collision);
  assert(!collision_report.has_errors);
  const auto metadata_report = stellar::world::validate_world_metadata(world);
  assert(!metadata_report.has_errors);

  assert(world.diagnostics.has_collision);
  assert(world.diagnostics.collision_mesh_count == 1);
  assert(world.diagnostics.marker_count == 4);
  assert(world.diagnostics.sprite_marker_count == 1);
  assert(world.diagnostics.object_collider_marker_count == 1);
  assert(world.diagnostics.has_player_spawn);
  assert(stellar::world::find_player_spawn(world) != nullptr);
  assert(stellar::world::find_player_spawn(world)->position ==
         (std::array<float, 3>{0.0F, 0.0F, 36.0F}));
  assert(stellar::world::find_trigger_markers(world).size() == 1);
  assert(stellar::world::find_object_collider_markers(world).size() == 1);
  assert(stellar::world::find_sprite_markers(world).size() == 1);
}

void assert_gameplay_room_authority_session_path(const stellar::world::RuntimeWorld &world) {
  auto config = session_config();
  config.local_player_id = 11;

  stellar::server::WorldSession runtime(world, config);
  stellar::server::WorldSession deterministic_a(world, config);
  stellar::server::WorldSession deterministic_b(world, config);
  const std::vector<stellar::server::PlayerCommand> forward_right{{
      .player_id = 11,
      .movement = {.wish_direction = {1.0F, 1.0F, 0.0F}},
  }};

  auto previous = runtime.snapshot();
  assert(previous.players.size() == 1);
  assert(previous.players[0].position == (std::array<float, 3>{0.0F, 0.0F, 36.0F}));

  for (int i = 0; i < 8; ++i) {
    previous = runtime.tick(forward_right);
    assert(previous.players.size() == 1);

    const auto a = deterministic_a.tick(forward_right);
    const auto b = deterministic_b.tick(forward_right);
    assert_same_snapshot(a, b);
  }

  const auto &moved_player = previous.players[0];
  assert(moved_player.position[0] > 0.0F);
  assert(std::fabs(moved_player.position[1]) > 0.0F);
  assert(moved_player.position[2] >= 16.0F);
  assert(moved_player.position[2] <= 80.0F);

  const std::vector<stellar::server::PlayerCommand> right{{
      .player_id = 11,
      .movement = {.wish_direction = {1.0F, 0.0F, 0.0F}},
  }};
  for (int i = 0; i < 120; ++i) {
    previous = runtime.tick(right);
  }
  assert(previous.players[0].position[0] <= 80.01F);
  assert(previous.players[0].position[0] >= -80.01F);

  const std::vector<stellar::server::PlayerCommand> left{{
      .player_id = 11,
      .movement = {.wish_direction = {-1.0F, 0.0F, 0.0F}},
  }};
  for (int i = 0; i < 240; ++i) {
    previous = runtime.tick(left);
  }
  assert(previous.players[0].position[0] >= -80.01F);
  assert(previous.players[0].position[0] <= 80.01F);

  const std::vector<stellar::server::PlayerCommand> forward{{
      .player_id = 11,
      .movement = {.wish_direction = {0.0F, 1.0F, 0.0F}},
  }};
  for (int i = 0; i < 240; ++i) {
    previous = runtime.tick(forward);
  }
  assert(previous.players[0].position[1] >= -80.01F);
  assert(previous.players[0].position[1] <= 80.01F);
}

void assert_gameplay_room_path() {
  const auto bytes = stellar::tests::fixtures::build_bsp_gameplay_room_fixture();
  const auto level =
      stellar::import::bsp::load_level_from_memory(bytes, "gameplay_room.bsp");
  assert(level.has_value());

  const auto world = stellar::world::build_runtime_world(*level);
  assert_gameplay_room_import(*level, world);
  assert_gameplay_room_authority_session_path(world);
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
  assert_gameplay_room_path();
  return 0;
}
