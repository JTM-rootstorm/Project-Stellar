#pragma once

#include <array>
#include <cstdint>
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
    /** @brief Collision mesh name from the source level model, entity, or mesh. */
    std::string name;

    /** @brief World-space collision triangles. */
    std::vector<CollisionTriangle> triangles;

    /** @brief World-space axis-aligned bounds minimum. */
    std::array<float, 3> bounds_min{};

    /** @brief World-space axis-aligned bounds maximum. */
    std::array<float, 3> bounds_max{};
};

/**
 * @brief Plane equation used by optional source collision hulls.
 */
struct CollisionPlane {
    /** @brief Plane normal. */
    std::array<float, 3> normal{};

    /** @brief Plane distance from the origin. */
    float distance = 0.0F;
};

/**
 * @brief Optional convex collision hull preserved from a source level model or entity.
 */
struct CollisionHull {
    /** @brief Hull name from the source level model or entity. */
    std::string name;

    /** @brief Planes defining the convex hull. */
    std::vector<CollisionPlane> planes;

    /** @brief World-space axis-aligned bounds minimum. */
    std::array<float, 3> bounds_min{};

    /** @brief World-space axis-aligned bounds maximum. */
    std::array<float, 3> bounds_max{};

    /** @brief Source format contents flags preserved for diagnostics and later import use. */
    std::uint32_t source_contents = 0;
};

/**
 * @brief Backend-neutral static level collision data for a playable level.
 */
struct LevelCollisionAsset {
    /** @brief Source-neutral static triangle collision meshes. */
    std::vector<CollisionMesh> meshes;

    /** @brief Optional source collision hulls for later query paths. */
    std::vector<CollisionHull> hulls;

    /** @brief Aggregate world-space bounds minimum for all collision meshes. */
    std::array<float, 3> bounds_min{};

    /** @brief Aggregate world-space bounds maximum for all collision meshes. */
    std::array<float, 3> bounds_max{};
};

} // namespace stellar::assets
