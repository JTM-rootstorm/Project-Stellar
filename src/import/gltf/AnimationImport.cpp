#include "ImporterPrivate.hpp"

#include <cmath>

namespace stellar::import::gltf {
namespace {

std::expected<stellar::assets::AnimationInterpolation, stellar::platform::Error>
to_animation_interpolation(cgltf_interpolation_type interpolation) {
    switch (interpolation) {
        case cgltf_interpolation_type_step:
            return stellar::assets::AnimationInterpolation::kStep;
        case cgltf_interpolation_type_cubic_spline:
            return stellar::assets::AnimationInterpolation::kCubicSpline;
        case cgltf_interpolation_type_linear:
            return stellar::assets::AnimationInterpolation::kLinear;
        default:
            return import_error("Unsupported animation interpolation mode");
    }
}

std::expected<stellar::assets::AnimationTargetPath, stellar::platform::Error>
to_animation_target_path(cgltf_animation_path_type path) {
    switch (path) {
        case cgltf_animation_path_type_translation:
            return stellar::assets::AnimationTargetPath::kTranslation;
        case cgltf_animation_path_type_rotation:
            return stellar::assets::AnimationTargetPath::kRotation;
        case cgltf_animation_path_type_scale:
            return stellar::assets::AnimationTargetPath::kScale;
        case cgltf_animation_path_type_weights:
            return stellar::assets::AnimationTargetPath::kWeights;
        default:
            return import_error("Unsupported animation target path");
    }
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_animation_sampler_target(const stellar::assets::AnimationSamplerAsset& sampler,
                                  stellar::assets::AnimationTargetPath target_path) {
    switch (target_path) {
        case stellar::assets::AnimationTargetPath::kTranslation:
        case stellar::assets::AnimationTargetPath::kScale:
            if (sampler.output_components != 3) {
                return import_error("Animation translation/scale output must be VEC3");
            }
            return {};
        case stellar::assets::AnimationTargetPath::kRotation:
            if (sampler.output_components != 4) {
                return import_error("Animation rotation output must be VEC4");
            }
            return {};
        case stellar::assets::AnimationTargetPath::kWeights:
            return import_error("Morph target weight animations are unsupported");
    }

    return import_error("Unsupported animation target path");
}

} // namespace

std::expected<stellar::assets::AnimationAsset, stellar::platform::Error>
load_animation(const cgltf_data* data, const cgltf_animation& source_animation) {
    stellar::assets::AnimationAsset animation;
    animation.name = duplicate_to_string(source_animation.name);

    animation.samplers.reserve(source_animation.samplers_count);
    for (cgltf_size i = 0; i < source_animation.samplers_count; ++i) {
        const cgltf_animation_sampler& source_sampler = source_animation.samplers[i];
        if (!source_sampler.input || source_sampler.input->component_type != cgltf_component_type_r_32f ||
            source_sampler.input->type != cgltf_type_scalar || source_sampler.input->count == 0) {
            return std::unexpected(stellar::platform::Error("Invalid animation sampler input"));
        }
        if (!source_sampler.output || source_sampler.output->count == 0) {
            return std::unexpected(stellar::platform::Error("Invalid animation sampler output"));
        }

        auto interpolation = to_animation_interpolation(source_sampler.interpolation);
        if (!interpolation) {
            return std::unexpected(interpolation.error());
        }

        const std::size_t output_components = cgltf_num_components(source_sampler.output->type);
        const std::size_t expected_outputs =
            source_sampler.input->count *
            (*interpolation == stellar::assets::AnimationInterpolation::kCubicSpline ? 3u : 1u);
        if (output_components == 0 || source_sampler.output->count != expected_outputs) {
            return std::unexpected(stellar::platform::Error("Animation sampler output count mismatch"));
        }
        if (source_sampler.output->component_type != cgltf_component_type_r_32f) {
            return std::unexpected(
                stellar::platform::Error("Animation sampler output must use float components"));
        }

        stellar::assets::AnimationSamplerAsset sampler;
        sampler.interpolation = *interpolation;
        sampler.output_components = output_components;
        sampler.input_times.resize(source_sampler.input->count);
        for (std::size_t key = 0; key < source_sampler.input->count; ++key) {
            cgltf_float time = 0.0f;
            if (!cgltf_accessor_read_float(source_sampler.input, key, &time, 1) ||
                !std::isfinite(time)) {
                return std::unexpected(stellar::platform::Error("Failed to read animation input"));
            }
            if (key > 0 && time <= sampler.input_times[key - 1]) {
                return std::unexpected(stellar::platform::Error(
                    "Animation sampler input times must be strictly increasing"));
            }
            sampler.input_times[key] = time;
        }

        sampler.output_values.resize(static_cast<std::size_t>(source_sampler.output->count) *
                                     output_components);
        std::vector<cgltf_float> value(output_components);
        for (std::size_t key = 0; key < source_sampler.output->count; ++key) {
            if (!cgltf_accessor_read_float(source_sampler.output, key, value.data(),
                                           output_components)) {
                return std::unexpected(stellar::platform::Error("Failed to read animation output"));
            }
            for (std::size_t component = 0; component < output_components; ++component) {
                if (!std::isfinite(value[component])) {
                    return std::unexpected(
                        stellar::platform::Error("Invalid animation output value"));
                }
                sampler.output_values[key * output_components + component] = value[component];
            }
        }

        animation.samplers.push_back(std::move(sampler));
    }

    animation.channels.reserve(source_animation.channels_count);
    for (cgltf_size i = 0; i < source_animation.channels_count; ++i) {
        const cgltf_animation_channel& source_channel = source_animation.channels[i];
        auto sampler_index = checked_index(
            cgltf_animation_sampler_index(&source_animation, source_channel.sampler),
            "Failed to resolve animation channel sampler index");
        if (!sampler_index) {
            return std::unexpected(sampler_index.error());
        }
        auto target_path = to_animation_target_path(source_channel.target_path);
        if (!target_path) {
            return std::unexpected(target_path.error());
        }
        if (*sampler_index >= animation.samplers.size()) {
            return std::unexpected(
                stellar::platform::Error("Animation channel sampler index exceeds sampler count"));
        }
        if (auto valid = validate_animation_sampler_target(animation.samplers[*sampler_index],
                                                          *target_path);
            !valid) {
            return std::unexpected(valid.error());
        }

        stellar::assets::AnimationChannelAsset channel;
        channel.sampler_index = *sampler_index;
        channel.target_path = *target_path;
        if (source_channel.target_node) {
            auto node_index = checked_index(cgltf_node_index(data, source_channel.target_node),
                                            "Failed to resolve animation channel target node");
            if (!node_index) {
                return std::unexpected(node_index.error());
            }
            channel.target_node = *node_index;
        }
        animation.channels.push_back(channel);
    }

    return animation;
}

} // namespace stellar::import::gltf
