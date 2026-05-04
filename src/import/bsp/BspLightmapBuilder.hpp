#pragma once

#include "BspBinary.hpp"
#include "BspGeometryBuilder.hpp"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace stellar::import::bsp::detail {

struct LightmapBuildResult {
  std::optional<std::size_t> lightmap_index;
  std::optional<std::array<float, 2>> uv_min;
  std::optional<std::array<float, 2>> uv_extent;
};

struct LightmapRgbStats {
  std::array<std::uint8_t, 3> min_rgb{255U, 255U, 255U};
  std::array<std::uint8_t, 3> max_rgb{0U, 0U, 0U};
  std::array<double, 3> average_rgb{0.0, 0.0, 0.0};
  std::uint64_t texel_count = 0;
  bool all_black = true;
};

[[nodiscard]] std::size_t face_lightmap_reference_count(const BspMap &map) noexcept;
[[nodiscard]] stellar::assets::LevelLightingMode infer_lighting_mode(
    const BspMap &map, const stellar::assets::LevelGeometryAsset &geometry) noexcept;
[[nodiscard]] std::size_t material_lightmap_count(
    const stellar::assets::LevelGeometryAsset &geometry) noexcept;
[[nodiscard]] LightmapRgbStats rgb_stats_for_pixels(
    std::span<const std::uint8_t> pixels) noexcept;
void add_lightmap_diagnostics(const stellar::assets::LevelGeometryAsset &geometry,
                              const std::string &source_uri, ImportReport *report);
void apply_lighting_policy(stellar::assets::LevelAsset &level, const BspMap &map,
                           const std::vector<Entity> &entities, ImportReport *report);
[[nodiscard]] LightmapBuildResult build_lightmap_for_face(
    stellar::assets::LevelGeometryAsset &geometry, const BspMap &map, const Face &face,
    const std::vector<Vec3> &polygon, std::size_t face_index,
    const std::string &source_uri, ImportReport *report);

} // namespace stellar::import::bsp::detail
