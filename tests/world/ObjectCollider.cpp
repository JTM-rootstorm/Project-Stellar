#include "stellar/world/ObjectCollider.hpp"

#include <cassert>
#include <limits>
#include <vector>

namespace {

stellar::world::TriggerCapsule player_capsule(std::array<float, 3> center = {}) {
    return {.center = center, .up = {0.0F, 1.0F, 0.0F}, .radius = 0.5F, .height = 2.0F};
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

void disabled_previously_overlapping_collider_emits_exit() {
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
    disabled_previously_overlapping_collider_emits_exit();
    return 0;
}
