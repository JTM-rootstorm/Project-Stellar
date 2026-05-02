#include "stellar/client/PlayerPresentation.hpp"

#include <algorithm>
#include <cmath>

namespace stellar::client {
namespace {

constexpr float kDefaultNearPlane = 0.1F;
constexpr float kDefaultFarPlane = 4096.0F;
constexpr float kMinimumNearPlane = 0.01F;
constexpr float kMinimumClipSeparation = 0.01F;

float finite_or_zero(float value) noexcept {
    return std::isfinite(value) ? value : 0.0F;
}

std::array<float, 3> sanitized_sum(const std::array<float, 3>& lhs,
                                   const std::array<float, 3>& rhs) noexcept {
    return {finite_or_zero(lhs[0]) + finite_or_zero(rhs[0]),
            finite_or_zero(lhs[1]) + finite_or_zero(rhs[1]),
            finite_or_zero(lhs[2]) + finite_or_zero(rhs[2])};
}

float sanitize_near_plane(float near_plane) noexcept {
    if (!std::isfinite(near_plane) || near_plane <= 0.0F) {
        return kDefaultNearPlane;
    }
    return std::max(near_plane, kMinimumNearPlane);
}

float sanitize_far_plane(float far_plane, float near_plane) noexcept {
    const float minimum_far = near_plane + kMinimumClipSeparation;
    if (!std::isfinite(far_plane)) {
        return std::max(kDefaultFarPlane, minimum_far);
    }
    return std::max(far_plane, minimum_far);
}

} // namespace

std::optional<PlayerPresentationState> make_player_presentation_state(
    const stellar::server::WorldSnapshot& snapshot,
    stellar::server::PlayerId player_id) {
    for (const stellar::server::PlayerSnapshot& player : snapshot.players) {
        if (player.player_id == player_id) {
            return PlayerPresentationState{.position = player.position,
                                           .rotation = player.rotation,
                                           .grounded = player.grounded};
        }
    }

    return std::nullopt;
}

std::optional<PlayerPresentationState> make_player_presentation_state(
    const stellar::network::NetworkWorldSnapshot& snapshot,
    stellar::server::PlayerId player_id) {
    for (const stellar::server::PlayerSnapshot& player : snapshot.players) {
        if (player.player_id == player_id) {
            return PlayerPresentationState{.position = player.position,
                                           .rotation = player.rotation,
                                           .grounded = player.grounded};
        }
    }

    return std::nullopt;
}

PlayerCameraFrame make_player_camera_frame(const PlayerPresentationState& player,
                                           const PlayerCameraConfig& config) noexcept {
    PlayerCameraFrame frame;
    frame.eye = sanitized_sum(player.position, config.follow_offset);
    frame.target = sanitized_sum(player.position, config.look_at_offset);
    frame.near_plane = sanitize_near_plane(config.near_plane);
    frame.far_plane = sanitize_far_plane(config.far_plane, frame.near_plane);
    return frame;
}

} // namespace stellar::client
