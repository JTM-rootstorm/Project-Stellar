#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace stellar::scene {

/**
 * @brief Local transform data for a scene node.
 *
 * A node may express its transform as TRS or as a raw matrix. When `matrix`
 * has a value, it takes precedence over the TRS fields.
 */
struct Transform {
    std::array<float, 3> translation{0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
    std::optional<std::array<float, 16>> matrix;
};

/**
 * @brief Mesh binding for a node.
 */
struct MeshInstance {
    std::size_t mesh_index = 0;
};

/**
 * @brief A node in the scene hierarchy.
 */
struct Node {
    std::string name;
    std::optional<std::size_t> parent_index;
    std::vector<std::size_t> children;
    Transform local_transform;
    std::vector<MeshInstance> mesh_instances;
    std::optional<std::size_t> skin_index;
};

/**
 * @brief A scene root set.
 */
struct Scene {
    std::string name;
    std::vector<std::size_t> root_nodes;
};

} // namespace stellar::scene
