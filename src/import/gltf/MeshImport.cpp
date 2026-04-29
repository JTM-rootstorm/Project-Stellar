#include "ImporterPrivate.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace stellar::import::gltf {
namespace {

bool is_finite_vec3(const std::array<float, 3>& value) noexcept {
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

std::array<float, 3> subtract(const std::array<float, 3>& lhs,
                              const std::array<float, 3>& rhs) noexcept {
    return {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2]};
}

std::array<float, 3> add(const std::array<float, 3>& lhs,
                         const std::array<float, 3>& rhs) noexcept {
    return {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
}

std::array<float, 3> multiply(const std::array<float, 3>& value, float scale) noexcept {
    return {value[0] * scale, value[1] * scale, value[2] * scale};
}

float dot(const std::array<float, 3>& lhs, const std::array<float, 3>& rhs) noexcept {
    return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
}

std::array<float, 3> cross(const std::array<float, 3>& lhs,
                           const std::array<float, 3>& rhs) noexcept {
    return {lhs[1] * rhs[2] - lhs[2] * rhs[1], lhs[2] * rhs[0] - lhs[0] * rhs[2],
            lhs[0] * rhs[1] - lhs[1] * rhs[0]};
}

float length_squared(const std::array<float, 3>& value) noexcept {
    return dot(value, value);
}

bool normalize_in_place(std::array<float, 3>& value) noexcept {
    const float len_sq = length_squared(value);
    if (!std::isfinite(len_sq) || len_sq <= 0.00000001f) {
        return false;
    }

    const float inv_len = 1.0f / std::sqrt(len_sq);
    value = multiply(value, inv_len);
    return is_finite_vec3(value);
}

bool generate_tangents(std::vector<stellar::assets::StaticVertex>& vertices,
                       const std::vector<std::uint32_t>& indices) noexcept {
    if (vertices.empty() || indices.empty() || indices.size() % 3 != 0) {
        return false;
    }

    std::vector<std::array<float, 3>> tangent_sums(vertices.size(), {0.0f, 0.0f, 0.0f});
    std::vector<std::array<float, 3>> bitangent_sums(vertices.size(), {0.0f, 0.0f, 0.0f});

    for (std::size_t i = 0; i < indices.size(); i += 3) {
        const std::uint32_t i0 = indices[i];
        const std::uint32_t i1 = indices[i + 1];
        const std::uint32_t i2 = indices[i + 2];
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            return false;
        }

        const auto edge1 = subtract(vertices[i1].position, vertices[i0].position);
        const auto edge2 = subtract(vertices[i2].position, vertices[i0].position);
        const float du1 = vertices[i1].uv0[0] - vertices[i0].uv0[0];
        const float dv1 = vertices[i1].uv0[1] - vertices[i0].uv0[1];
        const float du2 = vertices[i2].uv0[0] - vertices[i0].uv0[0];
        const float dv2 = vertices[i2].uv0[1] - vertices[i0].uv0[1];
        const float determinant = du1 * dv2 - du2 * dv1;
        if (!std::isfinite(determinant) || std::abs(determinant) <= 0.00000001f) {
            return false;
        }

        const float scale = 1.0f / determinant;
        const auto tangent = multiply(subtract(multiply(edge1, dv2), multiply(edge2, dv1)), scale);
        const auto bitangent = multiply(subtract(multiply(edge2, du1), multiply(edge1, du2)), scale);
        if (!is_finite_vec3(tangent) || !is_finite_vec3(bitangent) ||
            length_squared(tangent) <= 0.00000001f || length_squared(bitangent) <= 0.00000001f) {
            return false;
        }

        tangent_sums[i0] = add(tangent_sums[i0], tangent);
        tangent_sums[i1] = add(tangent_sums[i1], tangent);
        tangent_sums[i2] = add(tangent_sums[i2], tangent);
        bitangent_sums[i0] = add(bitangent_sums[i0], bitangent);
        bitangent_sums[i1] = add(bitangent_sums[i1], bitangent);
        bitangent_sums[i2] = add(bitangent_sums[i2], bitangent);
    }

    for (std::size_t i = 0; i < vertices.size(); ++i) {
        std::array<float, 3> normal = vertices[i].normal;
        if (!normalize_in_place(normal)) {
            return false;
        }

        std::array<float, 3> tangent = subtract(tangent_sums[i], multiply(normal, dot(normal, tangent_sums[i])));
        if (!normalize_in_place(tangent)) {
            return false;
        }

        const auto bitangent_cross = cross(normal, tangent);
        const float handedness = dot(bitangent_cross, bitangent_sums[i]) < 0.0f ? -1.0f : 1.0f;
        vertices[i].tangent = {tangent[0], tangent[1], tangent[2], handedness};
    }

    return true;
}

bool primitive_uses_normal_texture(const cgltf_data* data,
                                   const cgltf_primitive& primitive) noexcept {
    if (!primitive.material || !primitive.material->normal_texture.texture) {
        return false;
    }

    return cgltf_material_index(data, primitive.material) != static_cast<cgltf_size>(-1);
}


} // namespace

std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
load_mesh(const cgltf_data* data, const cgltf_mesh& source_mesh) {
    stellar::assets::MeshAsset mesh;
    mesh.name = duplicate_to_string(source_mesh.name);

    // Static import policy: fail on data needed to draw the primitive correctly, import common
    // draw-affecting optional attributes with asset/runtime support, and ignore optional features
    // without a runtime representation such as UV2+, morph targets, cameras, lights, and extensions.
    for (cgltf_size p = 0; p < source_mesh.primitives_count; ++p) {
        const cgltf_primitive& primitive = source_mesh.primitives[p];
        if (primitive.type != cgltf_primitive_type_triangles) {
            return std::unexpected(stellar::platform::Error("Unsupported primitive topology"));
        }

        const cgltf_accessor* position_accessor =
            cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
        if (!position_accessor) {
            return std::unexpected(stellar::platform::Error("Primitive is missing positions"));
        }
        if (position_accessor->count == 0) {
            return std::unexpected(stellar::platform::Error("Primitive has no positions"));
        }
        if (auto valid = validate_float_accessor(position_accessor, cgltf_type_vec3,
                                                 position_accessor->count,
                                                 "Invalid POSITION accessor");
            !valid) {
            return std::unexpected(valid.error());
        }

        std::vector<stellar::assets::StaticVertex> vertices(position_accessor->count);
        std::array<float, 3> bounds_min{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
        };
        std::array<float, 3> bounds_max{
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
        };

        for (std::size_t i = 0; i < position_accessor->count; ++i) {
            auto position = read_vec3(position_accessor, i, "Failed to read POSITION accessor");
            if (!position) {
                return std::unexpected(position.error());
            }
            vertices[i].position = *position;
            for (std::size_t axis = 0; axis < 3; ++axis) {
                bounds_min[axis] = std::min(bounds_min[axis], (*position)[axis]);
                bounds_max[axis] = std::max(bounds_max[axis], (*position)[axis]);
            }
        }

        if (const auto* normal_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0)) {
            if (auto valid = validate_float_accessor(normal_accessor, cgltf_type_vec3,
                                                     position_accessor->count,
                                                     "Invalid NORMAL accessor");
                !valid) {
                return std::unexpected(valid.error());
            }
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                auto normal = read_vec3(normal_accessor, i, "Failed to read NORMAL accessor");
                if (!normal) {
                    return std::unexpected(normal.error());
                }
                vertices[i].normal = *normal;
            }
        }

        if (const auto* texcoord_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0)) {
            if (auto valid = validate_normalized_float_attribute_accessor(
                    texcoord_accessor, cgltf_type_vec2, position_accessor->count,
                    "Invalid TEXCOORD_0 accessor");
                !valid) {
                return std::unexpected(valid.error());
            }
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                auto texcoord =
                    read_vec2(texcoord_accessor, i, "Failed to read TEXCOORD_0 accessor");
                if (!texcoord) {
                    return std::unexpected(texcoord.error());
                }
                vertices[i].uv0 = *texcoord;
            }
        }

        if (const auto* texcoord_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 1)) {
            if (auto valid = validate_normalized_float_attribute_accessor(
                    texcoord_accessor, cgltf_type_vec2, position_accessor->count,
                    "Invalid TEXCOORD_1 accessor");
                !valid) {
                return std::unexpected(valid.error());
            }
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                auto texcoord =
                    read_vec2(texcoord_accessor, i, "Failed to read TEXCOORD_1 accessor");
                if (!texcoord) {
                    return std::unexpected(texcoord.error());
                }
                vertices[i].uv1 = *texcoord;
            }
        }

        bool has_colors = false;
        if (const auto* color_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_color, 0)) {
            const bool is_vec3 = color_accessor->type == cgltf_type_vec3;
            const bool is_vec4 = color_accessor->type == cgltf_type_vec4;
            if (!is_vec3 && !is_vec4) {
                return std::unexpected(stellar::platform::Error("Invalid COLOR_0 accessor"));
            }
            if (auto valid = validate_normalized_float_attribute_accessor(
                    color_accessor, color_accessor->type, position_accessor->count,
                    "Invalid COLOR_0 accessor");
                !valid) {
                return std::unexpected(valid.error());
            }
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                if (is_vec4) {
                    auto color = read_vec4(color_accessor, i, "Failed to read COLOR_0 accessor");
                    if (!color) {
                        return std::unexpected(color.error());
                    }
                    vertices[i].color = *color;
                } else {
                    auto color = read_vec3(color_accessor, i, "Failed to read COLOR_0 accessor");
                    if (!color) {
                        return std::unexpected(color.error());
                    }
                    vertices[i].color = {(*color)[0], (*color)[1], (*color)[2], 1.0f};
                }
            }
            has_colors = true;
        }

        bool has_skinning = false;
        const auto* joints_accessor =
            cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0);
        const auto* weights_accessor =
            cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0);
        if (joints_accessor || weights_accessor) {
            if (!joints_accessor || !weights_accessor) {
                return std::unexpected(
                    stellar::platform::Error("JOINTS_0 and WEIGHTS_0 must be provided together"));
            }
            if (auto valid = validate_joints_accessor(joints_accessor, position_accessor->count);
                !valid) {
                return std::unexpected(valid.error());
            }
            if (auto valid = validate_weights_accessor(weights_accessor, position_accessor->count);
                !valid) {
                return std::unexpected(valid.error());
            }
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                auto joints = read_u16_vec4(joints_accessor, i, "Failed to read JOINTS_0 accessor");
                if (!joints) {
                    return std::unexpected(joints.error());
                }
                auto weights = read_vec4(weights_accessor, i, "Failed to read WEIGHTS_0 accessor");
                if (!weights) {
                    return std::unexpected(weights.error());
                }
                if (!normalize_weights(*weights)) {
                    return std::unexpected(stellar::platform::Error("Invalid WEIGHTS_0 values"));
                }
                vertices[i].joints0 = *joints;
                vertices[i].weights0 = *weights;
            }
            has_skinning = true;
        }

        bool has_tangents = false;
        if (const auto* tangent_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_tangent, 0)) {
            if (auto valid = validate_float_accessor(tangent_accessor, cgltf_type_vec4,
                                                     position_accessor->count,
                                                     "Invalid TANGENT accessor");
                !valid) {
                return std::unexpected(valid.error());
            }
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                auto tangent = read_vec4(tangent_accessor, i, "Failed to read TANGENT accessor");
                if (!tangent) {
                    return std::unexpected(tangent.error());
                }
                vertices[i].tangent = *tangent;
            }
            has_tangents = true;
        }

        std::vector<std::uint32_t> indices;
        if (primitive.indices) {
            if (auto valid = validate_index_accessor(primitive.indices); !valid) {
                return std::unexpected(valid.error());
            }
            indices.resize(primitive.indices->count);
            for (std::size_t i = 0; i < primitive.indices->count; ++i) {
                const auto index = cgltf_accessor_read_index(primitive.indices, i);
                if (index >= vertices.size() ||
                    index > static_cast<cgltf_size>(std::numeric_limits<std::uint32_t>::max())) {
                    return std::unexpected(stellar::platform::Error("Index exceeds vertex count"));
                }
                indices[i] = static_cast<std::uint32_t>(index);
            }
        } else {
            if (vertices.size() > std::numeric_limits<std::uint32_t>::max()) {
                return std::unexpected(stellar::platform::Error("Primitive has too many vertices"));
            }
            indices.reserve(vertices.size());
            for (std::size_t i = 0; i < vertices.size(); ++i) {
                indices.push_back(static_cast<std::uint32_t>(i));
            }
        }

        if (!has_tangents && primitive_uses_normal_texture(data, primitive)) {
            has_tangents = generate_tangents(vertices, indices);
        }

        stellar::assets::MeshPrimitive mesh_primitive;
        mesh_primitive.topology = stellar::assets::PrimitiveTopology::kTriangles;
        mesh_primitive.vertices = std::move(vertices);
        mesh_primitive.indices = std::move(indices);
        mesh_primitive.has_tangents = has_tangents;
        mesh_primitive.has_colors = has_colors;
        mesh_primitive.has_skinning = has_skinning;
        mesh_primitive.bounds_min = bounds_min;
        mesh_primitive.bounds_max = bounds_max;
        if (primitive.material) {
            auto index = checked_index(cgltf_material_index(data, primitive.material),
                                       "Failed to resolve primitive material index");
            if (!index) {
                return std::unexpected(index.error());
            }
            mesh_primitive.material_index = *index;
        }
        mesh.primitives.push_back(std::move(mesh_primitive));
    }

    return mesh;
}

} // namespace stellar::import::gltf
