#include "stellar/server/MovementTriggerIntegration.hpp"

#include <array>
#include <cassert>
#include <string>
#include <utility>

namespace {

using Vec3 = std::array<float, 3>;

stellar::assets::WorldMarker trigger_marker(std::string name, Vec3 position, Vec3 scale) {
    stellar::assets::WorldMarker marker;
    marker.type = stellar::assets::WorldMarkerType::kTrigger;
    marker.name = std::move(name);
    marker.position = position;
    marker.scale = scale;
    return marker;
}

stellar::assets::SceneAsset scene_with_triggers(
    std::initializer_list<stellar::assets::WorldMarker> markers) {
    stellar::assets::SceneAsset scene;
    scene.world_metadata.markers.assign(markers.begin(), markers.end());
    return scene;
}

stellar::server::MovementSimulationConfig trigger_test_config() {
    stellar::server::MovementSimulationConfig config;
    config.max_speed = 10.0F;
    config.acceleration = 100.0F;
    config.gravity = 0.0F;
    config.fixed_dt = 0.1F;
    config.character.radius = 0.25F;
    config.character.height = 1.0F;
    config.character.skin_width = 0.0F;
    config.character.ground_snap_distance = 0.0F;
    return config;
}

void reset_from_world_builds_trigger_volumes() {
    const auto scene = scene_with_triggers({trigger_marker("Door", {0.0F, 0.0F, 0.0F},
                                                            {1.0F, 1.0F, 1.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::MovementTriggerTracker tracker;

    tracker.reset_from_world(world);
    const auto events = tracker.update({0.0F, 0.0F, 0.0F}, 0.1F);

    assert(events.size() == 1);
    assert(events[0].trigger_name == "Door");
    assert(events[0].entered);
    assert(!events[0].stayed);
    assert(!events[0].exited);
}

void moving_into_trigger_emits_enter() {
    const auto scene = scene_with_triggers({trigger_marker("Box", {0.0F, 0.0F, 0.0F},
                                                           {1.0F, 1.0F, 1.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::MovementTriggerTracker tracker;
    tracker.reset_from_world(world);

    assert(tracker.update({3.0F, 0.0F, 0.0F}, 0.25F).empty());
    const auto events = tracker.update({0.0F, 0.0F, 0.0F}, 0.25F);

    assert(events.size() == 1);
    assert(events[0].entered);
}

void remaining_inside_trigger_emits_stay() {
    const auto scene = scene_with_triggers({trigger_marker("Box", {0.0F, 0.0F, 0.0F},
                                                           {1.0F, 1.0F, 1.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::MovementTriggerTracker tracker;
    tracker.reset_from_world(world);

    assert(tracker.update({0.0F, 0.0F, 0.0F}, 0.25F).size() == 1);
    const auto events = tracker.update({0.25F, 0.0F, 0.0F}, 0.25F);

    assert(events.size() == 1);
    assert(!events[0].entered);
    assert(events[0].stayed);
    assert(!events[0].exited);
}

void moving_out_of_trigger_emits_exit() {
    const auto scene = scene_with_triggers({trigger_marker("Box", {0.0F, 0.0F, 0.0F},
                                                           {1.0F, 1.0F, 1.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::MovementTriggerTracker tracker;
    tracker.reset_from_world(world);

    assert(tracker.update({0.0F, 0.0F, 0.0F}, 0.25F).size() == 1);
    const auto events = tracker.update({4.0F, 0.0F, 0.0F}, 0.25F);

    assert(events.size() == 1);
    assert(!events[0].entered);
    assert(!events[0].stayed);
    assert(events[0].exited);
}

void multiple_triggers_emit_deterministic_order() {
    const auto scene = scene_with_triggers({trigger_marker("Beta", {0.0F, 0.0F, 0.0F},
                                                            {1.0F, 1.0F, 1.0F}),
                                            trigger_marker("Alpha", {0.0F, 0.0F, 0.0F},
                                                           {1.0F, 1.0F, 1.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::MovementTriggerTracker tracker;
    tracker.reset_from_world(world);

    const auto events = tracker.update({0.0F, 0.0F, 0.0F}, 0.25F);

    assert(events.size() == 2);
    assert(events[0].trigger_name == "Alpha");
    assert(events[1].trigger_name == "Beta");
}

void reset_clears_previous_overlap_state() {
    const auto scene = scene_with_triggers({trigger_marker("Box", {0.0F, 0.0F, 0.0F},
                                                           {1.0F, 1.0F, 1.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::MovementTriggerTracker tracker;
    tracker.reset_from_world(world);

    assert(tracker.update({0.0F, 0.0F, 0.0F}, 0.25F)[0].entered);
    assert(tracker.update({0.0F, 0.0F, 0.0F}, 0.25F)[0].stayed);

    tracker.reset_from_world(world);
    const auto events = tracker.update({0.0F, 0.0F, 0.0F}, 0.25F);

    assert(events.size() == 1);
    assert(events[0].entered);
    assert(!events[0].stayed);
}

void world_without_triggers_emits_no_events() {
    const stellar::assets::SceneAsset scene;
    const auto world = stellar::world::build_runtime_world(scene);
    stellar::server::MovementTriggerTracker tracker;
    tracker.reset_from_world(world);

    const auto events = tracker.update({0.0F, 0.0F, 0.0F}, 100.0F);

    assert(events.empty());
}

void movement_tick_then_trigger_update_uses_authoritative_final_position() {
    const auto scene = scene_with_triggers({trigger_marker("Finish", {1.0F, 0.0F, 0.0F},
                                                            {0.0F, 0.5F, 0.5F})});
    const auto world = stellar::world::build_runtime_world(scene);
    auto config = trigger_test_config();
    stellar::server::MovementState previous;
    previous.grounded = true;
    stellar::server::MovementTriggerTracker tracker;
    tracker.reset_from_world(world);

    const auto result = stellar::server::simulate_movement_tick_and_update_triggers(
        world, previous, {.wish_direction = {1.0F, 0.0F, 0.0F}}, config, tracker);

    assert(result.movement.state.position[0] > 0.9F);
    assert(result.trigger_events.size() == 1);
    assert(result.trigger_events[0].trigger_name == "Finish");
    assert(result.trigger_events[0].entered);
}

void trigger_update_uses_radius_from_config_or_documented_policy() {
    const auto scene = scene_with_triggers({trigger_marker("Touch", {2.0F, 0.0F, 0.0F},
                                                           {0.0F, 0.0F, 0.0F})});
    const auto world = stellar::world::build_runtime_world(scene);
    auto config = trigger_test_config();
    config.character.radius = 2.0F;
    stellar::server::MovementState previous;
    previous.grounded = true;
    stellar::server::MovementTriggerTracker tracker;
    tracker.reset_from_world(world);

    const auto result = stellar::server::simulate_movement_tick_and_update_triggers(
        world, previous, {}, config, tracker);

    assert(result.trigger_events.size() == 1);
    assert(result.trigger_events[0].trigger_name == "Touch");
    assert(result.trigger_events[0].entered);
}

void client_input_cannot_directly_trigger_without_authoritative_movement() {
    const auto scene = scene_with_triggers({trigger_marker("Far", {10.0F, 0.0F, 0.0F},
                                                         {0.25F, 0.25F, 0.25F})});
    const auto world = stellar::world::build_runtime_world(scene);
    auto config = trigger_test_config();
    config.max_speed = 1.0F;
    config.acceleration = 1.0F;
    stellar::server::MovementState previous;
    previous.grounded = true;
    stellar::server::MovementTriggerTracker tracker;
    tracker.reset_from_world(world);

    const auto result = stellar::server::simulate_movement_tick_and_update_triggers(
        world, previous, {.wish_direction = {1000.0F, 0.0F, 0.0F}}, config, tracker);

    assert(result.movement.command_was_sanitized);
    assert(result.movement.state.position[0] < 1.0F);
    assert(result.trigger_events.empty());
}

} // namespace

int main() {
    reset_from_world_builds_trigger_volumes();
    moving_into_trigger_emits_enter();
    remaining_inside_trigger_emits_stay();
    moving_out_of_trigger_emits_exit();
    multiple_triggers_emit_deterministic_order();
    reset_clears_previous_overlap_state();
    world_without_triggers_emits_no_events();
    movement_tick_then_trigger_update_uses_authoritative_final_position();
    trigger_update_uses_radius_from_config_or_documented_policy();
    client_input_cannot_directly_trigger_without_authoritative_movement();
    return 0;
}
