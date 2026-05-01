#include "stellar/client/GameplayPresentation.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace {

stellar::server::GameplayEntity make_entity(stellar::server::EntityKind kind, bool active = true) {
    stellar::server::GameplayEntity entity;
    entity.id = 7;
    entity.kind = kind;
    entity.active = active;
    entity.transform.position = {1.0F, 2.0F, 3.0F};
    entity.metadata.size = {12.0F, 18.0F, 0.0F};
    entity.metadata.alpha = 0.75F;
    return entity;
}

stellar::network::NetworkGameplayEntity make_network_entity(stellar::server::EntityKind kind,
                                                            bool active = true) {
    stellar::network::NetworkGameplayEntity entity;
    entity.id = 7;
    entity.kind = kind;
    entity.active = active;
    entity.transform.position = {1.0F, 2.0F, 3.0F};
    entity.metadata.size = {12.0F, 18.0F, 0.0F};
    entity.metadata.alpha = 0.75F;
    return entity;
}

void active_sprite_converts_to_billboard() {
    stellar::server::WorldSnapshot snapshot;
    snapshot.gameplay_world.entities.push_back(make_entity(stellar::server::EntityKind::kSprite));

    const auto frame = stellar::client::make_gameplay_presentation_frame(snapshot);

    assert(frame.sprites.size() == 1);
    assert(frame.sprites[0].position == (std::array<float, 3>{1.0F, 2.0F, 3.0F}));
    assert(frame.sprites[0].size == (std::array<float, 2>{12.0F, 18.0F}));
    assert(frame.sprites[0].color[3] == 0.75F);
    assert(!frame.sprites[0].texture);
}

void active_pickup_uses_pickup_color_and_inactive_pickup_is_filtered() {
    stellar::server::WorldSnapshot snapshot;
    snapshot.gameplay_world.entities.push_back(make_entity(stellar::server::EntityKind::kPickup));
    snapshot.gameplay_world.entities.push_back(
        make_entity(stellar::server::EntityKind::kPickup, false));

    const auto frame = stellar::client::make_gameplay_presentation_frame(snapshot);

    assert(frame.sprites.size() == 1);
    assert(frame.sprites[0].color[0] == 1.0F);
    assert(frame.sprites[0].color[1] == 1.0F);
    assert(frame.sprites[0].color[2] == 0.25F);
}

void door_marker_requires_debug_flag_and_tracks_open_state() {
    stellar::server::WorldSnapshot snapshot;
    auto door = make_entity(stellar::server::EntityKind::kDoor);
    door.open = true;
    snapshot.gameplay_world.entities.push_back(door);

    assert(stellar::client::make_gameplay_presentation_frame(snapshot).sprites.empty());

    stellar::client::GameplayPresentationConfig config;
    config.show_debug_interaction_markers = true;
    const auto frame = stellar::client::make_gameplay_presentation_frame(snapshot, config);

    assert(frame.sprites.size() == 1);
    assert(frame.sprites[0].color[0] == 0.25F);
    assert(frame.sprites[0].color[1] == 1.0F);
    assert(frame.sprites[0].color[3] == 0.375F);
}

void unknown_metadata_uses_finite_defaults() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    stellar::server::WorldSnapshot snapshot;
    auto sprite = make_entity(stellar::server::EntityKind::kSprite);
    sprite.transform.position = {nan, inf, -4.0F};
    sprite.metadata.size = {nan, -1.0F, inf};
    sprite.metadata.alpha = inf;
    snapshot.gameplay_world.entities.push_back(sprite);

    const auto frame = stellar::client::make_gameplay_presentation_frame(snapshot);

    assert(frame.sprites.size() == 1);
    assert(frame.sprites[0].position == (std::array<float, 3>{0.0F, 0.0F, -4.0F}));
    assert(frame.sprites[0].size == (std::array<float, 2>{24.0F, 24.0F}));
    for (const float value : frame.sprites[0].position) {
        assert(std::isfinite(value));
    }
    for (const float value : frame.sprites[0].size) {
        assert(std::isfinite(value));
    }
    for (const float value : frame.sprites[0].color) {
        assert(std::isfinite(value));
    }
}

void non_presented_entity_kinds_are_hidden_by_default() {
    stellar::server::WorldSnapshot snapshot;
    snapshot.gameplay_world.entities.push_back(make_entity(stellar::server::EntityKind::kPlayer));
    snapshot.gameplay_world.entities.push_back(make_entity(stellar::server::EntityKind::kTrigger));
    snapshot.gameplay_world.entities.push_back(
        make_entity(stellar::server::EntityKind::kObjectCollider));

    const auto frame = stellar::client::make_gameplay_presentation_frame(snapshot);

    assert(frame.sprites.empty());
}

void network_snapshot_overload_matches_server_snapshot_behavior() {
    stellar::network::NetworkWorldSnapshot snapshot;
    snapshot.entities.push_back(make_network_entity(stellar::server::EntityKind::kSprite));
    snapshot.entities.push_back(make_network_entity(stellar::server::EntityKind::kPickup));
    snapshot.entities.push_back(make_network_entity(stellar::server::EntityKind::kPickup, false));
    auto door = make_network_entity(stellar::server::EntityKind::kDoor);
    door.open = true;
    snapshot.entities.push_back(door);

    assert(stellar::client::make_gameplay_presentation_frame(snapshot).sprites.size() == 2);

    stellar::client::GameplayPresentationConfig config;
    config.show_debug_interaction_markers = true;
    const auto frame = stellar::client::make_gameplay_presentation_frame(snapshot, config);

    assert(frame.sprites.size() == 3);
    assert(frame.sprites[0].position == (std::array<float, 3>{1.0F, 2.0F, 3.0F}));
    assert(frame.sprites[1].color[2] == 0.25F);
    assert(frame.sprites[2].color[1] == 1.0F);
}

} // namespace

int main() {
    active_sprite_converts_to_billboard();
    active_pickup_uses_pickup_color_and_inactive_pickup_is_filtered();
    door_marker_requires_debug_flag_and_tracks_open_state();
    unknown_metadata_uses_finite_defaults();
    non_presented_entity_kinds_are_hidden_by_default();
    network_snapshot_overload_matches_server_snapshot_behavior();
    return 0;
}
