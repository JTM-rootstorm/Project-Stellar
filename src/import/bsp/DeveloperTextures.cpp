#include "DeveloperTextures.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <utility>

namespace stellar::import::bsp::detail {
namespace {

constexpr std::uint32_t kDeveloperTextureSize = 128U;

struct DeveloperTextureSpec {
  std::string_view canonical_name;
  std::uint32_t grid_spacing = 16U;
  enum class Kind { kGrid, kPlayer, kWall } kind = Kind::kGrid;
};

[[nodiscard]] std::string normalized_name(std::string_view name) {
  std::string normalized;
  normalized.reserve(name.size());
  for (const char character : name) {
    const char slash = character == '\\' ? '/' : character;
    normalized.push_back(static_cast<char>(
        std::tolower(static_cast<unsigned char>(slash))));
  }
  return normalized;
}

[[nodiscard]] std::optional<DeveloperTextureSpec>
spec_for_name(std::string_view material_name) {
  const std::string name = normalized_name(material_name);
  const std::array specs{
      std::pair{std::string_view{"stellar_dev_grid_12"},
                DeveloperTextureSpec{"stellar_dev_grid_12", 12,
                                      DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"dev_grid_12"},
                DeveloperTextureSpec{"stellar_dev_grid_12", 12,
                                      DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"dev/grid_12"},
                DeveloperTextureSpec{"stellar_dev_grid_12", 12,
                                      DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"stellar_dev_grid_16"},
                DeveloperTextureSpec{"stellar_dev_grid_16", 16,
                                      DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"dev_grid_16"},
                DeveloperTextureSpec{"stellar_dev_grid_16", 16,
                                      DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"dev/grid_16"},
                DeveloperTextureSpec{"stellar_dev_grid_16", 16,
                                      DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"stellar_dev_grid_32"},
                DeveloperTextureSpec{"stellar_dev_grid_32", 32,
                                      DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"dev_grid_32"},
                DeveloperTextureSpec{"stellar_dev_grid_32", 32,
                                      DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"dev/grid_32"},
                DeveloperTextureSpec{"stellar_dev_grid_32", 32,
                                      DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"stellar_dev_grid_64"},
                DeveloperTextureSpec{"stellar_dev_grid_64", 64,
                                      DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"dev_grid_64"},
                DeveloperTextureSpec{"stellar_dev_grid_64", 64,
                                      DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"dev/grid_64"},
                DeveloperTextureSpec{"stellar_dev_grid_64", 64,
                                      DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"stellar_dev_player_72"},
                DeveloperTextureSpec{"stellar_dev_player_72", 16,
                                      DeveloperTextureSpec::Kind::kPlayer}},
      std::pair{std::string_view{"dev_player_72"},
                DeveloperTextureSpec{"stellar_dev_player_72", 16,
                                      DeveloperTextureSpec::Kind::kPlayer}},
      std::pair{std::string_view{"dev/player_72"},
                DeveloperTextureSpec{"stellar_dev_player_72", 16,
                                      DeveloperTextureSpec::Kind::kPlayer}},
      std::pair{std::string_view{"stellar_dev_wall_96"},
                DeveloperTextureSpec{"stellar_dev_wall_96", 16,
                                      DeveloperTextureSpec::Kind::kWall}},
      std::pair{std::string_view{"dev_wall_96"},
                DeveloperTextureSpec{"stellar_dev_wall_96", 16,
                                      DeveloperTextureSpec::Kind::kWall}},
      std::pair{std::string_view{"dev/wall_96"},
                DeveloperTextureSpec{"stellar_dev_wall_96", 16,
                                      DeveloperTextureSpec::Kind::kWall}},
  };

  const auto found = std::ranges::find_if(
      specs, [&](const auto &entry) { return entry.first == name; });
  if (found == specs.end()) {
    return std::nullopt;
  }
  return found->second;
}

void append_pixel(stellar::assets::ImageAsset &image,
                  std::array<std::uint8_t, 4> color) {
  image.pixels.insert(image.pixels.end(), color.begin(), color.end());
}

[[nodiscard]] std::array<std::uint8_t, 4> palette_color(
    std::uint8_t index) noexcept {
  constexpr std::array<std::array<std::uint8_t, 3>, 16> kPalette{{
      {0U, 0U, 0U},       {32U, 32U, 36U},    {72U, 72U, 80U},
      {112U, 112U, 124U}, {176U, 176U, 188U}, {224U, 224U, 232U},
      {32U, 64U, 128U},   {48U, 96U, 192U},   {96U, 144U, 224U},
      {160U, 192U, 240U}, {96U, 64U, 32U},    {144U, 96U, 48U},
      {192U, 144U, 72U},  {224U, 192U, 112U}, {128U, 48U, 48U},
      {208U, 80U, 72U},
  }};
  if (index < kPalette.size()) {
    const auto rgb = kPalette[index];
    return {rgb[0], rgb[1], rgb[2], 255U};
  }
  return {static_cast<std::uint8_t>((index * 37U) & 0xFFU),
          static_cast<std::uint8_t>((index * 67U) & 0xFFU),
          static_cast<std::uint8_t>((index * 97U) & 0xFFU), 255U};
}

[[nodiscard]] std::uint8_t grid_index(std::uint32_t x, std::uint32_t y,
                                      std::uint32_t spacing) noexcept {
  const std::uint32_t safe_spacing = std::max(spacing, 1U);
  const std::uint32_t major = safe_spacing * 4U;
  if (x % major == 0U || y % major == 0U) {
    return 9U;
  }
  if (x % safe_spacing == 0U || y % safe_spacing == 0U) {
    return 7U;
  }
  return (((x / safe_spacing) + (y / safe_spacing)) & 1U) != 0U ? 2U : 1U;
}

[[nodiscard]] std::uint8_t player_index(std::uint32_t x,
                                        std::uint32_t y) noexcept {
  constexpr std::uint32_t kCenter = kDeveloperTextureSize / 2U;
  constexpr std::uint32_t kHalfWidth = 16U;
  constexpr std::uint32_t kHalfHeight = 36U;
  const std::uint32_t dx = std::max(x, kCenter) - std::min(x, kCenter);
  const std::uint32_t dy = std::max(y, kCenter) - std::min(y, kCenter);
  const bool grid = x % 16U == 0U || y % 16U == 0U;
  const bool in_marker = dx <= kHalfWidth && dy <= kHalfHeight;
  const bool edge = in_marker && (dx == kHalfWidth || dy == kHalfHeight);
  if (edge) {
    return 15U;
  }
  if (in_marker) {
    return 14U;
  }
  return grid ? 6U : 1U;
}

[[nodiscard]] std::uint8_t wall_index(std::uint32_t x,
                                      std::uint32_t y) noexcept {
  constexpr std::uint32_t kBrickWidth = 32U;
  constexpr std::uint32_t kBrickHeight = 16U;
  const std::uint32_t row = y / kBrickHeight;
  const std::uint32_t offset = (row & 1U) != 0U ? 16U : 0U;
  const std::uint32_t local_x = (x + offset) % kBrickWidth;
  if (local_x == 0U || y % kBrickHeight == 0U) {
    return 10U;
  }
  return (((x / 8U) + (y / 8U)) & 1U) != 0U ? 12U : 11U;
}

[[nodiscard]] std::array<std::uint8_t, 4> texture_color(
    std::uint32_t x, std::uint32_t y,
    const DeveloperTextureSpec &spec) noexcept {
  switch (spec.kind) {
  case DeveloperTextureSpec::Kind::kGrid:
    return palette_color(grid_index(x, y, spec.grid_spacing));
  case DeveloperTextureSpec::Kind::kPlayer:
    return palette_color(player_index(x, y));
  case DeveloperTextureSpec::Kind::kWall:
    return palette_color(wall_index(x, y));
  }
  return palette_color(0U);
}

} // namespace

std::optional<DeveloperTextureAsset>
make_developer_texture(std::string_view material_name,
                       std::string_view source_uri) {
  const std::optional<DeveloperTextureSpec> spec = spec_for_name(material_name);
  if (!spec.has_value()) {
    return std::nullopt;
  }

  stellar::assets::ImageAsset image;
  image.name = std::string(spec->canonical_name);
  image.width = kDeveloperTextureSize;
  image.height = kDeveloperTextureSize;
  image.format = stellar::assets::ImageFormat::kR8G8B8A8;
  image.source_uri = std::string(source_uri) + "#developer_texture/" + image.name;
  image.pixels.reserve(static_cast<std::size_t>(image.width) * image.height * 4U);

  for (std::uint32_t y = 0; y < image.height; ++y) {
    for (std::uint32_t x = 0; x < image.width; ++x) {
      append_pixel(image, texture_color(x, y, *spec));
    }
  }

  stellar::assets::SamplerAsset sampler;
  sampler.name = image.name + "_nearest_repeat";
  sampler.mag_filter = stellar::assets::TextureFilter::kNearest;
  sampler.min_filter = stellar::assets::TextureFilter::kNearest;
  sampler.wrap_s = stellar::assets::TextureWrapMode::kRepeat;
  sampler.wrap_t = stellar::assets::TextureWrapMode::kRepeat;

  stellar::assets::TextureAsset texture;
  texture.name = image.name;
  texture.color_space = stellar::assets::TextureColorSpace::kSrgb;

  return DeveloperTextureAsset{.image = std::move(image),
                               .texture = std::move(texture),
                               .sampler = std::move(sampler)};
}

} // namespace stellar::import::bsp::detail
