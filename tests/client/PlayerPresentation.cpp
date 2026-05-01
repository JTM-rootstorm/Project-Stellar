#include "stellar/client/PlayerPresentation.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <limits>

namespace {

constexpr float kEpsilon = 0.00001F;

void assert_near(float actual, float expected) {
    assert(std::fabs(actual - expected) <= kEpsilon);
}

void assert_vec3(const std::array<float, 3>& actual,
                 const std::array<float, 3>& expected) {
    assert_near(actual[0], expected[0]);
    assert_near(actual[1], expected[1]);
    assert_near(actual[2], expected[2]);
}

stellar::server::PlayerSnapshot make_snapshot_player(stellar::server::PlayerId player_id) {
    stellar::server::PlayerSnapshot player;
    player.player_id = player_id;
    player.position = {1.0F, 2.0F, 3.0F};
    player.velocity = {4.0F, 5.0F, 6.0F};
    player.rotation = {0.0F, 0.70710677F, 0.0F, 0.70710677F};
    player.grounded = true;
    return player;
}

void missing_player_snapshot_returns_nullopt() {
    stellar::server::WorldSnapshot snapshot;
    snapshot.players.push_back(make_snapshot_player(1));

    const auto state = stellar::client::make_player_presentation_state(snapshot, 2);

    assert(!state.has_value());
}

void player_snapshot_extracts_position_rotation_grounded() {
    stellar::server::WorldSnapshot snapshot;
    snapshot.players.push_back(make_snapshot_player(1));
    snapshot.players.push_back(make_snapshot_player(7));
    snapshot.players[1].position = {-2.0F, 0.5F, 9.0F};
    snapshot.players[1].rotation = {0.25F, 0.5F, 0.75F, 1.0F};
    snapshot.players[1].grounded = false;

    const auto state = stellar::client::make_player_presentation_state(snapshot, 7);

    assert(state.has_value());
    assert_vec3(state->position, {-2.0F, 0.5F, 9.0F});
    assert(state->rotation == (std::array<float, 4>{0.25F, 0.5F, 0.75F, 1.0F}));
    assert(!state->grounded);
}

void camera_frame_uses_follow_and_look_offsets() {
    stellar::client::PlayerPresentationState player;
    player.position = {10.0F, 20.0F, 30.0F};
    stellar::client::PlayerCameraConfig config;
    config.follow_offset = {1.0F, 2.0F, 3.0F};
    config.look_at_offset = {-4.0F, 5.0F, -6.0F};
    config.near_plane = 0.25F;
    config.far_plane = 300.0F;

    const auto frame = stellar::client::make_player_camera_frame(player, config);

    assert_vec3(frame.eye, {11.0F, 22.0F, 33.0F});
    assert_vec3(frame.target, {6.0F, 25.0F, 24.0F});
    assert_near(frame.near_plane, 0.25F);
    assert_near(frame.far_plane, 300.0F);
}

void default_camera_config_uses_inch_scale_debug_offsets() {
    const stellar::client::PlayerCameraConfig config;

    assert_vec3(config.follow_offset, {0.0F, 72.0F, 144.0F});
    assert_vec3(config.look_at_offset, {0.0F, 60.0F, 0.0F});
    assert_near(config.far_plane, 4096.0F);
}

void camera_frame_sanitizes_non_finite_input() {
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    stellar::client::PlayerPresentationState player;
    player.position = {infinity, 2.0F, nan};
    stellar::client::PlayerCameraConfig config;
    config.follow_offset = {1.0F, infinity, 3.0F};
    config.look_at_offset = {nan, 4.0F, infinity};
    config.near_plane = nan;
    config.far_plane = infinity;

    const auto frame = stellar::client::make_player_camera_frame(player, config);

    assert_vec3(frame.eye, {1.0F, 2.0F, 3.0F});
    assert_vec3(frame.target, {0.0F, 6.0F, 0.0F});
    assert_near(frame.near_plane, 0.1F);
    assert_near(frame.far_plane, 4096.0F);
    for (float value : frame.eye) {
        assert(std::isfinite(value));
    }
    for (float value : frame.target) {
        assert(std::isfinite(value));
    }
}

void camera_near_far_are_clamped_or_preserved_by_documented_policy() {
    stellar::client::PlayerPresentationState player;
    stellar::client::PlayerCameraConfig config;
    config.near_plane = 0.001F;
    config.far_plane = 0.005F;

    const auto clamped = stellar::client::make_player_camera_frame(player, config);
    assert_near(clamped.near_plane, 0.01F);
    assert_near(clamped.far_plane, 0.02F);

    config.near_plane = 2.0F;
    config.far_plane = 10.0F;
    const auto preserved = stellar::client::make_player_camera_frame(player, config);
    assert_near(preserved.near_plane, 2.0F);
    assert_near(preserved.far_plane, 10.0F);
}

void snapshot_presentation_does_not_mutate_authoritative_snapshot() {
    stellar::server::WorldSnapshot snapshot;
    snapshot.tick = 42;
    snapshot.players.push_back(make_snapshot_player(5));
    const stellar::server::WorldSnapshot original = snapshot;

    const auto state = stellar::client::make_player_presentation_state(snapshot, 5);
    assert(state.has_value());
    const auto frame = stellar::client::make_player_camera_frame(*state);
    assert(std::isfinite(frame.eye[0]));

    assert(snapshot.tick == original.tick);
    assert(snapshot.players.size() == original.players.size());
    assert(snapshot.players[0].player_id == original.players[0].player_id);
    assert(snapshot.players[0].position == original.players[0].position);
    assert(snapshot.players[0].velocity == original.players[0].velocity);
    assert(snapshot.players[0].rotation == original.players[0].rotation);
    assert(snapshot.players[0].grounded == original.players[0].grounded);
}

} // namespace

int main() {
    missing_player_snapshot_returns_nullopt();
    player_snapshot_extracts_position_rotation_grounded();
    camera_frame_uses_follow_and_look_offsets();
    default_camera_config_uses_inch_scale_debug_offsets();
    camera_frame_sanitizes_non_finite_input();
    camera_near_far_are_clamped_or_preserved_by_documented_policy();
    snapshot_presentation_does_not_mutate_authoritative_snapshot();

    return 0;
}
