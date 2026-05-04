#pragma once

#include "BspBinary.hpp"

#include <array>
#include <optional>
#include <vector>

namespace stellar::import::bsp::detail {

using Vec3 = std::array<float, 3>;

[[nodiscard]] Vec3 subtract(Vec3 a, Vec3 b) noexcept;
[[nodiscard]] float dot(Vec3 a, Vec3 b) noexcept;
[[nodiscard]] Vec3 cross(Vec3 a, Vec3 b) noexcept;
[[nodiscard]] Vec3 normalize(Vec3 v) noexcept;
[[nodiscard]] std::optional<std::array<float, 4>>
tangent_for_texinfo(Vec3 normal, const Texinfo &info) noexcept;
void include_point(Vec3 &mins, Vec3 &maxs, const Vec3 &point) noexcept;
[[nodiscard]] std::vector<Vec3> face_vertices(const BspMap &map, const Face &face);

} // namespace stellar::import::bsp::detail
