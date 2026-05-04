#include "stellar/client/PlayerPresentation.hpp"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "stellar/core/WorldAxes.hpp"

namespace stellar::client {
namespace {

constexpr float kDefaultNearPlane = 0.1F;
constexpr float kDefaultFarPlane = 4096.0F;
constexpr float kMinimumNearPlane = 0.01F;
constexpr float kMinimumClipSeparation = 0.01F;

float finite_or_zero(float value) noexcept {
    return std::isfinite(value) ? value : 0.0F;
}

glm::vec3 sanitized_vec3(const std::array<float, 3>& value) noexcept {
    return {finite_or_zero(value[0]), finite_or_zero(value[1]),
            finite_or_zero(value[2])};
}

std::array<float, 3> to_array(const glm::vec3& value) noexcept {
    return {value.x, value.y, value.z};
}

glm::quat sanitized_rotation(const std::array<float, 4>& rotation) noexcept {
    const glm::quat quaternion(finite_or_zero(rotation[3]),
                               finite_or_zero(rotation[0]),
                               finite_or_zero(rotation[1]),
                               finite_or_zero(rotation[2]));
    const float length = glm::length(quaternion);
    if (!std::isfinite(length) || length < 0.0001F) {
        return glm::quat(1.0F, 0.0F, 0.0F, 0.0F);
    }
    return glm::normalize(quaternion);
}

glm::vec3 world_axis(const std::array<float, 3>& axis) noexcept {
    return {axis[0], axis[1], axis[2]};
}

glm::vec3 z_up_forward_from_rotation(const std::array<float, 4>& rotation) noexcept {
    const glm::quat orientation = sanitized_rotation(rotation);
    glm::vec3 forward = orientation * world_axis(stellar::core::kWorldForward);
    if (!std::isfinite(forward.x) || !std::isfinite(forward.y) || !std::isfinite(forward.z) ||
        glm::length(forward) < 0.0001F) {
        return world_axis(stellar::core::kWorldForward);
    }

    forward = glm::normalize(forward);
    glm::vec3 horizontal_forward(forward.x, forward.y, 0.0F);
    if (glm::length(horizontal_forward) < 0.0001F) {
        horizontal_forward = world_axis(stellar::core::kWorldForward);
    } else {
        horizontal_forward = glm::normalize(horizontal_forward);
    }

    const float pitch_z = std::clamp(forward.z, -1.0F, 1.0F);
    const float horizontal_scale =
        std::sqrt(std::max(0.0F, 1.0F - pitch_z * pitch_z));
    return glm::normalize(horizontal_forward * horizontal_scale +
                          world_axis(stellar::core::kWorldUp) * pitch_z);
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
    const stellar::network::NetworkWorldSnapshot& snapshot,
    stellar::network::PlayerId player_id) {
    for (const stellar::network::PlayerSnapshot& player : snapshot.players) {
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
    const glm::vec3 eye =
        sanitized_vec3(player.position) + sanitized_vec3(config.follow_offset);
    const float look_distance =
        std::isfinite(config.look_distance) && config.look_distance > 0.0F
            ? config.look_distance
            : stellar::core::feet_to_units(12.0F);
    const glm::vec3 forward = z_up_forward_from_rotation(player.rotation);
    frame.eye = to_array(eye);
    frame.target = to_array(eye + forward * look_distance);
    frame.up = stellar::core::kWorldUp;
    frame.near_plane = sanitize_near_plane(config.near_plane);
    frame.far_plane = sanitize_far_plane(config.far_plane, frame.near_plane);
    return frame;
}

} // namespace stellar::client
