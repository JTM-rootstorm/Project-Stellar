#include "stellar/server/FootstepTracker.hpp"

#include <array>
#include <cassert>
#include <string>
#include <utility>

namespace {

using Vec3 = std::array<float, 3>;

stellar::server::FootstepTrackerInput input(Vec3 previous,
                                            Vec3 current,
                                            bool grounded = true,
                                            std::string surface_id = "wood") {
    return stellar::server::FootstepTrackerInput{
        .player_id = 7,
        .entity_id = 42,
        .tick = 123,
        .previous_position = previous,
        .current_position = current,
        .current_velocity = {40.0F, 0.0F, 0.0F},
        .grounded = grounded,
        .surface = stellar::server::FootstepSurfaceHit{.valid = !surface_id.empty(),
                                                       .surface_id = std::move(surface_id),
                                                       .source_material_name = "wood/plank_01"}};
}

void emits_after_enough_grounded_horizontal_travel() {
    stellar::server::FootstepTracker tracker(
        stellar::server::FootstepTrackerConfig{.walk_step_distance = 10.0F});

    assert(!tracker.update(input({0.0F, 0.0F, 0.0F}, {5.0F, 0.0F, 0.0F})).has_value());
    const auto event = tracker.update(input({5.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F}));
    assert(event.has_value());
    assert(event->tick == 123);
    assert(event->player_id == 7);
    assert(event->entity_id == 42);
    assert(event->surface_id == "wood");
    assert(event->source_material_name == "wood/plank_01");
}

void does_not_emit_when_idle_or_airborne() {
    stellar::server::FootstepTracker tracker(
        stellar::server::FootstepTrackerConfig{.walk_step_distance = 1.0F});

    auto idle = input({0.0F, 0.0F, 0.0F}, {5.0F, 0.0F, 0.0F});
    idle.current_velocity = {};
    assert(!tracker.update(idle).has_value());
    assert(!tracker.update(input({5.0F, 0.0F, 0.0F}, {20.0F, 0.0F, 0.0F}, false)).has_value());
}

void reset_clears_accumulated_distance() {
    stellar::server::FootstepTracker tracker(
        stellar::server::FootstepTrackerConfig{.walk_step_distance = 10.0F});

    assert(!tracker.update(input({0.0F, 0.0F, 0.0F}, {6.0F, 0.0F, 0.0F})).has_value());
    tracker.reset({6.0F, 0.0F, 0.0F});
    assert(!tracker.update(input({6.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F})).has_value());
}

void invalid_or_empty_surface_falls_back_to_generic() {
    stellar::server::FootstepTracker tracker(
        stellar::server::FootstepTrackerConfig{.walk_step_distance = 1.0F});

    const auto event = tracker.update(input({0.0F, 0.0F, 0.0F}, {2.0F, 0.0F, 0.0F}, true, ""));
    assert(event.has_value());
    assert(event->surface_id == "generic");
}

} // namespace

int main() {
    emits_after_enough_grounded_horizontal_travel();
    does_not_emit_when_idle_or_airborne();
    reset_clears_accumulated_distance();
    invalid_or_empty_surface_falls_back_to_generic();
    return 0;
}
