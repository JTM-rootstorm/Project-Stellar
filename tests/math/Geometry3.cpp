#include "stellar/math/Geometry3.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace {

bool near_equal(float lhs, float rhs, float epsilon = 1.0e-5F) {
    return std::abs(lhs - rhs) <= epsilon;
}

void finite_checks_are_deterministic() {
    const float inf = std::numeric_limits<float>::infinity();
    assert(stellar::math::is_finite(1.0F));
    assert(!stellar::math::is_finite(inf));
    assert(stellar::math::is_finite(stellar::math::Vec3{1.0F, 2.0F, 3.0F}));
    assert(!stellar::math::is_finite(stellar::math::Vec3{1.0F, inf, 3.0F}));
}

void vector_arithmetic_is_componentwise() {
    using stellar::math::Vec3;
    assert((stellar::math::add(Vec3{1.0F, 2.0F, 3.0F}, Vec3{4.0F, 5.0F, 6.0F}) ==
            Vec3{5.0F, 7.0F, 9.0F}));
    assert((stellar::math::sub(Vec3{4.0F, 5.0F, 6.0F}, Vec3{1.0F, 2.0F, 3.0F}) ==
            Vec3{3.0F, 3.0F, 3.0F}));
    assert((stellar::math::mul(Vec3{1.0F, -2.0F, 3.0F}, 2.0F) ==
            Vec3{2.0F, -4.0F, 6.0F}));
    assert(near_equal(stellar::math::dot(Vec3{1.0F, 2.0F, 3.0F}, Vec3{4.0F, 5.0F, 6.0F}),
                      32.0F));
    assert(near_equal(stellar::math::length_squared(Vec3{2.0F, 3.0F, 6.0F}), 49.0F));
}

void normalize_or_handles_valid_zero_and_non_finite_inputs() {
    using stellar::math::Vec3;
    const Vec3 fallback{0.0F, 1.0F, 0.0F};
    const auto normalized = stellar::math::normalize_or(Vec3{3.0F, 0.0F, 4.0F}, fallback);
    assert(near_equal(normalized[0], 0.6F));
    assert(near_equal(normalized[1], 0.0F));
    assert(near_equal(normalized[2], 0.8F));

    assert((stellar::math::normalize_or(Vec3{0.0F, 0.0F, 0.0F}, fallback) == fallback));

    const float inf = std::numeric_limits<float>::infinity();
    assert((stellar::math::normalize_or(Vec3{inf, 0.0F, 0.0F}, fallback) == fallback));
}

void sanitizers_are_small_and_predictable() {
    const float inf = std::numeric_limits<float>::infinity();
    assert(stellar::math::sanitized_radius(2.0F) == 2.0F);
    assert(stellar::math::sanitized_radius(-1.0F) == 0.0F);
    assert(stellar::math::sanitized_radius(inf) == 0.0F);

    assert(stellar::math::sanitized_half_extent(-3.0F) == 3.0F);
    assert(stellar::math::sanitized_half_extent(inf) == 0.0F);

    assert(stellar::math::sanitized_capsule_height(4.0F, 1.0F) == 4.0F);
    assert(stellar::math::sanitized_capsule_height(1.0F, 1.0F) == 2.0F);
    assert(stellar::math::sanitized_capsule_height(inf, 1.0F) == 2.0F);
}

void point_distance_helpers_match_simple_cases() {
    using stellar::math::Aabb3;
    using stellar::math::Segment3;
    using stellar::math::Vec3;

    const Aabb3 unit_box{.min = {-1.0F, -1.0F, -1.0F}, .max = {1.0F, 1.0F, 1.0F}};
    assert(near_equal(stellar::math::point_aabb_distance_squared(Vec3{0.0F, 0.0F, 0.0F},
                                                                  unit_box),
                      0.0F));
    assert(near_equal(stellar::math::point_aabb_distance_squared(Vec3{3.0F, 1.0F, 1.0F},
                                                                  unit_box),
                      4.0F));

    const Segment3 segment{.start = {0.0F, 0.0F, 0.0F}, .end = {2.0F, 0.0F, 0.0F}};
    assert(near_equal(stellar::math::point_segment_distance_squared(Vec3{1.0F, 3.0F, 0.0F},
                                                                     segment),
                      9.0F));
    assert(near_equal(stellar::math::point_segment_distance_squared(Vec3{3.0F, 0.0F, 0.0F},
                                                                     segment),
                      1.0F));
}

} // namespace

int main() {
    finite_checks_are_deterministic();
    vector_arithmetic_is_componentwise();
    normalize_or_handles_valid_zero_and_non_finite_inputs();
    sanitizers_are_small_and_predictable();
    point_distance_helpers_match_simple_cases();
    return 0;
}
