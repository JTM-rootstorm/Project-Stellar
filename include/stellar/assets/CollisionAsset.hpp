#pragma once

#include <array>
#include <string>
#include <vector>

namespace stellar::assets {

/**
 * @brief Triangle in backend-neutral static level collision data.
 */
struct CollisionTriangle {
    /** @brief World-space first triangle vertex. */
    std::array<float, 3> a{};

    /** @brief World-space second triangle vertex. */
    std::array<float, 3> b{};

    /** @brief World-space third triangle vertex. */
    std::array<float, 3> c{};

    /** @brief Unit-length world-space triangle normal derived from winding. */
    std::array<float, 3> normal{};
};

/**
 * @brief Named set of static collision triangles and world-space bounds.
 */
struct CollisionMesh {
    /** @brief Collision mesh name from the source node or mesh. */
    std::string name;

    /** @brief World-space collision triangles. */
    std::vector<CollisionTriangle> triangles;

    /** @brief World-space axis-aligned bounds minimum. */
    std::array<float, 3> bounds_min{};

    /** @brief World-space axis-aligned bounds maximum. */
    std::array<float, 3> bounds_max{};
};

/**
 * @brief Backend-neutral static level collision data extracted from a scene.
 */
struct LevelCollisionAsset {
    /** @brief Collision meshes extracted from collision-marked scene nodes. */
    std::vector<CollisionMesh> meshes;

    /** @brief Aggregate world-space bounds minimum for all collision meshes. */
    std::array<float, 3> bounds_min{};

    /** @brief Aggregate world-space bounds maximum for all collision meshes. */
    std::array<float, 3> bounds_max{};
};

} // namespace stellar::assets
