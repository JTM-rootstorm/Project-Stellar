#include "stellar/world/CollisionValidation.hpp"

#include "stellar/core/WorldAxes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <unordered_map>

namespace stellar::world {
namespace {

constexpr float kNearZeroNormalLengthSq = 1.0e-8F;
constexpr float kNearZeroTriangleAreaSq = 1.0e-10F;
constexpr float kBoundsTolerance = 1.0e-4F;
constexpr float kWalkableUpNormalDot = 0.5F;

using Vec3 = std::array<float, 3>;

bool is_finite(float value) noexcept {
    return std::isfinite(value);
}

bool is_finite_vec3(const Vec3& value) noexcept {
    return is_finite(value[0]) && is_finite(value[1]) && is_finite(value[2]);
}

Vec3 subtract(const Vec3& lhs, const Vec3& rhs) noexcept {
    return {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2]};
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) noexcept {
    return {lhs[1] * rhs[2] - lhs[2] * rhs[1],
            lhs[2] * rhs[0] - lhs[0] * rhs[2],
            lhs[0] * rhs[1] - lhs[1] * rhs[0]};
}

float length_sq(const Vec3& value) noexcept {
    return value[0] * value[0] + value[1] * value[1] + value[2] * value[2];
}

bool contains_point(const Vec3& min, const Vec3& max, const Vec3& point) noexcept {
    if (!is_finite_vec3(min) || !is_finite_vec3(max) || !is_finite_vec3(point)) {
        return false;
    }

    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (point[axis] < min[axis] - kBoundsTolerance ||
            point[axis] > max[axis] + kBoundsTolerance) {
            return false;
        }
    }
    return true;
}

bool contains_bounds(const Vec3& min, const Vec3& max, const Vec3& child_min,
                     const Vec3& child_max) noexcept {
    if (!is_finite_vec3(min) || !is_finite_vec3(max) || !is_finite_vec3(child_min) ||
        !is_finite_vec3(child_max)) {
        return false;
    }

    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (child_min[axis] < min[axis] - kBoundsTolerance ||
            child_max[axis] > max[axis] + kBoundsTolerance) {
            return false;
        }
    }
    return true;
}

bool has_large_extent(const Vec3& min, const Vec3& max) noexcept {
    if (!is_finite_vec3(min) || !is_finite_vec3(max)) {
        return false;
    }

    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (std::fabs(max[axis] - min[axis]) > kCollisionValidationLargeBoundsThreshold) {
            return true;
        }
    }
    return false;
}

void add_finding(CollisionValidationReport& report,
                 CollisionValidationSeverity severity,
                 std::string code,
                 std::string message,
                 std::size_t mesh_index,
                 std::size_t triangle_index) {
    report.findings.push_back(CollisionValidationFinding{.severity = severity,
                                                         .code = std::move(code),
                                                         .message = std::move(message),
                                                         .mesh_index = mesh_index,
                                                         .triangle_index = triangle_index});
    report.has_errors = report.has_errors || severity == CollisionValidationSeverity::kError;
}

bool finding_less(const CollisionValidationFinding& lhs,
                  const CollisionValidationFinding& rhs) noexcept {
    if (lhs.mesh_index != rhs.mesh_index) {
        return lhs.mesh_index < rhs.mesh_index;
    }
    if (lhs.triangle_index != rhs.triangle_index) {
        return lhs.triangle_index < rhs.triangle_index;
    }
    return lhs.code < rhs.code;
}

} // namespace

CollisionValidationReport validate_level_collision(
    const stellar::assets::LevelCollisionAsset& collision) {
    CollisionValidationReport report;

    if (collision.meshes.empty()) {
        add_finding(report,
                    CollisionValidationSeverity::kWarning,
                    "empty_level_collision",
                    "Level collision asset contains no collision meshes",
                    kCollisionValidationInvalidIndex,
                    kCollisionValidationInvalidIndex);
    }

    if (has_large_extent(collision.bounds_min, collision.bounds_max)) {
        add_finding(report,
                    CollisionValidationSeverity::kWarning,
                    "aggregate_bounds_extremely_large",
                    "Aggregate collision bounds exceed the documented 100000 world-unit extent "
                    "threshold",
                    kCollisionValidationInvalidIndex,
                    kCollisionValidationInvalidIndex);
    }

    std::unordered_map<std::string, std::size_t> first_mesh_name_indices;
    bool has_walkable_upward_surface = false;

    for (std::size_t mesh_index = 0; mesh_index < collision.meshes.size(); ++mesh_index) {
        const auto& mesh = collision.meshes[mesh_index];

        if (mesh.name.empty()) {
            add_finding(report,
                        CollisionValidationSeverity::kWarning,
                        "empty_mesh_name",
                        "Collision mesh name is empty and cannot be targeted by runtime name",
                        mesh_index,
                        kCollisionValidationInvalidIndex);
        } else {
            const auto [it, inserted] = first_mesh_name_indices.emplace(mesh.name, mesh_index);
            if (!inserted) {
                add_finding(report,
                            CollisionValidationSeverity::kWarning,
                            "duplicate_mesh_name",
                            "Collision mesh name duplicates an earlier mesh: " + mesh.name,
                            mesh_index,
                            kCollisionValidationInvalidIndex);
                (void)it;
            }
        }

        if (mesh.triangles.empty()) {
            add_finding(report,
                        CollisionValidationSeverity::kWarning,
                        "empty_mesh",
                        "Collision mesh contains no triangles",
                        mesh_index,
                        kCollisionValidationInvalidIndex);
        }

        if (has_large_extent(mesh.bounds_min, mesh.bounds_max)) {
            add_finding(report,
                        CollisionValidationSeverity::kWarning,
                        "mesh_bounds_extremely_large",
                        "Collision mesh bounds exceed the documented 100000 world-unit extent threshold",
                        mesh_index,
                        kCollisionValidationInvalidIndex);
        }

        if (!contains_bounds(collision.bounds_min,
                             collision.bounds_max,
                             mesh.bounds_min,
                             mesh.bounds_max)) {
            add_finding(report,
                        CollisionValidationSeverity::kError,
                        "aggregate_bounds_do_not_contain_mesh_bounds",
                        "Aggregate collision bounds do not contain mesh bounds",
                        mesh_index,
                        kCollisionValidationInvalidIndex);
        }

        for (std::size_t triangle_index = 0; triangle_index < mesh.triangles.size();
             ++triangle_index) {
            const auto& triangle = mesh.triangles[triangle_index];
            const bool finite_vertices = is_finite_vec3(triangle.a) && is_finite_vec3(triangle.b) &&
                                         is_finite_vec3(triangle.c);
            const bool finite_normal = is_finite_vec3(triangle.normal);

            if (!finite_vertices) {
                add_finding(report,
                            CollisionValidationSeverity::kError,
                            "non_finite_vertex",
                            "Collision triangle contains a non-finite vertex value",
                            mesh_index,
                            triangle_index);
            }

            if (!finite_normal || length_sq(triangle.normal) <= kNearZeroNormalLengthSq) {
                add_finding(report,
                            CollisionValidationSeverity::kError,
                            finite_normal ? "near_zero_normal" : "non_finite_normal",
                            finite_normal ? "Collision triangle normal is near zero length"
                                          : "Collision triangle normal contains a non-finite value",
                            mesh_index,
                            triangle_index);
            }

            if (finite_vertices) {
                const Vec3 ab = subtract(triangle.b, triangle.a);
                const Vec3 ac = subtract(triangle.c, triangle.a);
                if (length_sq(cross(ab, ac)) <= kNearZeroTriangleAreaSq) {
                    add_finding(report,
                                CollisionValidationSeverity::kError,
                                "near_zero_triangle_area",
                                "Collision triangle has zero or near-zero area",
                                mesh_index,
                                triangle_index);
                }
            }

            if (!contains_point(mesh.bounds_min, mesh.bounds_max, triangle.a) ||
                !contains_point(mesh.bounds_min, mesh.bounds_max, triangle.b) ||
                !contains_point(mesh.bounds_min, mesh.bounds_max, triangle.c)) {
                add_finding(report,
                            CollisionValidationSeverity::kError,
                            "mesh_bounds_do_not_contain_vertices",
                            "Collision mesh bounds do not contain all triangle vertices",
                            mesh_index,
                            triangle_index);
            }

            if (finite_normal && length_sq(triangle.normal) > kNearZeroNormalLengthSq &&
                triangle.normal[stellar::core::kWorldUpIndex] >= kWalkableUpNormalDot) {
                has_walkable_upward_surface = true;
            }
        }
    }

    if (!collision.meshes.empty() && !has_walkable_upward_surface) {
        add_finding(report,
                    CollisionValidationSeverity::kWarning,
                    "no_walkable_upward_surfaces",
                    "Collision asset has no upward-facing surfaces likely to be walkable",
                    kCollisionValidationInvalidIndex,
                    kCollisionValidationInvalidIndex);
    }

    std::sort(report.findings.begin(), report.findings.end(), finding_less);
    return report;
}

} // namespace stellar::world
