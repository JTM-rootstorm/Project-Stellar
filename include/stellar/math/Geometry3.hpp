#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace stellar::math {

/** @brief Backend-neutral three-component float vector used by collision/world internals. */
using Vec3 = std::array<float, 3>;

/** @brief Returns whether a scalar is finite. */
[[nodiscard]] inline bool is_finite(float value) noexcept {
    return std::isfinite(value);
}

/** @brief Returns whether all vector components are finite. */
[[nodiscard]] inline bool is_finite(Vec3 value) noexcept {
    return is_finite(value[0]) && is_finite(value[1]) && is_finite(value[2]);
}

/** @brief Adds two vectors component-wise. */
[[nodiscard]] inline Vec3 add(Vec3 lhs, Vec3 rhs) noexcept {
    return {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
}

/** @brief Subtracts two vectors component-wise. */
[[nodiscard]] inline Vec3 sub(Vec3 lhs, Vec3 rhs) noexcept {
    return {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2]};
}

/** @brief Multiplies a vector by a scalar. */
[[nodiscard]] inline Vec3 mul(Vec3 value, float scalar) noexcept {
    return {value[0] * scalar, value[1] * scalar, value[2] * scalar};
}

/** @brief Returns the dot product of two vectors. */
[[nodiscard]] inline float dot(Vec3 lhs, Vec3 rhs) noexcept {
    return (lhs[0] * rhs[0]) + (lhs[1] * rhs[1]) + (lhs[2] * rhs[2]);
}

/** @brief Returns the squared vector length. */
[[nodiscard]] inline float length_squared(Vec3 value) noexcept {
    return dot(value, value);
}

/** @brief Normalizes a vector or returns the supplied fallback for zero-length input. */
[[nodiscard]] inline Vec3 normalize_or(Vec3 value, Vec3 fallback) noexcept {
    constexpr float kEpsilon = 1.0e-5F;
    const float len_sq = length_squared(value);
    if (len_sq <= kEpsilon * kEpsilon) {
        return fallback;
    }

    return mul(value, 1.0F / std::sqrt(len_sq));
}

/** @brief Clamps non-finite or negative radii to zero. */
[[nodiscard]] inline float sanitized_radius(float radius) noexcept {
    if (!std::isfinite(radius) || radius < 0.0F) {
        return 0.0F;
    }
    return radius;
}

/** @brief Converts a finite half extent to its absolute value, or zero for non-finite input. */
[[nodiscard]] inline float sanitized_half_extent(float extent) noexcept {
    if (!std::isfinite(extent)) {
        return 0.0F;
    }
    return std::abs(extent);
}

/** @brief Sanitizes a capsule height so it is finite, non-negative, and at least its diameter. */
[[nodiscard]] inline float sanitized_capsule_height(float height, float radius) noexcept {
    if (!std::isfinite(height) || height < 0.0F) {
        return 2.0F * radius;
    }
    return std::max(height, 2.0F * radius);
}

/** @brief Line segment in three-dimensional float space. */
struct Segment3 {
    /** @brief Segment start point. */
    Vec3 start{};

    /** @brief Segment end point. */
    Vec3 end{};
};

/** @brief Axis-aligned bounding box in three-dimensional float space. */
struct Aabb3 {
    /** @brief Minimum corner of the box. */
    Vec3 min{};

    /** @brief Maximum corner of the box. */
    Vec3 max{};
};

/** @brief Returns squared distance from a point to an axis-aligned bounding box. */
[[nodiscard]] inline float point_aabb_distance_squared(Vec3 point, Aabb3 bounds) noexcept {
    float distance_squared = 0.0F;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        float delta = 0.0F;
        if (point[axis] < bounds.min[axis]) {
            delta = bounds.min[axis] - point[axis];
        } else if (point[axis] > bounds.max[axis]) {
            delta = point[axis] - bounds.max[axis];
        }
        distance_squared += delta * delta;
    }
    return distance_squared;
}

/** @brief Returns squared distance from a point to a line segment. */
[[nodiscard]] inline float point_segment_distance_squared(Vec3 point, Segment3 segment) noexcept {
    const Vec3 ab = sub(segment.end, segment.start);
    const float ab_len_sq = length_squared(ab);
    if (ab_len_sq <= 0.0F) {
        return length_squared(sub(point, segment.start));
    }
    const float t = std::clamp(dot(sub(point, segment.start), ab) / ab_len_sq, 0.0F, 1.0F);
    return length_squared(sub(point, add(segment.start, mul(ab, t))));
}

} // namespace stellar::math
