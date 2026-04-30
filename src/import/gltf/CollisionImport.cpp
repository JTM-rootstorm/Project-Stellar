#include "ImporterPrivate.hpp"

#include "stellar/scene/AnimationRuntime.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace stellar::import::gltf {
namespace {

using Mat4 = std::array<float, 16>;
using Vec3 = std::array<float, 3>;

bool starts_with(std::string_view value, std::string_view prefix) noexcept {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool is_direct_collision_node_name(std::string_view name) noexcept {
    return starts_with(name, "COL_") || starts_with(name, "Collision_");
}

Mat4 multiply(const Mat4& lhs, const Mat4& rhs) noexcept {
    Mat4 out{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            float value = 0.0F;
            for (std::size_t k = 0; k < 4; ++k) {
                value += lhs[k * 4 + row] * rhs[column * 4 + k];
            }
            out[column * 4 + row] = value;
        }
    }
    return out;
}

Vec3 transform_point(const Mat4& matrix, const Vec3& point) noexcept {
    return {matrix[0] * point[0] + matrix[4] * point[1] + matrix[8] * point[2] + matrix[12],
            matrix[1] * point[0] + matrix[5] * point[1] + matrix[9] * point[2] + matrix[13],
            matrix[2] * point[0] + matrix[6] * point[1] + matrix[10] * point[2] + matrix[14]};
}

Vec3 subtract(const Vec3& lhs, const Vec3& rhs) noexcept {
    return {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2]};
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) noexcept {
    return {lhs[1] * rhs[2] - lhs[2] * rhs[1], lhs[2] * rhs[0] - lhs[0] * rhs[2],
            lhs[0] * rhs[1] - lhs[1] * rhs[0]};
}

float dot(const Vec3& lhs, const Vec3& rhs) noexcept {
    return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
}

bool normalize(Vec3& value) noexcept {
    const float length_squared = dot(value, value);
    if (!std::isfinite(length_squared) || length_squared <= 0.00000001F) {
        return false;
    }
    const float inv_length = 1.0F / std::sqrt(length_squared);
    value = {value[0] * inv_length, value[1] * inv_length, value[2] * inv_length};
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

void include_point(Vec3& bounds_min, Vec3& bounds_max, const Vec3& point) noexcept {
    for (std::size_t axis = 0; axis < 3; ++axis) {
        bounds_min[axis] = std::min(bounds_min[axis], point[axis]);
        bounds_max[axis] = std::max(bounds_max[axis], point[axis]);
    }
}

void include_mesh_bounds(stellar::assets::LevelCollisionAsset& level,
                         const stellar::assets::CollisionMesh& mesh) noexcept {
    include_point(level.bounds_min, level.bounds_max, mesh.bounds_min);
    include_point(level.bounds_min, level.bounds_max, mesh.bounds_max);
}

std::string collision_mesh_name(const stellar::scene::Node& node,
                                const stellar::assets::MeshAsset& mesh) {
    if (!node.name.empty()) {
        return node.name;
    }
    if (!mesh.name.empty()) {
        return mesh.name;
    }
    return "collision_mesh";
}

std::expected<stellar::assets::CollisionMesh, stellar::platform::Error>
extract_collision_mesh(const stellar::scene::Node& node,
                       const stellar::assets::MeshAsset& mesh,
                       const Mat4& world_transform) {
    stellar::assets::CollisionMesh collision_mesh;
    collision_mesh.name = collision_mesh_name(node, mesh);
    collision_mesh.bounds_min = {std::numeric_limits<float>::max(),
                                 std::numeric_limits<float>::max(),
                                 std::numeric_limits<float>::max()};
    collision_mesh.bounds_max = {std::numeric_limits<float>::lowest(),
                                 std::numeric_limits<float>::lowest(),
                                 std::numeric_limits<float>::lowest()};

    for (const auto& primitive : mesh.primitives) {
        if (primitive.topology != stellar::assets::PrimitiveTopology::kTriangles ||
            primitive.indices.size() < 3 || primitive.indices.size() % 3 != 0) {
            return std::unexpected(stellar::platform::Error(
                "Collision-marked mesh primitive cannot produce triangle geometry"));
        }

        for (std::size_t i = 0; i < primitive.indices.size(); i += 3) {
            const std::uint32_t i0 = primitive.indices[i];
            const std::uint32_t i1 = primitive.indices[i + 1];
            const std::uint32_t i2 = primitive.indices[i + 2];
            if (i0 >= primitive.vertices.size() || i1 >= primitive.vertices.size() ||
                i2 >= primitive.vertices.size()) {
                return std::unexpected(stellar::platform::Error(
                    "Collision-marked mesh primitive has an out-of-range triangle index"));
            }

            stellar::assets::CollisionTriangle triangle;
            triangle.a = transform_point(world_transform, primitive.vertices[i0].position);
            triangle.b = transform_point(world_transform, primitive.vertices[i1].position);
            triangle.c = transform_point(world_transform, primitive.vertices[i2].position);
            triangle.normal = cross(subtract(triangle.b, triangle.a),
                                    subtract(triangle.c, triangle.a));
            if (!normalize(triangle.normal)) {
                return std::unexpected(stellar::platform::Error(
                    "Collision-marked mesh primitive has a degenerate triangle"));
            }

            include_point(collision_mesh.bounds_min, collision_mesh.bounds_max, triangle.a);
            include_point(collision_mesh.bounds_min, collision_mesh.bounds_max, triangle.b);
            include_point(collision_mesh.bounds_min, collision_mesh.bounds_max, triangle.c);
            collision_mesh.triangles.push_back(triangle);
        }
    }

    if (collision_mesh.triangles.empty()) {
        return std::unexpected(stellar::platform::Error(
            "Collision-marked mesh does not contain triangle geometry"));
    }

    return collision_mesh;
}

std::expected<void, stellar::platform::Error>
walk_collision_nodes(const stellar::assets::SceneAsset& scene,
                     stellar::assets::LevelCollisionAsset& level,
                     std::size_t node_index,
                     const Mat4& parent_world,
                     bool inherited_collision,
                     std::vector<bool>& visited) {
    if (node_index >= scene.nodes.size()) {
        return std::unexpected(stellar::platform::Error("Scene root node index exceeds node count"));
    }
    if (visited[node_index]) {
        return {};
    }
    visited[node_index] = true;

    const auto& node = scene.nodes[node_index];
    const Mat4 local = stellar::scene::compose_transform(node.local_transform);
    const Mat4 world = multiply(parent_world, local);
    const bool direct_collision = is_direct_collision_node_name(node.name);
    const bool extract_node = direct_collision || inherited_collision;
    const bool child_collision = inherited_collision || node.name == "Collision";

    if (extract_node) {
        for (const auto& instance : node.mesh_instances) {
            if (instance.mesh_index >= scene.meshes.size()) {
                return std::unexpected(stellar::platform::Error(
                    "Collision-marked node mesh index exceeds mesh count"));
            }
            auto collision_mesh = extract_collision_mesh(node, scene.meshes[instance.mesh_index], world);
            if (!collision_mesh) {
                return std::unexpected(collision_mesh.error());
            }
            include_mesh_bounds(level, *collision_mesh);
            level.meshes.push_back(std::move(*collision_mesh));
        }
    }

    for (std::size_t child : node.children) {
        auto child_result = walk_collision_nodes(scene, level, child, world, child_collision, visited);
        if (!child_result) {
            return child_result;
        }
    }

    return {};
}

std::vector<std::size_t> collision_roots(const stellar::assets::SceneAsset& scene) {
    if (scene.default_scene_index.has_value() && *scene.default_scene_index < scene.scenes.size()) {
        return scene.scenes[*scene.default_scene_index].root_nodes;
    }

    std::vector<std::size_t> roots;
    for (std::size_t i = 0; i < scene.nodes.size(); ++i) {
        if (!scene.nodes[i].parent_index.has_value()) {
            roots.push_back(i);
        }
    }
    return roots;
}

} // namespace

std::expected<stellar::assets::LevelCollisionAsset, stellar::platform::Error>
extract_level_collision(const stellar::assets::SceneAsset& scene) {
    stellar::assets::LevelCollisionAsset level;
    level.bounds_min = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max()};
    level.bounds_max = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest()};

    std::vector<bool> visited(scene.nodes.size(), false);
    for (std::size_t root : collision_roots(scene)) {
        auto result = walk_collision_nodes(scene, level, root, identity_matrix(), false, visited);
        if (!result) {
            return std::unexpected(result.error());
        }
    }

    if (level.meshes.empty()) {
        level.bounds_min = {0.0F, 0.0F, 0.0F};
        level.bounds_max = {0.0F, 0.0F, 0.0F};
    }

    return level;
}

} // namespace stellar::import::gltf
