#include "stellar/physics/CollisionWorld.hpp"

#include "stellar/core/WorldAxes.hpp"
#include "stellar/math/Geometry3.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace stellar::physics {
namespace {

constexpr float kEpsilon = 1.0e-5F;
constexpr float kContactOffset = 1.0e-4F;
constexpr std::uint32_t kInvalidNode = std::numeric_limits<std::uint32_t>::max();
constexpr std::size_t kBvhLeafSize = 4;

using stellar::math::add;
using stellar::math::dot;
using stellar::math::length_squared;
using stellar::math::mul;
using stellar::math::normalize_or;
using stellar::math::sub;
using stellar::math::Vec3;

constexpr Vec3 kDefaultUp = stellar::core::kWorldUp;

Vec3 min_vec(Vec3 a, Vec3 b) noexcept {
    return {std::min(a[0], b[0]), std::min(a[1], b[1]), std::min(a[2], b[2])};
}

Vec3 max_vec(Vec3 a, Vec3 b) noexcept {
    return {std::max(a[0], b[0]), std::max(a[1], b[1]), std::max(a[2], b[2])};
}

Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {(a[1] * b[2]) - (a[2] * b[1]),
            (a[2] * b[0]) - (a[0] * b[2]),
            (a[0] * b[1]) - (a[1] * b[0])};
}

Vec3 triangle_normal(const stellar::assets::CollisionTriangle& triangle) noexcept {
    if (length_squared(triangle.normal) > kEpsilon * kEpsilon) {
        return normalize_or(triangle.normal, kDefaultUp);
    }

    return normalize_or(cross(sub(triangle.b, triangle.a), sub(triangle.c, triangle.a)),
                        kDefaultUp);
}

bool point_in_triangle(Vec3 point, const stellar::assets::CollisionTriangle& triangle) noexcept {
    const Vec3 normal = triangle_normal(triangle);
    const Vec3 edge_ab = sub(triangle.b, triangle.a);
    const Vec3 edge_bc = sub(triangle.c, triangle.b);
    const Vec3 edge_ca = sub(triangle.a, triangle.c);

    const Vec3 ap = sub(point, triangle.a);
    const Vec3 bp = sub(point, triangle.b);
    const Vec3 cp = sub(point, triangle.c);

    const float side_ab = dot(normal, cross(edge_ab, ap));
    const float side_bc = dot(normal, cross(edge_bc, bp));
    const float side_ca = dot(normal, cross(edge_ca, cp));

    return side_ab >= -kEpsilon && side_bc >= -kEpsilon && side_ca >= -kEpsilon;
}

bool segment_triangle(Vec3 origin,
                      Vec3 delta,
                      const stellar::assets::CollisionTriangle& triangle,
                      float& out_t) noexcept {
    const Vec3 edge1 = sub(triangle.b, triangle.a);
    const Vec3 edge2 = sub(triangle.c, triangle.a);
    const Vec3 pvec = cross(delta, edge2);
    const float det = dot(edge1, pvec);

    if (std::abs(det) <= kEpsilon) {
        return false;
    }

    const float inv_det = 1.0F / det;
    const Vec3 tvec = sub(origin, triangle.a);
    const float u = dot(tvec, pvec) * inv_det;
    if (u < -kEpsilon || u > 1.0F + kEpsilon) {
        return false;
    }

    const Vec3 qvec = cross(tvec, edge1);
    const float v = dot(delta, qvec) * inv_det;
    if (v < -kEpsilon || u + v > 1.0F + kEpsilon) {
        return false;
    }

    const float t = dot(edge2, qvec) * inv_det;
    if (t < -kEpsilon || t > 1.0F + kEpsilon) {
        return false;
    }

    out_t = std::clamp(t, 0.0F, 1.0F);
    return true;
}

struct SweepHit {
    bool hit = false;
    float t = 1.0F;
    Vec3 normal{};
    std::size_t mesh_index = 0;
    std::size_t triangle_index = 0;
};

} // namespace

namespace {

using Aabb = detail::CollisionAabb;
using BvhNode = detail::CollisionBvhNode;
using TriangleRef = detail::CollisionTriangleRef;

Aabb make_empty_aabb() noexcept {
    return {.min = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max()},
            .max = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                    std::numeric_limits<float>::lowest()}};
}

Aabb merge(Aabb a, Aabb b) noexcept {
    return {.min = min_vec(a.min, b.min), .max = max_vec(a.max, b.max)};
}

Aabb expand(Aabb bounds, float amount) noexcept {
    const float safe_amount = std::max(amount, 0.0F);
    const Vec3 margin{safe_amount, safe_amount, safe_amount};
    return {.min = sub(bounds.min, margin), .max = add(bounds.max, margin)};
}

Aabb triangle_bounds(const stellar::assets::CollisionTriangle& triangle) noexcept {
    const Vec3 min_ab = min_vec(triangle.a, triangle.b);
    const Vec3 max_ab = max_vec(triangle.a, triangle.b);
    return {.min = min_vec(min_ab, triangle.c), .max = max_vec(max_ab, triangle.c)};
}

Vec3 aabb_centroid(Aabb bounds) noexcept {
    return mul(add(bounds.min, bounds.max), 0.5F);
}

bool segment_aabb(Vec3 origin, Vec3 delta, Aabb bounds) noexcept {
    float t_min = 0.0F;
    float t_max = 1.0F;

    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(delta[axis]) <= kEpsilon) {
            if (origin[axis] < bounds.min[axis] || origin[axis] > bounds.max[axis]) {
                return false;
            }
            continue;
        }

        const float inv_delta = 1.0F / delta[axis];
        float near_t = (bounds.min[axis] - origin[axis]) * inv_delta;
        float far_t = (bounds.max[axis] - origin[axis]) * inv_delta;
        if (near_t > far_t) {
            std::swap(near_t, far_t);
        }
        t_min = std::max(t_min, near_t);
        t_max = std::min(t_max, far_t);
        if (t_min > t_max + kEpsilon) {
            return false;
        }
    }

    return true;
}

bool intersects(Aabb a, Aabb b) noexcept {
    return a.max[0] >= b.min[0] && a.min[0] <= b.max[0] && a.max[1] >= b.min[1] &&
           a.min[1] <= b.max[1] && a.max[2] >= b.min[2] && a.min[2] <= b.max[2];
}

bool finite_bounds(Aabb bounds) noexcept {
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(bounds.min[axis]) || !std::isfinite(bounds.max[axis]) ||
            bounds.min[axis] > bounds.max[axis]) {
            return false;
        }
    }
    return true;
}

bool triangle_order_less(const TriangleRef& a, const TriangleRef& b) noexcept {
    if (a.mesh_index != b.mesh_index) {
        return a.mesh_index < b.mesh_index;
    }
    return a.triangle_index < b.triangle_index;
}

bool hit_order_less(std::size_t mesh_a,
                    std::size_t triangle_a,
                    std::size_t mesh_b,
                    std::size_t triangle_b) noexcept {
    return mesh_a < mesh_b || (mesh_a == mesh_b && triangle_a < triangle_b);
}

SweepHit sweep_sphere_against_triangle(Vec3 position,
                                        Vec3 displacement,
                                        float radius,
                                        const stellar::assets::CollisionTriangle& triangle) noexcept {
    const Vec3 normal = triangle_normal(triangle);
    const float signed_start = dot(sub(position, triangle.a), normal);
    const float denom = dot(displacement, normal);
    Vec3 contact_normal = normal;
    float target_distance = radius;

    if (std::abs(denom) <= kEpsilon) {
        return {};
    }

    if (signed_start > radius && denom < -kEpsilon) {
        target_distance = radius;
        contact_normal = normal;
    } else if (signed_start < -radius && denom > kEpsilon) {
        target_distance = -radius;
        contact_normal = mul(normal, -1.0F);
    } else {
        return {};
    }

    const float t = (target_distance - signed_start) / denom;
    if (t < -kEpsilon || t > 1.0F + kEpsilon) {
        return {};
    }

    const Vec3 center_at_hit = add(position, mul(displacement, std::clamp(t, 0.0F, 1.0F)));
    const Vec3 contact_point = sub(center_at_hit, mul(contact_normal, radius));
    if (!point_in_triangle(contact_point, triangle)) {
        return {};
    }

    return {.hit = true, .t = std::clamp(t, 0.0F, 1.0F), .normal = contact_normal};
}

} // namespace

CollisionWorld::CollisionWorld(const stellar::assets::LevelCollisionAsset& asset) noexcept
    : asset_(&asset) {
    build_broadphase();
}

bool collision_mesh_passes_filter(const CollisionQueryFilter& filter,
                                   std::size_t mesh_index) noexcept {
    if (filter.enabled_meshes == nullptr || filter.enabled_meshes->empty()) {
        return true;
    }
    if (mesh_index >= filter.enabled_meshes->size()) {
        return false;
    }
    return (*filter.enabled_meshes)[mesh_index];
}

Vec3 mesh_translation_for(const CollisionQueryFilter& filter, std::size_t mesh_index) noexcept {
    if (filter.mesh_translations == nullptr || mesh_index >= filter.mesh_translations->size()) {
        return {0.0F, 0.0F, 0.0F};
    }
    return (*filter.mesh_translations)[mesh_index];
}

Aabb translated_bounds(Aabb bounds, Vec3 translation) noexcept {
    return {.min = add(bounds.min, translation), .max = add(bounds.max, translation)};
}

stellar::assets::CollisionTriangle translated_triangle(
    const stellar::assets::CollisionTriangle& triangle,
    Vec3 translation) noexcept {
    return {.a = add(triangle.a, translation),
            .b = add(triangle.b, translation),
            .c = add(triangle.c, translation),
            .normal = triangle.normal};
}

void CollisionWorld::build_broadphase() noexcept {
    triangle_refs_.clear();
    bvh_nodes_.clear();
    stats_ = {};

    if (asset_ == nullptr) {
        return;
    }

    stats_.mesh_count = asset_->meshes.size();
    for (std::size_t mesh_index = 0; mesh_index < asset_->meshes.size(); ++mesh_index) {
        const auto& mesh = asset_->meshes[mesh_index];
        stats_.triangle_count += mesh.triangles.size();
        for (std::size_t triangle_index = 0; triangle_index < mesh.triangles.size();
             ++triangle_index) {
            const Aabb bounds = triangle_bounds(mesh.triangles[triangle_index]);
            triangle_refs_.push_back({.mesh_index = mesh_index,
                                      .triangle_index = triangle_index,
                                      .bounds = bounds,
                                      .centroid = aabb_centroid(bounds)});
        }
    }

    if (triangle_refs_.empty()) {
        return;
    }

    auto build_node = [this](auto&& self,
                             std::uint32_t first,
                             std::uint32_t count) -> std::uint32_t {
        const std::uint32_t node_index = static_cast<std::uint32_t>(bvh_nodes_.size());
        BvhNode node{};
        node.first = first;
        node.count = count;
        node.left = kInvalidNode;
        node.right = kInvalidNode;
        Aabb bounds = make_empty_aabb();
        Aabb centroid_bounds = make_empty_aabb();
        for (std::uint32_t index = first; index < first + count; ++index) {
            bounds = merge(bounds, triangle_refs_[index].bounds);
            const Aabb point_bounds{.min = triangle_refs_[index].centroid,
                                    .max = triangle_refs_[index].centroid};
            centroid_bounds = merge(centroid_bounds, point_bounds);
        }
        node.bounds = bounds;
        bvh_nodes_.push_back(node);

        if (count <= kBvhLeafSize) {
            return node_index;
        }

        const Vec3 extent = sub(centroid_bounds.max, centroid_bounds.min);
        int split_axis = 0;
        if (extent[1] > extent[split_axis]) {
            split_axis = 1;
        }
        if (extent[2] > extent[split_axis]) {
            split_axis = 2;
        }

        auto begin = triangle_refs_.begin() + static_cast<std::ptrdiff_t>(first);
        auto end = begin + static_cast<std::ptrdiff_t>(count);
        std::stable_sort(begin, end, [split_axis](const TriangleRef& a, const TriangleRef& b) {
            if (std::abs(a.centroid[split_axis] - b.centroid[split_axis]) > kEpsilon) {
                return a.centroid[split_axis] < b.centroid[split_axis];
            }
            return triangle_order_less(a, b);
        });

        const std::uint32_t left_count = count / 2;
        const std::uint32_t right_count = count - left_count;
        bvh_nodes_[node_index].left = self(self, first, left_count);
        bvh_nodes_[node_index].right = self(self, first + left_count, right_count);
        bvh_nodes_[node_index].count = 0;
        return node_index;
    };

    build_node(build_node, 0, static_cast<std::uint32_t>(triangle_refs_.size()));
    stats_.broadphase_node_count = bvh_nodes_.size();
}

RaycastHit CollisionWorld::raycast(Vec3 origin, Vec3 delta) const noexcept {
    return raycast(origin, delta, {});
}

RaycastHit CollisionWorld::raycast(Vec3 origin,
                                   Vec3 delta,
                                   CollisionQueryFilter filter) const noexcept {
    RaycastHit result{};
    stats_.last_query_triangle_tests = 0;
    stats_.last_query_candidate_count = 0;
    if (asset_ == nullptr || length_squared(delta) <= kEpsilon * kEpsilon || bvh_nodes_.empty()) {
        return result;
    }

    float nearest_t = std::numeric_limits<float>::max();
    std::vector<std::uint32_t> stack;
    stack.reserve(bvh_nodes_.size());
    stack.push_back(0);

    while (!stack.empty()) {
        const std::uint32_t node_index = stack.back();
        stack.pop_back();
        const BvhNode& node = bvh_nodes_[node_index];
        if (filter.mesh_translations == nullptr && !segment_aabb(origin, delta, node.bounds)) {
            continue;
        }

        if (node.count == 0) {
            if (node.right != kInvalidNode) {
                stack.push_back(node.right);
            }
            if (node.left != kInvalidNode) {
                stack.push_back(node.left);
            }
            continue;
        }

        for (std::uint32_t index = node.first; index < node.first + node.count; ++index) {
            const TriangleRef& ref = triangle_refs_[index];
            const Vec3 translation = mesh_translation_for(filter, ref.mesh_index);
            const Aabb ref_bounds = translated_bounds(ref.bounds, translation);
            if (!segment_aabb(origin, delta, ref_bounds)) {
                continue;
            }
            if (!collision_mesh_passes_filter(filter, ref.mesh_index)) {
                continue;
            }

            const auto triangle = translated_triangle(
                asset_->meshes[ref.mesh_index].triangles[ref.triangle_index], translation);
            ++stats_.last_query_candidate_count;
            ++stats_.last_query_triangle_tests;
            float t = 0.0F;
            if (!segment_triangle(origin, delta, triangle, t)) {
                continue;
            }

            const bool nearer = t < nearest_t - kEpsilon;
            const bool tie_break = std::abs(t - nearest_t) <= kEpsilon &&
                                   (!result.hit || hit_order_less(ref.mesh_index, ref.triangle_index,
                                                                  result.mesh_index,
                                                                  result.triangle_index));
            if (nearer || tie_break) {
                nearest_t = t;
                result.hit = true;
                result.t = t;
                result.position = add(origin, mul(delta, t));
                result.normal = triangle_normal(triangle);
                result.mesh_index = ref.mesh_index;
                result.triangle_index = ref.triangle_index;
            }
        }
    }

    return result;
}

std::vector<CollisionTriangleCandidate>
CollisionWorld::query_triangles(CollisionQueryAabb bounds) const {
    return query_triangles(bounds, {});
}

std::vector<CollisionTriangleCandidate>
CollisionWorld::query_triangles(CollisionQueryAabb bounds, CollisionQueryFilter filter) const {
    std::vector<CollisionTriangleCandidate> candidates;
    stats_.last_query_triangle_tests = 0;
    stats_.last_query_candidate_count = 0;

    const Aabb query{.min = bounds.min, .max = bounds.max};
    if (asset_ == nullptr || bvh_nodes_.empty() || !finite_bounds(query)) {
        return candidates;
    }

    std::vector<std::uint32_t> stack;
    stack.reserve(bvh_nodes_.size());
    stack.push_back(0);

    while (!stack.empty()) {
        const std::uint32_t node_index = stack.back();
        stack.pop_back();
        const BvhNode& node = bvh_nodes_[node_index];
        if (filter.mesh_translations == nullptr && !intersects(node.bounds, query)) {
            continue;
        }

        if (node.count == 0) {
            if (node.right != kInvalidNode) {
                stack.push_back(node.right);
            }
            if (node.left != kInvalidNode) {
                stack.push_back(node.left);
            }
            continue;
        }

        for (std::uint32_t index = node.first; index < node.first + node.count; ++index) {
            const TriangleRef& ref = triangle_refs_[index];
            const Aabb ref_bounds = translated_bounds(ref.bounds, mesh_translation_for(filter, ref.mesh_index));
            if (!intersects(ref_bounds, query)) {
                continue;
            }
            if (!collision_mesh_passes_filter(filter, ref.mesh_index)) {
                continue;
            }

            candidates.push_back({.mesh_index = ref.mesh_index,
                                  .triangle_index = ref.triangle_index});
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const CollisionTriangleCandidate& a,
                                                       const CollisionTriangleCandidate& b) {
        return hit_order_less(a.mesh_index, a.triangle_index, b.mesh_index, b.triangle_index);
    });
    stats_.last_query_candidate_count = candidates.size();
    stats_.last_query_triangle_tests = candidates.size();
    return candidates;
}

GroundProbeHit CollisionWorld::probe_ground(Vec3 origin,
                                             float max_distance,
                                             float min_floor_normal_up) const noexcept {
    return probe_ground(origin, max_distance, min_floor_normal_up, {});
}

GroundProbeHit CollisionWorld::probe_ground(Vec3 origin,
                                             float max_distance,
                                             float min_floor_normal_up,
                                             CollisionQueryFilter filter) const noexcept {
    GroundProbeHit result{};
    if (max_distance <= 0.0F) {
        return result;
    }

    const RaycastHit hit = raycast(origin, mul(stellar::core::kWorldDown, max_distance), filter);
    if (!hit.hit || dot(hit.normal, kDefaultUp) < min_floor_normal_up) {
        return result;
    }

    result.hit = true;
    result.distance = hit.t * max_distance;
    result.raycast = hit;
    return result;
}

MoveResult CollisionWorld::move_sphere(Vec3 position,
                                       Vec3 displacement,
                                       float radius,
                                       int max_iterations) const noexcept {
    return move_sphere(position, displacement, radius, max_iterations, {});
}

MoveResult CollisionWorld::move_sphere(Vec3 position,
                                       Vec3 displacement,
                                       float radius,
                                       int max_iterations,
                                       CollisionQueryFilter filter) const noexcept {
    MoveResult result{};
    result.position = position;
    result.velocity = displacement;

    stats_.last_query_triangle_tests = 0;
    stats_.last_query_candidate_count = 0;
    if (asset_ == nullptr || asset_->meshes.empty() ||
        length_squared(displacement) <= kEpsilon * kEpsilon || bvh_nodes_.empty()) {
        result.position = add(position, displacement);
        result.velocity = {0.0F, 0.0F, 0.0F};
        result.grounded = probe_ground(result.position, std::max(radius + 0.05F, 0.05F), 0.5F,
                                       filter).hit;
        return result;
    }

    const float safe_radius = std::max(radius, 0.0F);
    Vec3 current_position = position;
    Vec3 remaining = displacement;
    const int iterations = std::clamp(max_iterations, 0, 8);

    for (int iteration = 0; iteration < iterations; ++iteration) {
        if (length_squared(remaining) <= kEpsilon * kEpsilon) {
            break;
        }

        SweepHit nearest{};
        const Aabb sweep_bounds = expand({.min = min_vec(current_position, add(current_position, remaining)),
                                          .max = max_vec(current_position, add(current_position, remaining))},
                                         safe_radius);
        std::vector<std::uint32_t> stack;
        stack.reserve(bvh_nodes_.size());
        stack.push_back(0);

        while (!stack.empty()) {
            const std::uint32_t node_index = stack.back();
            stack.pop_back();
            const BvhNode& node = bvh_nodes_[node_index];
            if (filter.mesh_translations == nullptr &&
                !segment_aabb(current_position, remaining, expand(node.bounds, safe_radius)) &&
                !intersects(node.bounds, sweep_bounds)) {
                continue;
            }

            if (node.count == 0) {
                if (node.right != kInvalidNode) {
                    stack.push_back(node.right);
                }
                if (node.left != kInvalidNode) {
                    stack.push_back(node.left);
                }
                continue;
            }

            for (std::uint32_t index = node.first; index < node.first + node.count; ++index) {
                const TriangleRef& ref = triangle_refs_[index];
                const Vec3 translation = mesh_translation_for(filter, ref.mesh_index);
                const Aabb ref_bounds = translated_bounds(ref.bounds, translation);
                if (!segment_aabb(current_position, remaining, expand(ref_bounds, safe_radius))) {
                    continue;
                }
                if (!collision_mesh_passes_filter(filter, ref.mesh_index)) {
                    continue;
                }

                const auto triangle = translated_triangle(
                    asset_->meshes[ref.mesh_index].triangles[ref.triangle_index], translation);
                ++stats_.last_query_candidate_count;
                ++stats_.last_query_triangle_tests;
                const SweepHit candidate =
                    sweep_sphere_against_triangle(current_position, remaining, safe_radius, triangle);
                const bool nearer = candidate.hit && candidate.t < nearest.t - kEpsilon;
                const bool tie_break = candidate.hit && std::abs(candidate.t - nearest.t) <= kEpsilon &&
                                       (!nearest.hit || hit_order_less(ref.mesh_index,
                                                                      ref.triangle_index,
                                                                      nearest.mesh_index,
                                                                      nearest.triangle_index));
                if (nearer || tie_break) {
                    nearest = candidate;
                    nearest.mesh_index = ref.mesh_index;
                    nearest.triangle_index = ref.triangle_index;
                }
            }
        }

        if (!nearest.hit) {
            current_position = add(current_position, remaining);
            remaining = {0.0F, 0.0F, 0.0F};
            break;
        }

        result.hit = true;
        const float travel_t = std::max(0.0F, nearest.t - kContactOffset);
        current_position = add(current_position, mul(remaining, travel_t));

        const Vec3 unused = mul(remaining, 1.0F - travel_t);
        const float into_plane = dot(unused, nearest.normal);
        remaining = sub(unused, mul(nearest.normal, into_plane));
        result.iterations = iteration + 1;
    }

    result.position = current_position;
    result.velocity = remaining;
    const std::size_t movement_triangle_tests = stats_.last_query_triangle_tests;
    result.grounded = probe_ground(result.position, std::max(safe_radius + 0.05F, 0.05F), 0.5F,
                                   filter).hit;
    stats_.last_query_triangle_tests += movement_triangle_tests;
    return result;
}

const stellar::assets::LevelCollisionAsset& CollisionWorld::asset() const noexcept {
    return *asset_;
}

stellar::assets::CollisionTriangle
CollisionWorld::triangle(CollisionTriangleCandidate candidate,
                         CollisionQueryFilter filter) const noexcept {
    if (asset_ == nullptr || candidate.mesh_index >= asset_->meshes.size() ||
        candidate.triangle_index >= asset_->meshes[candidate.mesh_index].triangles.size()) {
        return {};
    }
    return translated_triangle(asset_->meshes[candidate.mesh_index].triangles[candidate.triangle_index],
                               mesh_translation_for(filter, candidate.mesh_index));
}

CollisionWorldStats CollisionWorld::stats() const noexcept {
    return stats_;
}

} // namespace stellar::physics
