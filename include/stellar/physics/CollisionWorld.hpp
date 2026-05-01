#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "stellar/assets/CollisionAsset.hpp"

namespace stellar::physics {

namespace detail {

/** @brief Internal static broadphase AABB storage. */
struct CollisionAabb {
    std::array<float, 3> min{};
    std::array<float, 3> max{};
};

/** @brief Internal triangle reference used by the static broadphase. */
struct CollisionTriangleRef {
    std::size_t mesh_index = 0;
    std::size_t triangle_index = 0;
    CollisionAabb bounds{};
    std::array<float, 3> centroid{};
};

/** @brief Internal static broadphase node storage. */
struct CollisionBvhNode {
    CollisionAabb bounds{};
    std::uint32_t first = 0;
    std::uint32_t count = 0;
    std::uint32_t left = 0;
    std::uint32_t right = 0;
};

} // namespace detail

/**
 * @brief Result of a finite segment query against static collision geometry.
 */
struct RaycastHit {
    /** @brief True when the finite segment intersects a collision triangle. */
    bool hit = false;

    /** @brief Normalized segment parameter in [0, 1] for the nearest hit. */
    float t = 0.0F;

    /** @brief World-space hit position. */
    std::array<float, 3> position{};

    /** @brief World-space unit triangle normal from the collision asset winding. */
    std::array<float, 3> normal{};

    /** @brief Collision mesh index containing the hit triangle. */
    std::size_t mesh_index = 0;

    /** @brief Triangle index within the hit collision mesh. */
    std::size_t triangle_index = 0;
};

/**
 * @brief Result of a downward ground probe against static collision geometry.
 */
struct GroundProbeHit {
    /** @brief True when a floor-like triangle was found within the probe distance. */
    bool hit = false;

    /** @brief Distance from probe origin to the ground hit along negative Y. */
    float distance = 0.0F;

    /** @brief Full raycast hit data for the ground triangle. */
    RaycastHit raycast{};
};

/**
 * @brief Result of a static sweep-and-slide movement query.
 */
struct MoveResult {
    /** @brief Final world-space center position after applying movement. */
    std::array<float, 3> position{};

    /** @brief Remaining displacement after sweep/slide iterations have completed. */
    std::array<float, 3> velocity{};

    /** @brief True when movement touched any collision triangle. */
    bool hit = false;

    /** @brief True when the final position has floor-like support below it. */
    bool grounded = false;

    /** @brief Number of sweep/slide iterations used. */
    int iterations = 0;
};

/**
 * @brief Diagnostics for immutable static collision geometry and the most recent query.
 */
struct CollisionWorldStats {
    /** @brief Number of static collision meshes in the backing asset. */
    std::size_t mesh_count = 0;

    /** @brief Number of static collision triangles in the backing asset. */
    std::size_t triangle_count = 0;

    /** @brief Number of internal broadphase nodes built for triangle queries. */
    std::size_t broadphase_node_count = 0;

    /** @brief Number of narrowphase triangle tests performed by the most recent query. */
    std::size_t last_query_triangle_tests = 0;
};

/**
 * @brief Backend-neutral query wrapper for static level collision triangles.
 */
class CollisionWorld {
public:
    /**
     * @brief Construct a query world over immutable static collision asset data.
     */
    explicit CollisionWorld(const stellar::assets::LevelCollisionAsset& asset) noexcept;

    /**
     * @brief Cast a finite segment from origin along delta and return the nearest triangle hit.
     */
    [[nodiscard]] RaycastHit raycast(std::array<float, 3> origin,
                                     std::array<float, 3> delta) const noexcept;

    /**
     * @brief Probe downward along negative Y and return the nearest floor-like hit.
     */
    [[nodiscard]] GroundProbeHit probe_ground(std::array<float, 3> origin,
                                              float max_distance,
                                              float min_floor_normal_y = 0.5F) const noexcept;

    /**
     * @brief Move a sphere center by displacement using fixed-iteration sweep-and-slide.
     *
     * This is a conservative static-triangle movement helper, not a rigid body solver. Radius
     * handling uses triangle plane offsets and only accepts contacts whose projected point lies
     * inside the source triangle; triangle edges and vertices are not expanded into capsules.
     */
    [[nodiscard]] MoveResult move_sphere(std::array<float, 3> position,
                                         std::array<float, 3> displacement,
                                         float radius,
                                         int max_iterations = 3) const noexcept;

    /**
     * @brief Return the immutable static collision asset backing this query world.
     */
    [[nodiscard]] const stellar::assets::LevelCollisionAsset& asset() const noexcept;

    /**
     * @brief Return static broadphase diagnostics and the most recent query's triangle-test count.
     */
    [[nodiscard]] CollisionWorldStats stats() const noexcept;

private:
    void build_broadphase() noexcept;

    const stellar::assets::LevelCollisionAsset* asset_ = nullptr;
    std::vector<detail::CollisionTriangleRef> triangle_refs_;
    std::vector<detail::CollisionBvhNode> bvh_nodes_;
    mutable CollisionWorldStats stats_{};
};

} // namespace stellar::physics
