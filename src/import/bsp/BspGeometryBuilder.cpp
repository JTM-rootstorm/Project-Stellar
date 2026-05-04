#include "BspGeometryBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace stellar::import::bsp::detail {

Vec3 subtract(Vec3 a, Vec3 b) noexcept {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

float dot(Vec3 a, Vec3 b) noexcept {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

Vec3 cross(Vec3 a, Vec3 b) noexcept {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}

namespace {

[[nodiscard]] float length(Vec3 v) noexcept {
  return std::sqrt(dot(v, v));
}

[[nodiscard]] Vec3 subtract_scaled(Vec3 v, Vec3 axis, float scale) noexcept {
  return {v[0] - axis[0] * scale, v[1] - axis[1] * scale,
          v[2] - axis[2] * scale};
}

} // namespace

Vec3 normalize(Vec3 v) noexcept {
  const float len = length(v);
  if (len <= 0.000001F) {
    return {0.0F, 0.0F, 1.0F};
  }
  return {v[0] / len, v[1] / len, v[2] / len};
}

std::optional<std::array<float, 4>>
tangent_for_texinfo(Vec3 normal, const Texinfo &info) noexcept {
  constexpr float kEpsilon = 0.000001F;
  const Vec3 s_axis{info.s[0], info.s[1], info.s[2]};
  const Vec3 t_axis{info.t[0], info.t[1], info.t[2]};
  if (length(normal) <= kEpsilon || length(s_axis) <= kEpsilon ||
      length(t_axis) <= kEpsilon) {
    return std::nullopt;
  }
  normal = normalize(normal);
  Vec3 tangent = subtract_scaled(s_axis, normal, dot(s_axis, normal));
  if (length(tangent) <= kEpsilon) {
    return std::nullopt;
  }
  tangent = normalize(tangent);
  Vec3 bitangent = subtract_scaled(t_axis, normal, dot(t_axis, normal));
  if (length(bitangent) <= kEpsilon) {
    return std::nullopt;
  }
  bitangent = normalize(bitangent);
  const float handedness = dot(cross(normal, tangent), bitangent) < 0.0F
                               ? -1.0F
                               : 1.0F;
  return std::array<float, 4>{tangent[0], tangent[1], tangent[2], handedness};
}

void include_point(Vec3 &mins, Vec3 &maxs, const Vec3 &point) noexcept {
  for (std::size_t i = 0; i < 3; ++i) {
    mins[i] = std::min(mins[i], point[i]);
    maxs[i] = std::max(maxs[i], point[i]);
  }
}

std::vector<Vec3> face_vertices(const BspMap &map, const Face &face) {
  std::vector<Vec3> vertices;
  if (face.first_edge < 0) {
    return vertices;
  }
  const auto first_edge = static_cast<std::size_t>(face.first_edge);
  for (std::uint16_t i = 0; i < face.edge_count; ++i) {
    const std::size_t surfedge_index = first_edge + i;
    if (surfedge_index >= map.surfedges.size()) {
      return {};
    }
    const std::int32_t surfedge = map.surfedges[surfedge_index];
    const std::size_t edge_index = static_cast<std::size_t>(std::abs(surfedge));
    if (edge_index >= map.edges.size()) {
      return {};
    }
    const Edge &edge = map.edges[edge_index];
    const std::uint16_t vertex_index = surfedge >= 0 ? edge.first : edge.second;
    if (vertex_index >= map.vertices.size()) {
      return {};
    }
    vertices.push_back(map.vertices[vertex_index].position);
  }
  return vertices;
}

} // namespace stellar::import::bsp::detail
