#include "BspGeometryBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace stellar::import::bsp::detail {

Vec3 subtract(Vec3 a, Vec3 b) noexcept {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

Vec3 cross(Vec3 a, Vec3 b) noexcept {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}

Vec3 normalize(Vec3 v) noexcept {
  const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (len <= 0.000001F) {
    return {0.0F, 0.0F, 1.0F};
  }
  return {v[0] / len, v[1] / len, v[2] / len};
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
