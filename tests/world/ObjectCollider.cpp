#include "stellar/world/ObjectCollider.hpp"

#include "stellar/world/RuntimeWorld.hpp"

#include <cassert>
#include <limits>
#include <vector>

namespace {

stellar::world::TriggerCapsule player_capsule(std::array<float, 3> center = {}) {
    return {.center = center, .up = {0.0F, 0.0F, 1.0F}, .radius = 0.5F, .height = 2.0F};
}

stellar::world::ObjectCollider make_collider(std::uint32_t id,
                                             stellar::world::ObjectColliderShapeType type,
                                             std::array<float, 3> center) {
    stellar::world::ObjectCollider collider;
    collider.id = id;
    collider.name = "Collider";
    collider.shape.type = type;
    collider.shape.center = center;
    collider.shape.radius = 0.5F;
    collider.shape.height = 2.0F;
    collider.shape.half_extents = {0.5F, 0.5F, 0.5F};
    return collider;
}

void empty_object_collider_system_returns_no_events() {
    stellar::world::ObjectColliderSystem system;
    assert(system.update_player_capsule(player_capsule()).empty());
}

void capsule_enters_sphere_collider() {
    auto collider = make_collider(7, stellar::world::ObjectColliderShapeType::kSphere,
                                  {1.0F, 0.0F, 0.0F});
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{collider};
    system.set_colliders(colliders);

    const auto events = system.update_player_capsule(player_capsule());
    assert(events.size() == 1);
    assert(events[0].collider_id == 7);
    assert(events[0].entered);
}

void capsule_enters_aabb_collider() {
    auto collider = make_collider(8, stellar::world::ObjectColliderShapeType::kAabb,
                                  {1.0F, 0.0F, 0.0F});
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{collider};
    system.set_colliders(colliders);

    const auto events = system.update_player_capsule(player_capsule());
    assert(events.size() == 1);
    assert(events[0].collider_id == 8);
    assert(events[0].entered);
}

void capsule_enters_capsule_collider_if_supported() {
    auto collider = make_collider(9, stellar::world::ObjectColliderShapeType::kCapsule,
                                  {1.0F, 0.0F, 0.0F});
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{collider};
    system.set_colliders(colliders);

    const auto events = system.update_player_capsule(player_capsule());
    assert(events.size() == 1);
    assert(events[0].collider_id == 9);
    assert(events[0].entered);
}

void disabled_collider_does_not_emit_events() {
    auto collider = make_collider(10, stellar::world::ObjectColliderShapeType::kSphere,
                                  {0.0F, 0.0F, 0.0F});
    collider.enabled = false;
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{collider};
    system.set_colliders(colliders);

    assert(system.update_player_capsule(player_capsule()).empty());
}

void enter_stay_exit_are_deterministic() {
    auto collider = make_collider(11, stellar::world::ObjectColliderShapeType::kSphere,
                                  {0.0F, 0.0F, 0.0F});
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{collider};
    system.set_colliders(colliders);

    auto events = system.update_player_capsule(player_capsule());
    assert(events.size() == 1);
    assert(events[0].entered && !events[0].stayed && !events[0].exited);

    events = system.update_player_capsule(player_capsule({0.1F, 0.0F, 0.0F}));
    assert(events.size() == 1);
    assert(!events[0].entered && events[0].stayed && !events[0].exited);

    events = system.update_player_capsule(player_capsule({10.0F, 0.0F, 0.0F}));
    assert(events.size() == 1);
    assert(!events[0].entered && !events[0].stayed && events[0].exited);
}

void duplicate_names_do_not_break_id_based_events() {
    auto first = make_collider(21, stellar::world::ObjectColliderShapeType::kSphere,
                               {0.0F, 0.0F, 0.0F});
    auto second = make_collider(22, stellar::world::ObjectColliderShapeType::kAabb,
                                {0.0F, 0.0F, 0.0F});
    first.name = "Duplicate";
    second.name = "Duplicate";
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{first, second};
    system.set_colliders(colliders);

    const auto events = system.update_player_capsule(player_capsule());
    assert(events.size() == 2);
    assert(events[0].name == "Duplicate");
    assert(events[1].name == "Duplicate");
    assert(events[0].collider_id == 21);
    assert(events[1].collider_id == 22);
}

void non_finite_shape_data_is_sanitized_or_ignored_by_policy() {
    const float inf = std::numeric_limits<float>::infinity();
    auto ignored = make_collider(31, stellar::world::ObjectColliderShapeType::kSphere,
                                 {inf, 0.0F, 0.0F});
    auto sanitized_radius = make_collider(32, stellar::world::ObjectColliderShapeType::kSphere,
                                          {0.5F, 0.0F, 0.0F});
    sanitized_radius.shape.radius = -1.0F;
    auto sanitized_aabb = make_collider(33, stellar::world::ObjectColliderShapeType::kAabb,
                                        {0.0F, 0.0F, 0.0F});
    sanitized_aabb.shape.half_extents = {inf, 0.25F, 0.25F};
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{ignored, sanitized_radius, sanitized_aabb};
    system.set_colliders(colliders);

    const auto events = system.update_player_capsule(player_capsule());
    assert(events.size() == 2);
    assert(events[0].collider_id == 32);
    assert(events[1].collider_id == 33);
}

void object_collider_order_is_stable() {
    auto beta = make_collider(41, stellar::world::ObjectColliderShapeType::kSphere,
                              {0.0F, 0.0F, 0.0F});
    auto alpha = make_collider(42, stellar::world::ObjectColliderShapeType::kSphere,
                               {0.0F, 0.0F, 0.0F});
    beta.name = "Beta";
    alpha.name = "Alpha";
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{beta, alpha};
    system.set_colliders(colliders);

    assert(system.colliders().size() == 2);
    assert(system.colliders()[0].name == "Beta");
    assert(system.colliders()[1].name == "Alpha");

    const auto events = system.update_player_capsule(player_capsule());
    assert(events.size() == 2);
    assert(events[0].collider_id == 41);
    assert(events[1].collider_id == 42);
}

void set_colliders_is_hard_reset_and_does_not_emit_exit() {
    auto collider = make_collider(51, stellar::world::ObjectColliderShapeType::kSphere,
                                   {0.0F, 0.0F, 0.0F});
    stellar::world::ObjectColliderSystem system;
    std::vector colliders{collider};
    system.set_colliders(colliders);
    assert(system.update_player_capsule(player_capsule()).size() == 1);

    colliders[0].enabled = false;
    system.set_colliders(colliders);
    assert(system.update_player_capsule(player_capsule()).empty());
}

void disabled_previously_overlapping_collider_emits_exit_with_mutation_api() {
    auto collider = make_collider(52, stellar::world::ObjectColliderShapeType::kSphere,
                                  {0.0F, 0.0F, 0.0F});
    collider.name = "Switch";
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{collider};
    system.set_colliders(colliders);
    assert(system.update_player_capsule(player_capsule()).size() == 1);

    const auto result = system.set_collider_enabled(52, false);
    assert(result.applied);
    assert(result.exit_events.size() == 1);
    assert(result.exit_events[0].collider_id == 52);
    assert(result.exit_events[0].name == "Switch");
    assert(result.exit_events[0].exited);
    assert(system.update_player_capsule(player_capsule()).empty());
}

void removed_previously_overlapping_collider_emits_exit_once() {
    auto collider = make_collider(53, stellar::world::ObjectColliderShapeType::kSphere,
                                  {0.0F, 0.0F, 0.0F});
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{collider};
    system.set_colliders(colliders);
    assert(system.update_player_capsule(player_capsule()).size() == 1);

    const auto result = system.remove_collider(53);
    assert(result.applied);
    assert(result.exit_events.size() == 1);
    assert(result.exit_events[0].collider_id == 53);
    assert(result.exit_events[0].exited);
    assert(system.update_player_capsule(player_capsule()).empty());

    const auto second = system.remove_collider(53);
    assert(!second.applied);
    assert(second.exit_events.empty());
}

void reordered_colliders_preserve_overlap_state_by_id() {
    auto first = make_collider(61, stellar::world::ObjectColliderShapeType::kSphere,
                               {0.0F, 0.0F, 0.0F});
    auto second = make_collider(62, stellar::world::ObjectColliderShapeType::kSphere,
                                {0.0F, 0.0F, 0.0F});
    stellar::world::ObjectColliderSystem system;
    const std::vector original{first, second};
    system.set_colliders(original);
    assert(system.update_player_capsule(player_capsule()).size() == 2);

    const std::vector reordered{second, first};
    const auto exits = system.replace_colliders_preserving_overlaps(reordered);
    assert(exits.empty());
    const auto events = system.update_player_capsule(player_capsule());
    assert(events.size() == 2);
    assert(events[0].collider_id == 62);
    assert(events[0].stayed);
    assert(events[1].collider_id == 61);
    assert(events[1].stayed);
}

void upsert_preserves_unrelated_overlap_state() {
    auto first = make_collider(71, stellar::world::ObjectColliderShapeType::kSphere,
                               {0.0F, 0.0F, 0.0F});
    auto second = make_collider(72, stellar::world::ObjectColliderShapeType::kSphere,
                                {0.0F, 0.0F, 0.0F});
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{first, second};
    system.set_colliders(colliders);
    assert(system.update_player_capsule(player_capsule()).size() == 2);

    auto replacement = second;
    replacement.name = "Replacement";
    replacement.enabled = false;
    const auto result = system.upsert_collider(replacement);
    assert(result.applied);
    assert(result.exit_events.size() == 1);
    assert(result.exit_events[0].collider_id == 72);

    const auto events = system.update_player_capsule(player_capsule());
    assert(events.size() == 1);
    assert(events[0].collider_id == 71);
    assert(events[0].stayed);
}

void duplicate_collider_ids_report_diagnostic() {
    auto first = make_collider(81, stellar::world::ObjectColliderShapeType::kSphere,
                               {0.0F, 0.0F, 0.0F});
    auto second = make_collider(81, stellar::world::ObjectColliderShapeType::kAabb,
                                {0.0F, 0.0F, 0.0F});
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{first, second};
    system.set_colliders(colliders);

    const auto diagnostics = system.diagnostics();
    assert(diagnostics.size() == 1);
    assert(diagnostics[0].severity == stellar::world::ObjectColliderDiagnosticSeverity::kError);
    assert(diagnostics[0].code == "duplicate_collider_id");
    assert(diagnostics[0].collider_id == 81);
    assert(diagnostics[0].collider_index == 1);

    const auto result = system.set_collider_enabled(81, false);
    assert(!result.applied);
    assert(result.code == "duplicate_collider_id");
}

void duplicate_names_remain_allowed_but_do_not_affect_identity() {
    auto first = make_collider(91, stellar::world::ObjectColliderShapeType::kSphere,
                               {0.0F, 0.0F, 0.0F});
    auto second = make_collider(92, stellar::world::ObjectColliderShapeType::kSphere,
                                {0.0F, 0.0F, 0.0F});
    first.name = "Same";
    second.name = "Same";
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{first, second};
    system.set_colliders(colliders);
    assert(system.diagnostics().empty());
    assert(system.update_player_capsule(player_capsule()).size() == 2);

    const auto result = system.remove_collider(91);
    assert(result.applied);
    assert(result.exit_events.size() == 1);
    assert(result.exit_events[0].collider_id == 91);

    const auto events = system.update_player_capsule(player_capsule());
    assert(events.size() == 1);
    assert(events[0].collider_id == 92);
    assert(events[0].stayed);
}

void queued_exit_order_is_deterministic() {
    auto first = make_collider(101, stellar::world::ObjectColliderShapeType::kSphere,
                               {0.0F, 0.0F, 0.0F});
    auto second = make_collider(102, stellar::world::ObjectColliderShapeType::kSphere,
                                {0.0F, 0.0F, 0.0F});
    stellar::world::ObjectColliderSystem system;
    const std::vector original{first, second};
    system.set_colliders(original);
    assert(system.update_player_capsule(player_capsule()).size() == 2);

    second.enabled = false;
    const std::vector replacement{second};
    const auto exits = system.replace_colliders_preserving_overlaps(replacement);
    assert(exits.size() == 2);
    assert(exits[0].collider_id == 101);
    assert(exits[1].collider_id == 102);
}

void disabled_collider_can_reenter_after_reenabled() {
    auto collider = make_collider(111, stellar::world::ObjectColliderShapeType::kSphere,
                                  {0.0F, 0.0F, 0.0F});
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{collider};
    system.set_colliders(colliders);
    assert(system.update_player_capsule(player_capsule()).size() == 1);

    auto result = system.set_collider_enabled(111, false);
    assert(result.exit_events.size() == 1);
    assert(system.update_player_capsule(player_capsule()).empty());

    result = system.set_collider_enabled(111, true);
    assert(result.applied);
    assert(result.exit_events.empty());
    const auto events = system.update_player_capsule(player_capsule());
    assert(events.size() == 1);
    assert(events[0].collider_id == 111);
    assert(events[0].entered);
}

void non_finite_shape_diagnostics_are_deterministic() {
    const float inf = std::numeric_limits<float>::infinity();
    auto first = make_collider(121, stellar::world::ObjectColliderShapeType::kSphere,
                               {inf, 0.0F, 0.0F});
    first.shape.radius = -1.0F;
    auto second = make_collider(122, stellar::world::ObjectColliderShapeType::kAabb,
                                {0.0F, 0.0F, 0.0F});
    second.shape.half_extents = {0.25F, inf, 0.25F};
    stellar::world::ObjectColliderSystem system;
    const std::vector colliders{first, second};
    system.set_colliders(colliders);

    const auto diagnostics = system.diagnostics();
    assert(diagnostics.size() == 3);
    assert(diagnostics[0].code == "non_finite_center");
    assert(diagnostics[0].collider_id == 121);
    assert(diagnostics[1].code == "invalid_radius");
    assert(diagnostics[1].collider_id == 121);
    assert(diagnostics[2].code == "non_finite_half_extents");
    assert(diagnostics[2].collider_id == 122);
}

void build_object_colliders_from_metadata_aabb() {
    stellar::assets::WorldMetadataAsset metadata;
    stellar::assets::WorldMarker ignored;
    ignored.type = stellar::assets::WorldMarkerType::kTrigger;
    ignored.name = "Ignored";
    metadata.markers.push_back(ignored);

    stellar::assets::WorldMarker pickup;
    pickup.type = stellar::assets::WorldMarkerType::kObjectCollider;
    pickup.name = "Pickup";
    pickup.archetype = "Health";
    pickup.position = {1.0F, 2.0F, 3.0F};
    pickup.scale = {-0.25F, 0.5F, 0.75F};
    metadata.markers.push_back(pickup);

    stellar::assets::WorldMarker hazard;
    hazard.type = stellar::assets::WorldMarkerType::kObjectCollider;
    hazard.name = "Hazard";
    hazard.position = {4.0F, 5.0F, 6.0F};
    hazard.scale = {1.0F, 2.0F, 3.0F};
    metadata.markers.push_back(hazard);

    const auto colliders = stellar::world::build_object_colliders(metadata);
    assert(colliders.size() == 2);
    assert(colliders[0].id == 2);
    assert(colliders[0].name == "Pickup");
    assert(colliders[0].archetype == "Health");
    assert(colliders[0].shape.type == stellar::world::ObjectColliderShapeType::kAabb);
    assert(colliders[0].shape.center == pickup.position);
    assert(colliders[0].shape.half_extents == (std::array<float, 3>{0.25F, 0.5F, 0.75F}));
    assert(colliders[1].id == 3);
    assert(colliders[1].name == "Hazard");
}

void build_object_colliders_from_runtime_world_uses_copied_metadata() {
    stellar::world::RuntimeWorld world;
    stellar::assets::WorldMarker collider;
    collider.type = stellar::assets::WorldMarkerType::kObjectCollider;
    collider.name = "RuntimePickup";
    collider.position = {7.0F, 8.0F, 9.0F};
    collider.scale = {1.0F, 1.5F, 2.0F};
    world.world_metadata.markers.push_back(collider);

    const auto colliders = stellar::world::build_object_colliders(world);
    assert(colliders.size() == 1);
    assert(colliders[0].id == 1);
    assert(colliders[0].name == "RuntimePickup");
    assert(colliders[0].shape.center == collider.position);
}

} // namespace

int main() {
    empty_object_collider_system_returns_no_events();
    capsule_enters_sphere_collider();
    capsule_enters_aabb_collider();
    capsule_enters_capsule_collider_if_supported();
    disabled_collider_does_not_emit_events();
    enter_stay_exit_are_deterministic();
    duplicate_names_do_not_break_id_based_events();
    non_finite_shape_data_is_sanitized_or_ignored_by_policy();
    object_collider_order_is_stable();
    set_colliders_is_hard_reset_and_does_not_emit_exit();
    disabled_previously_overlapping_collider_emits_exit_with_mutation_api();
    removed_previously_overlapping_collider_emits_exit_once();
    reordered_colliders_preserve_overlap_state_by_id();
    upsert_preserves_unrelated_overlap_state();
    duplicate_collider_ids_report_diagnostic();
    duplicate_names_remain_allowed_but_do_not_affect_identity();
    queued_exit_order_is_deterministic();
    disabled_collider_can_reenter_after_reenabled();
    non_finite_shape_diagnostics_are_deterministic();
    build_object_colliders_from_metadata_aabb();
    build_object_colliders_from_runtime_world_uses_copied_metadata();
    return 0;
}
