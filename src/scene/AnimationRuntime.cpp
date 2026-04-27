#include "stellar/scene/AnimationRuntime.hpp"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace stellar::scene {
namespace {

glm::mat4 to_glm_mat4(const std::array<float, 16>& data) noexcept {
    return glm::make_mat4(data.data());
}

std::array<float, 16> to_array(const glm::mat4& matrix) noexcept {
    std::array<float, 16> result{};
    const float* data = &matrix[0][0];
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = data[index];
    }
    return result;
}

glm::quat normalize_quat(glm::quat value) noexcept {
    const float length = glm::length(value);
    if (!std::isfinite(length) || length <= 0.000001f) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    return glm::normalize(value);
}

float animation_duration(const assets::AnimationAsset& animation) noexcept {
    float duration = 0.0f;
    for (const auto& sampler : animation.samplers) {
        if (!sampler.input_times.empty()) {
            duration = std::max(duration, sampler.input_times.back());
        }
    }
    return duration;
}

std::expected<std::vector<float>, platform::Error>
sample_sampler(const assets::AnimationSamplerAsset& sampler, float time_seconds) noexcept {
    const std::size_t key_count = sampler.input_times.size();
    const std::size_t components = sampler.output_components;
    if (key_count == 0 || components == 0) {
        return std::unexpected(platform::Error("Animation sampler has no keyframes"));
    }

    const std::size_t values_per_key =
        sampler.interpolation == assets::AnimationInterpolation::kCubicSpline ? 3u : 1u;
    if (sampler.output_values.size() != key_count * values_per_key * components) {
        return std::unexpected(platform::Error("Animation sampler output size mismatch"));
    }

    auto value_at = [&](std::size_t key, std::size_t slot) {
        std::vector<float> value(components);
        const std::size_t offset = (key * values_per_key + slot) * components;
        for (std::size_t component = 0; component < components; ++component) {
            value[component] = sampler.output_values[offset + component];
        }
        return value;
    };

    if (key_count == 1 || time_seconds <= sampler.input_times.front()) {
        const std::size_t value_slot = values_per_key == 3u ? 1u : 0u;
        return value_at(0, value_slot);
    }
    if (time_seconds >= sampler.input_times.back()) {
        const std::size_t value_slot = values_per_key == 3u ? 1u : 0u;
        return value_at(key_count - 1, value_slot);
    }

    const auto upper = std::upper_bound(sampler.input_times.begin(), sampler.input_times.end(),
                                        time_seconds);
    const std::size_t next_key = static_cast<std::size_t>(upper - sampler.input_times.begin());
    const std::size_t prev_key = next_key - 1;
    const float t0 = sampler.input_times[prev_key];
    const float t1 = sampler.input_times[next_key];
    const float segment_duration = t1 - t0;
    if (!std::isfinite(segment_duration) || segment_duration <= 0.0f) {
        return std::unexpected(platform::Error("Animation sampler times must be increasing"));
    }
    const float u = (time_seconds - t0) / segment_duration;

    if (sampler.interpolation == assets::AnimationInterpolation::kStep) {
        return value_at(prev_key, 0);
    }

    if (sampler.interpolation == assets::AnimationInterpolation::kCubicSpline) {
        const auto p0 = value_at(prev_key, 1);
        const auto m0 = value_at(prev_key, 2);
        const auto p1 = value_at(next_key, 1);
        const auto m1 = value_at(next_key, 0);
        const float u2 = u * u;
        const float u3 = u2 * u;
        const float h00 = 2.0f * u3 - 3.0f * u2 + 1.0f;
        const float h10 = u3 - 2.0f * u2 + u;
        const float h01 = -2.0f * u3 + 3.0f * u2;
        const float h11 = u3 - u2;
        std::vector<float> result(components);
        for (std::size_t component = 0; component < components; ++component) {
            result[component] = h00 * p0[component] + h10 * segment_duration * m0[component] +
                                h01 * p1[component] + h11 * segment_duration * m1[component];
        }
        return result;
    }

    const auto p0 = value_at(prev_key, 0);
    const auto p1 = value_at(next_key, 0);
    std::vector<float> result(components);
    for (std::size_t component = 0; component < components; ++component) {
        result[component] = p0[component] * (1.0f - u) + p1[component] * u;
    }
    return result;
}

std::expected<std::array<float, 4>, platform::Error>
sample_rotation_sampler(const assets::AnimationSamplerAsset& sampler, float time_seconds) noexcept {
    if (sampler.interpolation != assets::AnimationInterpolation::kLinear) {
        auto sampled = sample_sampler(sampler, time_seconds);
        if (!sampled) {
            return std::unexpected(sampled.error());
        }
        if (sampled->size() != 4u) {
            return std::unexpected(platform::Error("Rotation channel must have 4 components"));
        }
        const glm::quat quat = normalize_quat(
            glm::quat((*sampled)[3], (*sampled)[0], (*sampled)[1], (*sampled)[2]));
        return std::array<float, 4>{quat.x, quat.y, quat.z, quat.w};
    }

    const std::size_t key_count = sampler.input_times.size();
    if (key_count == 0 || sampler.output_components != 4u ||
        sampler.output_values.size() != key_count * 4u) {
        return std::unexpected(platform::Error("Rotation sampler output size mismatch"));
    }
    auto quat_at = [&](std::size_t key) {
        const std::size_t offset = key * 4u;
        return normalize_quat(glm::quat(sampler.output_values[offset + 3u],
                                        sampler.output_values[offset + 0u],
                                        sampler.output_values[offset + 1u],
                                        sampler.output_values[offset + 2u]));
    };

    if (key_count == 1 || time_seconds <= sampler.input_times.front()) {
        const glm::quat quat = quat_at(0);
        return std::array<float, 4>{quat.x, quat.y, quat.z, quat.w};
    }
    if (time_seconds >= sampler.input_times.back()) {
        const glm::quat quat = quat_at(key_count - 1);
        return std::array<float, 4>{quat.x, quat.y, quat.z, quat.w};
    }

    const auto upper = std::upper_bound(sampler.input_times.begin(), sampler.input_times.end(),
                                        time_seconds);
    const std::size_t next_key = static_cast<std::size_t>(upper - sampler.input_times.begin());
    const std::size_t prev_key = next_key - 1;
    const float segment_duration = sampler.input_times[next_key] - sampler.input_times[prev_key];
    if (!std::isfinite(segment_duration) || segment_duration <= 0.0f) {
        return std::unexpected(platform::Error("Animation sampler times must be increasing"));
    }
    const float u = (time_seconds - sampler.input_times[prev_key]) / segment_duration;
    const glm::quat quat = normalize_quat(glm::slerp(quat_at(prev_key), quat_at(next_key), u));
    return std::array<float, 4>{quat.x, quat.y, quat.z, quat.w};
}

void compute_node_world(const assets::SceneAsset& scene, ScenePose& pose, std::size_t node_index,
                        const glm::mat4& parent_world, std::vector<bool>& visited) noexcept {
    if (node_index >= scene.nodes.size() || node_index >= pose.local_transforms.size() ||
        visited[node_index]) {
        return;
    }
    visited[node_index] = true;
    const glm::mat4 local = to_glm_mat4(compose_transform(pose.local_transforms[node_index]));
    const glm::mat4 world = parent_world * local;
    pose.world_transforms[node_index] = to_array(world);
    for (std::size_t child_index : scene.nodes[node_index].children) {
        compute_node_world(scene, pose, child_index, world, visited);
    }
}

} // namespace

void AnimationPlayer::bind(const assets::SceneAsset& scene) noexcept {
    scene_ = &scene;
    selected_animation_.reset();
    current_time_ = 0.0f;
    playing_ = false;
    pose_.local_transforms.clear();
    pose_.local_transforms.reserve(scene.nodes.size());
    for (const auto& node : scene.nodes) {
        pose_.local_transforms.push_back(node.local_transform);
    }
    pose_.world_transforms.assign(scene.nodes.size(), compose_transform(Transform{}));
    pose_.skin_poses.resize(scene.skins.size());
    for (std::size_t skin_index = 0; skin_index < scene.skins.size(); ++skin_index) {
        pose_.skin_poses[skin_index].joint_matrices.assign(scene.skins[skin_index].joints.size(),
                                                           compose_transform(Transform{}));
    }
    update_world_and_skin_poses();
}

std::expected<void, platform::Error>
AnimationPlayer::select_animation(std::size_t animation_index) noexcept {
    if (!scene_ || animation_index >= scene_->animations.size()) {
        return std::unexpected(platform::Error("Animation index out of range"));
    }
    selected_animation_ = animation_index;
    current_time_ = 0.0f;
    return evaluate(0.0f);
}

std::expected<void, platform::Error>
AnimationPlayer::select_animation(std::string_view name) noexcept {
    if (!scene_) {
        return std::unexpected(platform::Error("Animation player has no scene"));
    }
    for (std::size_t index = 0; index < scene_->animations.size(); ++index) {
        if (scene_->animations[index].name == name) {
            return select_animation(index);
        }
    }
    return std::unexpected(platform::Error("Animation name not found"));
}

void AnimationPlayer::play() noexcept {
    playing_ = true;
}

void AnimationPlayer::pause() noexcept {
    playing_ = false;
}

void AnimationPlayer::stop() noexcept {
    playing_ = false;
    current_time_ = 0.0f;
    if (scene_) {
        pose_.local_transforms.clear();
        pose_.local_transforms.reserve(scene_->nodes.size());
        for (const auto& node : scene_->nodes) {
            pose_.local_transforms.push_back(node.local_transform);
        }
        update_world_and_skin_poses();
    }
}

void AnimationPlayer::set_looping(bool enabled) noexcept {
    looping_ = enabled;
}

std::expected<void, platform::Error> AnimationPlayer::update(float delta_seconds) noexcept {
    if (!playing_ || !selected_animation_.has_value()) {
        update_world_and_skin_poses();
        return {};
    }
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0f) {
        return std::unexpected(platform::Error("Animation delta time must be non-negative"));
    }
    current_time_ += delta_seconds;
    if (scene_ && *selected_animation_ < scene_->animations.size()) {
        const float duration = animation_duration(scene_->animations[*selected_animation_]);
        if (duration > 0.0f) {
            if (looping_) {
                current_time_ = std::fmod(current_time_, duration);
            } else if (current_time_ > duration) {
                current_time_ = duration;
                playing_ = false;
            }
        }
    }
    return evaluate(current_time_);
}

std::expected<void, platform::Error> AnimationPlayer::evaluate(float time_seconds) noexcept {
    if (!scene_) {
        return std::unexpected(platform::Error("Animation player has no scene"));
    }
    if (!std::isfinite(time_seconds) || time_seconds < 0.0f) {
        return std::unexpected(platform::Error("Animation time must be non-negative"));
    }

    pose_.local_transforms.clear();
    pose_.local_transforms.reserve(scene_->nodes.size());
    for (const auto& node : scene_->nodes) {
        pose_.local_transforms.push_back(node.local_transform);
    }

    if (selected_animation_.has_value()) {
        if (*selected_animation_ >= scene_->animations.size()) {
            return std::unexpected(platform::Error("Animation index out of range"));
        }
        const auto& animation = scene_->animations[*selected_animation_];
        for (const auto& channel : animation.channels) {
            if (!channel.target_node.has_value() || *channel.target_node >= pose_.local_transforms.size()) {
                continue;
            }
            if (channel.sampler_index >= animation.samplers.size()) {
                return std::unexpected(platform::Error("Animation channel sampler index out of range"));
            }
            const auto& sampler = animation.samplers[channel.sampler_index];
            auto& transform = pose_.local_transforms[*channel.target_node];
            transform.matrix.reset();
            switch (channel.target_path) {
                case assets::AnimationTargetPath::kTranslation: {
                    auto sampled = sample_sampler(sampler, time_seconds);
                    if (!sampled) {
                        return std::unexpected(sampled.error());
                    }
                    if (sampled->size() != 3u) {
                        return std::unexpected(platform::Error("Translation channel must have 3 components"));
                    }
                    transform.translation = {(*sampled)[0], (*sampled)[1], (*sampled)[2]};
                    break;
                }
                case assets::AnimationTargetPath::kScale: {
                    auto sampled = sample_sampler(sampler, time_seconds);
                    if (!sampled) {
                        return std::unexpected(sampled.error());
                    }
                    if (sampled->size() != 3u) {
                        return std::unexpected(platform::Error("Scale channel must have 3 components"));
                    }
                    transform.scale = {(*sampled)[0], (*sampled)[1], (*sampled)[2]};
                    break;
                }
                case assets::AnimationTargetPath::kRotation: {
                    auto sampled = sample_rotation_sampler(sampler, time_seconds);
                    if (!sampled) {
                        return std::unexpected(sampled.error());
                    }
                    transform.rotation = *sampled;
                    break;
                }
                case assets::AnimationTargetPath::kWeights:
                    // Morph target weights are intentionally preserved in assets but have no
                    // runtime transform target in Phase 3B.
                    break;
            }
        }
    }

    current_time_ = time_seconds;
    update_world_and_skin_poses();
    return {};
}

void AnimationPlayer::update_world_and_skin_poses() noexcept {
    if (!scene_) {
        return;
    }
    pose_.world_transforms.assign(scene_->nodes.size(), compose_transform(Transform{}));
    std::vector<bool> visited(scene_->nodes.size(), false);

    if (scene_->default_scene_index.has_value() && *scene_->default_scene_index < scene_->scenes.size()) {
        for (std::size_t root_node : scene_->scenes[*scene_->default_scene_index].root_nodes) {
            compute_node_world(*scene_, pose_, root_node, glm::mat4(1.0f), visited);
        }
    }
    for (std::size_t node_index = 0; node_index < scene_->nodes.size(); ++node_index) {
        if (!scene_->nodes[node_index].parent_index.has_value()) {
            compute_node_world(*scene_, pose_, node_index, glm::mat4(1.0f), visited);
        }
    }

    pose_.skin_poses.resize(scene_->skins.size());
    for (std::size_t skin_index = 0; skin_index < scene_->skins.size(); ++skin_index) {
        const auto& skin = scene_->skins[skin_index];
        auto& skin_pose = pose_.skin_poses[skin_index];
        skin_pose.joint_matrices.resize(skin.joints.size());
        for (std::size_t joint_index = 0; joint_index < skin.joints.size(); ++joint_index) {
            const std::size_t node_index = skin.joints[joint_index];
            glm::mat4 joint_world(1.0f);
            if (node_index < pose_.world_transforms.size()) {
                joint_world = to_glm_mat4(pose_.world_transforms[node_index]);
            }
            glm::mat4 inverse_bind(1.0f);
            if (joint_index < skin.inverse_bind_matrices.size()) {
                inverse_bind = to_glm_mat4(skin.inverse_bind_matrices[joint_index]);
            }
            skin_pose.joint_matrices[joint_index] = to_array(joint_world * inverse_bind);
        }
    }
}

ScenePose& AnimationPlayer::pose() noexcept {
    return pose_;
}

const ScenePose& AnimationPlayer::pose() const noexcept {
    return pose_;
}

bool AnimationPlayer::is_playing() const noexcept {
    return playing_;
}

float AnimationPlayer::current_time() const noexcept {
    return current_time_;
}

std::optional<std::size_t> AnimationPlayer::selected_animation() const noexcept {
    return selected_animation_;
}

std::array<float, 16> compose_transform(const Transform& transform) noexcept {
    if (transform.matrix.has_value()) {
        return *transform.matrix;
    }

    const glm::mat4 translation = glm::translate(
        glm::mat4(1.0f),
        glm::vec3(transform.translation[0], transform.translation[1], transform.translation[2]));
    const glm::quat rotation(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                             transform.rotation[2]);
    const glm::mat4 rotation_matrix = glm::mat4_cast(normalize_quat(rotation));
    const glm::mat4 scale = glm::scale(
        glm::mat4(1.0f), glm::vec3(transform.scale[0], transform.scale[1], transform.scale[2]));

    return to_array(translation * rotation_matrix * scale);
}

} // namespace stellar::scene
