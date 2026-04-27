#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace stellar::assets {

/**
 * @brief Interpolation mode for stored animation sampler data.
 */
enum class AnimationInterpolation {
    kLinear,
    kStep,
    kCubicSpline,
};

/**
 * @brief Node property targeted by an animation channel.
 */
enum class AnimationTargetPath {
    kTranslation,
    kRotation,
    kScale,
    kWeights,
};

/**
 * @brief CPU-side sampled animation data for one glTF animation sampler.
 */
struct AnimationSamplerAsset {
    /** @brief Keyframe input times in seconds. */
    std::vector<float> input_times;

    /** @brief Output values stored as tightly packed float vectors per keyframe. */
    std::vector<float> output_values;

    /** @brief Number of scalar components in one output value. */
    std::size_t output_components = 0;

    /** @brief Interpolation mode to use when this sampler is evaluated later. */
    AnimationInterpolation interpolation = AnimationInterpolation::kLinear;
};

/**
 * @brief CPU-side binding from an animation sampler to a scene node property.
 */
struct AnimationChannelAsset {
    /** @brief Index of the sampler in the containing animation. */
    std::size_t sampler_index = 0;

    /** @brief Optional target node index, absent only if the source channel omits a target node. */
    std::optional<std::size_t> target_node;

    /** @brief Property targeted by the channel. */
    AnimationTargetPath target_path = AnimationTargetPath::kTranslation;
};

/**
 * @brief CPU-side animation data imported from a glTF animation.
 */
struct AnimationAsset {
    /** @brief Optional authoring name for the animation. */
    std::string name;

    /** @brief Samplers containing keyframe times and output values. */
    std::vector<AnimationSamplerAsset> samplers;

    /** @brief Channels binding samplers to scene node properties. */
    std::vector<AnimationChannelAsset> channels;
};

} // namespace stellar::assets
