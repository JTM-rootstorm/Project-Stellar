#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <string_view>
#include <vector>

#include "stellar/assets/SceneAsset.hpp"
#include "stellar/platform/Error.hpp"
#include "stellar/scene/SceneGraph.hpp"

namespace stellar::scene {

/**
 * @brief CPU-side final matrices for one evaluated skin pose.
 */
struct SkinPose {
    /** @brief Final joint matrices in skin joint order for later skinning upload. */
    std::vector<std::array<float, 16>> joint_matrices;
};

/**
 * @brief Runtime node transforms and skeleton poses evaluated from a scene asset.
 */
struct ScenePose {
    /** @brief Mutable local node transforms used by animation playback. */
    std::vector<Transform> local_transforms;

    /** @brief Cached world transforms for every node in scene node order. */
    std::vector<std::array<float, 16>> world_transforms;

    /** @brief Cached skin poses in scene skin order. */
    std::vector<SkinPose> skin_poses;
};

/**
 * @brief Backend-neutral runtime evaluator for imported animation and skin assets.
 */
class AnimationPlayer {
public:
    /**
     * @brief Bind the player to a scene asset and reset runtime transforms to asset defaults.
     * @param scene Asset scene containing nodes, skins, and animations.
     */
    void bind(const assets::SceneAsset& scene) noexcept;

    /**
     * @brief Select an animation by index and reset playback time.
     * @param animation_index Index in the bound scene animation array.
     * @return Empty result on success, or platform error when no such animation exists.
     */
    [[nodiscard]] std::expected<void, platform::Error>
    select_animation(std::size_t animation_index) noexcept;

    /**
     * @brief Select an animation by its imported name and reset playback time.
     * @param name Animation name to match exactly.
     * @return Empty result on success, or platform error when no matching animation exists.
     */
    [[nodiscard]] std::expected<void, platform::Error>
    select_animation(std::string_view name) noexcept;

    /** @brief Start or resume playback from the current local time. */
    void play() noexcept;

    /** @brief Pause playback while preserving current local time and pose. */
    void pause() noexcept;

    /** @brief Stop playback, reset local time to zero, and restore static node transforms. */
    void stop() noexcept;

    /**
     * @brief Enable or disable looping for the active animation.
     * @param enabled True to wrap time by animation duration.
     */
    void set_looping(bool enabled) noexcept;

    /**
     * @brief Advance playback by a time delta independent of rendering frame rate.
     * @param delta_seconds Seconds to advance when playback is active.
     * @return Empty result on success, or platform error if evaluation data is malformed.
     */
    [[nodiscard]] std::expected<void, platform::Error> update(float delta_seconds) noexcept;

    /**
     * @brief Evaluate the selected animation at an explicit time without changing play state.
     * @param time_seconds Animation-local sample time in seconds.
     * @return Empty result on success, or platform error if evaluation data is malformed.
     */
    [[nodiscard]] std::expected<void, platform::Error> evaluate(float time_seconds) noexcept;

    /** @brief Recompute node world transforms and skin joint matrices from current local pose. */
    void update_world_and_skin_poses() noexcept;

    /**
     * @brief Mutable access to the current runtime pose.
     * @return Runtime scene pose.
     */
    [[nodiscard]] ScenePose& pose() noexcept;

    /**
     * @brief Const access to the current runtime pose.
     * @return Runtime scene pose.
     */
    [[nodiscard]] const ScenePose& pose() const noexcept;

    /**
     * @brief Check whether playback is currently advancing over time.
     * @return True when playing.
     */
    [[nodiscard]] bool is_playing() const noexcept;

    /**
     * @brief Get the current animation-local playback time in seconds.
     * @return Current playback time.
     */
    [[nodiscard]] float current_time() const noexcept;

    /**
     * @brief Get the selected animation index, if any.
     * @return Selected animation index or empty optional.
     */
    [[nodiscard]] std::optional<std::size_t> selected_animation() const noexcept;

private:
    const assets::SceneAsset* scene_ = nullptr;
    ScenePose pose_;
    std::optional<std::size_t> selected_animation_;
    float current_time_ = 0.0f;
    bool playing_ = false;
    bool looping_ = true;
};

/**
 * @brief Compose a scene transform into a column-major matrix.
 * @param transform Local transform to compose.
 * @return Column-major transform matrix.
 */
[[nodiscard]] std::array<float, 16> compose_transform(const Transform& transform) noexcept;

} // namespace stellar::scene
