#pragma once

#include <array>

#include "stellar/physics/CollisionWorld.hpp"

namespace stellar::physics {

/**
 * @brief Tuning values for deterministic static character movement.
 */
struct CharacterControllerConfig {
    /** @brief Character collision radius used for static sweeps. */
    float radius = 0.35F;

    /** @brief Reserved vertical capsule height; current implementation uses the center sphere. */
    float height = 1.8F;

    /** @brief Small separation kept between the character and static triangles. */
    float skin_width = 0.03F;

    /** @brief Maximum ground slope angle, in degrees, considered walkable. */
    float max_slope_degrees = 50.0F;

    /** @brief Maximum conservative vertical lift used by step-up attempts. */
    float step_height = 0.35F;

    /** @brief Maximum post-move downward snap distance used to keep stable grounding. */
    float ground_snap_distance = 0.12F;

    /** @brief Fixed upper bound for sweep-and-slide iterations. */
    int max_slide_iterations = 4;
};

/**
 * @brief Input state for one deterministic character movement query.
 */
struct CharacterMoveInput {
    /** @brief World-space character center position before movement. */
    std::array<float, 3> position{};

    /** @brief Desired world-space displacement for this movement step. */
    std::array<float, 3> displacement{};

    /** @brief World-space up direction used for slope checks and ground snapping. */
    std::array<float, 3> up{0.0F, 1.0F, 0.0F};
};

/**
 * @brief Result state from one deterministic character movement query.
 */
struct CharacterMoveResult {
    /** @brief Final world-space character center position. */
    std::array<float, 3> position{};

    /** @brief Displacement not consumed by sweep-and-slide resolution. */
    std::array<float, 3> remaining_displacement{};

    /** @brief Walkable ground normal when grounded, otherwise the configured up direction. */
    std::array<float, 3> ground_normal{0.0F, 1.0F, 0.0F};

    /** @brief True when movement or recovery touched static collision. */
    bool hit = false;

    /** @brief True when final position has walkable static support within snap range. */
    bool grounded = false;

    /** @brief True when the conservative step-up fallback produced the final position. */
    bool stepped = false;

    /** @brief True when initial overlap recovery moved the character. */
    bool started_overlapping = false;

    /** @brief Number of sweep-and-slide iterations used by the selected move. */
    int iterations = 0;
};

/**
 * @brief Small deterministic character controller over immutable static CollisionWorld triangles.
 *
 * This is a static-world sweep/slide helper, not a full physics engine. The current controller uses
 * a sphere at the supplied position; height is retained in the config for a future capsule upgrade.
 */
class CharacterController {
public:
    /** @brief Construct a controller that queries an existing static collision world. */
    explicit CharacterController(const CollisionWorld& world) noexcept;

    /** @brief Move a character through the static collision world using recovery, slide, snap, and step-up. */
    [[nodiscard]] CharacterMoveResult move(const CharacterMoveInput& input,
                                           const CharacterControllerConfig& config) const noexcept;

private:
    const CollisionWorld* world_ = nullptr;
};

} // namespace stellar::physics
