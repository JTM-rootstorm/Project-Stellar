#include "stellar/world/TriggerSystem.hpp"

#include "stellar/world/RuntimeWorld.hpp"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace {

stellar::assets::WorldMarker make_marker(stellar::assets::WorldMarkerType type,
                                         std::string name) {
    stellar::assets::WorldMarker marker;
    marker.type = type;
    marker.name = std::move(name);
    marker.position = {1.0F, 2.0F, 3.0F};
    marker.scale = {2.0F, -3.0F, 4.0F};
    return marker;
}

void verify_metadata_trigger_marker_becomes_volume() {
    stellar::assets::WorldMetadataAsset metadata;
    metadata.markers.push_back(make_marker(stellar::assets::WorldMarkerType::kTrigger, "Door"));

    const auto triggers = stellar::world::build_trigger_volumes(metadata);

    assert(triggers.size() == 1);
    assert(triggers[0].name == "Door");
    assert((triggers[0].center == std::array<float, 3>{1.0F, 2.0F, 3.0F}));
    assert((triggers[0].half_extents == std::array<float, 3>{2.0F, 3.0F, 4.0F}));
}

void verify_non_trigger_markers_are_ignored() {
    stellar::assets::WorldMetadataAsset metadata;
    metadata.markers.push_back(make_marker(stellar::assets::WorldMarkerType::kPlayerSpawn,
                                           "Player"));
    metadata.markers.push_back(make_marker(stellar::assets::WorldMarkerType::kSprite, "Torch"));

    const auto triggers = stellar::world::build_trigger_volumes(metadata);
    assert(triggers.empty());
}

stellar::world::TriggerSystem make_single_trigger_system() {
    stellar::world::TriggerVolume trigger;
    trigger.name = "Box";
    trigger.center = {0.0F, 0.0F, 0.0F};
    trigger.half_extents = {1.0F, 1.0F, 1.0F};

    stellar::world::TriggerSystem system;
    const std::vector<stellar::world::TriggerVolume> triggers{trigger};
    system.set_triggers(triggers);
    return system;
}

void verify_sphere_outside_aabb_does_not_overlap() {
    auto system = make_single_trigger_system();
    const auto overlaps = system.update_sphere({3.1F, 0.0F, 0.0F}, 1.0F);
    assert(overlaps.empty());
}

void verify_sphere_touching_aabb_overlaps_inclusively() {
    auto system = make_single_trigger_system();
    const auto overlaps = system.update_sphere({2.0F, 0.0F, 0.0F}, 1.0F);
    assert(overlaps.size() == 1);
    assert(overlaps[0].name == "Box");
    assert(overlaps[0].entered);
    assert(!overlaps[0].stayed);
    assert(!overlaps[0].exited);
}

void verify_sphere_inside_aabb_overlaps() {
    auto system = make_single_trigger_system();
    const auto overlaps = system.update_sphere({0.0F, 0.0F, 0.0F}, 0.25F);
    assert(overlaps.size() == 1);
    assert(overlaps[0].entered);
}

void verify_enter_stay_exit_and_reenter() {
    auto system = make_single_trigger_system();

    auto overlaps = system.update_sphere({0.0F, 0.0F, 0.0F}, 0.25F);
    assert(overlaps.size() == 1);
    assert(overlaps[0].entered);

    overlaps = system.update_sphere({0.5F, 0.0F, 0.0F}, 0.25F);
    assert(overlaps.size() == 1);
    assert(overlaps[0].stayed);

    overlaps = system.update_sphere({4.0F, 0.0F, 0.0F}, 0.25F);
    assert(overlaps.size() == 1);
    assert(overlaps[0].exited);

    overlaps = system.update_sphere({0.0F, 0.0F, 0.0F}, 0.25F);
    assert(overlaps.size() == 1);
    assert(overlaps[0].entered);
}

void verify_multiple_trigger_ordering_is_deterministic() {
    stellar::assets::WorldMetadataAsset metadata;
    auto beta = make_marker(stellar::assets::WorldMarkerType::kTrigger, "Beta");
    beta.position = {10.0F, 0.0F, 0.0F};
    beta.scale = {1.0F, 1.0F, 1.0F};
    auto alpha = make_marker(stellar::assets::WorldMarkerType::kTrigger, "Alpha");
    alpha.position = {0.0F, 0.0F, 0.0F};
    alpha.scale = {1.0F, 1.0F, 1.0F};
    metadata.markers = {beta, alpha};

    const auto triggers = stellar::world::build_trigger_volumes(metadata);
    assert(triggers.size() == 2);
    assert(triggers[0].name == "Alpha");
    assert(triggers[1].name == "Beta");

    stellar::world::TriggerSystem system;
    system.set_triggers(triggers);
    const auto overlaps = system.update_sphere({5.0F, 0.0F, 0.0F}, 6.0F);
    assert(overlaps.size() == 2);
    assert(overlaps[0].name == "Alpha");
    assert(overlaps[1].name == "Beta");
}

void verify_empty_trigger_system_returns_no_events() {
    stellar::world::TriggerSystem system;
    const auto overlaps = system.update_sphere({0.0F, 0.0F, 0.0F}, 100.0F);
    assert(overlaps.empty());
}

void verify_runtime_world_helper_uses_copied_metadata() {
    stellar::assets::SceneAsset scene;
    scene.world_metadata.markers.push_back(
        make_marker(stellar::assets::WorldMarkerType::kTrigger, "Runtime"));
    const auto world = stellar::world::build_runtime_world(scene);

    const auto triggers = stellar::world::build_trigger_volumes(world);
    assert(triggers.size() == 1);
    assert(triggers[0].name == "Runtime");
}

} // namespace

int main() {
    verify_metadata_trigger_marker_becomes_volume();
    verify_non_trigger_markers_are_ignored();
    verify_sphere_outside_aabb_does_not_overlap();
    verify_sphere_touching_aabb_overlaps_inclusively();
    verify_sphere_inside_aabb_overlaps();
    verify_enter_stay_exit_and_reenter();
    verify_multiple_trigger_ordering_is_deterministic();
    verify_empty_trigger_system_returns_no_events();
    verify_runtime_world_helper_uses_copied_metadata();
    return 0;
}
