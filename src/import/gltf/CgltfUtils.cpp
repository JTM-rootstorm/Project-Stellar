#include "ImporterPrivate.hpp"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <cmath>
#include <limits>

namespace stellar::import::gltf {

void CgltfDataDeleter::operator()(cgltf_data* data) const noexcept {
    if (data) {
        cgltf_free(data);
    }
}

std::string duplicate_to_string(const char* value) {
    return value ? std::string(value) : std::string();
}

std::filesystem::path resolve_relative_path(const std::filesystem::path& base,
                                            const char* relative) {
    if (!relative || !*relative) {
        return {};
    }

    return base.parent_path() / std::filesystem::path(relative);
}

[[nodiscard]] std::expected<std::size_t, stellar::platform::Error>
checked_index(cgltf_size value, const char* error_message) {
    if (value == static_cast<cgltf_size>(-1)) {
        return std::unexpected(stellar::platform::Error(error_message));
    }

    return static_cast<std::size_t>(value);
}

[[nodiscard]] std::unexpected<stellar::platform::Error> import_error(const char* message) {
    return std::unexpected(stellar::platform::Error(message));
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_required_extensions(const cgltf_data* data) {
    for (cgltf_size i = 0; i < data->extensions_required_count; ++i) {
        const char* extension = data->extensions_required[i];
        const std::string extension_name = duplicate_to_string(extension);
        if (extension_name == "KHR_texture_transform" ||
            extension_name == "KHR_materials_unlit") {
            continue;
        }
        return std::unexpected(stellar::platform::Error(
            "Unsupported required glTF extension: " + extension_name));
    }

    return {};
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_float_accessor(const cgltf_accessor* accessor, cgltf_type type, cgltf_size expected_count,
                        const char* label) {
    if (!accessor) {
        return import_error(label);
    }
    if (accessor->component_type != cgltf_component_type_r_32f || accessor->type != type) {
        return import_error(label);
    }
    if (accessor->count != expected_count) {
        return import_error(label);
    }

    return {};
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_normalized_float_attribute_accessor(const cgltf_accessor* accessor, cgltf_type type,
                                             cgltf_size expected_count, const char* label) {
    if (!accessor) {
        return import_error(label);
    }
    if (accessor->type != type || accessor->count != expected_count) {
        return import_error(label);
    }

    switch (accessor->component_type) {
        case cgltf_component_type_r_8u:
        case cgltf_component_type_r_16u:
            if (!accessor->normalized) {
                return import_error(label);
            }
            return {};
        case cgltf_component_type_r_32f:
            return {};
        default:
            return import_error(label);
    }
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_index_accessor(const cgltf_accessor* accessor) {
    if (!accessor || accessor->type != cgltf_type_scalar) {
        return import_error("Invalid index accessor");
    }

    switch (accessor->component_type) {
        case cgltf_component_type_r_8u:
        case cgltf_component_type_r_16u:
        case cgltf_component_type_r_32u:
            return {};
        default:
            return import_error("Invalid index accessor component type");
    }
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_joints_accessor(const cgltf_accessor* accessor, cgltf_size expected_count) {
    if (!accessor || accessor->type != cgltf_type_vec4 || accessor->count != expected_count) {
        return import_error("Invalid JOINTS_0 accessor");
    }

    switch (accessor->component_type) {
        case cgltf_component_type_r_8u:
        case cgltf_component_type_r_16u:
            return {};
        default:
            return import_error("Invalid JOINTS_0 accessor component type");
    }
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_weights_accessor(const cgltf_accessor* accessor, cgltf_size expected_count) {
    if (!accessor || accessor->type != cgltf_type_vec4 || accessor->count != expected_count) {
        return import_error("Invalid WEIGHTS_0 accessor");
    }

    switch (accessor->component_type) {
        case cgltf_component_type_r_8u:
        case cgltf_component_type_r_16u:
            if (!accessor->normalized) {
                return import_error("Integer WEIGHTS_0 accessor must be normalized");
            }
            return {};
        case cgltf_component_type_r_32f:
            return {};
        default:
            return import_error("Invalid WEIGHTS_0 accessor component type");
    }
}

[[nodiscard]] std::expected<std::array<float, 2>, stellar::platform::Error>
read_vec2(const cgltf_accessor* accessor, std::size_t index, const char* error_message) {
    cgltf_float value[4] = {};
    if (!cgltf_accessor_read_float(accessor, index, value, 2)) {
        return import_error(error_message);
    }

    return std::array<float, 2>{value[0], value[1]};
}

[[nodiscard]] std::expected<std::array<float, 3>, stellar::platform::Error>
read_vec3(const cgltf_accessor* accessor, std::size_t index, const char* error_message) {
    cgltf_float value[4] = {};
    if (!cgltf_accessor_read_float(accessor, index, value, 3)) {
        return import_error(error_message);
    }

    return std::array<float, 3>{value[0], value[1], value[2]};
}

[[nodiscard]] std::expected<std::array<float, 4>, stellar::platform::Error>
read_vec4(const cgltf_accessor* accessor, std::size_t index, const char* error_message) {
    cgltf_float value[4] = {};
    if (!cgltf_accessor_read_float(accessor, index, value, 4)) {
        return import_error(error_message);
    }

    return std::array<float, 4>{value[0], value[1], value[2], value[3]};
}

[[nodiscard]] std::expected<std::array<std::uint16_t, 4>, stellar::platform::Error>
read_u16_vec4(const cgltf_accessor* accessor, std::size_t index, const char* error_message) {
    cgltf_uint value[4] = {};
    if (!cgltf_accessor_read_uint(accessor, index, value, 4)) {
        return import_error(error_message);
    }

    for (cgltf_uint component : value) {
        if (component > std::numeric_limits<std::uint16_t>::max()) {
            return import_error(error_message);
        }
    }

    return std::array<std::uint16_t, 4>{static_cast<std::uint16_t>(value[0]),
                                       static_cast<std::uint16_t>(value[1]),
                                       static_cast<std::uint16_t>(value[2]),
                                       static_cast<std::uint16_t>(value[3])};
}

[[nodiscard]] std::expected<std::array<float, 16>, stellar::platform::Error>
read_mat4(const cgltf_accessor* accessor, std::size_t index, const char* error_message) {
    cgltf_float value[16] = {};
    if (!cgltf_accessor_read_float(accessor, index, value, 16)) {
        return import_error(error_message);
    }

    std::array<float, 16> result{};
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = value[i];
    }
    return result;
}

bool normalize_weights(std::array<float, 4>& weights) noexcept {
    float sum = 0.0f;
    for (float weight : weights) {
        if (!std::isfinite(weight) || weight < 0.0f) {
            return false;
        }
        sum += weight;
    }

    if (!std::isfinite(sum) || sum <= 0.0f) {
        return false;
    }

    for (float& weight : weights) {
        weight /= sum;
    }
    return true;
}

std::array<float, 16> identity_matrix() noexcept {
    return {1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f};
}

std::array<float, 16> node_matrix(const cgltf_node& node) {
    std::array<float, 16> matrix{};
    if (node.has_matrix) {
        for (std::size_t i = 0; i < 16; ++i) {
            matrix[i] = node.matrix[i];
        }
    } else {
        matrix = {1.0f, 0.0f, 0.0f, 0.0f,
                  0.0f, 1.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 1.0f, 0.0f,
                  0.0f, 0.0f, 0.0f, 1.0f};
    }
    return matrix;
}

} // namespace stellar::import::gltf
