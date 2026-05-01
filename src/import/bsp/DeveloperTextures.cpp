#include "DeveloperTextures.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <utility>

namespace stellar::import::bsp::detail {
namespace {

struct DeveloperTextureSpec {
  std::string_view canonical_name;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
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
                DeveloperTextureSpec{"stellar_dev_grid_12", 12, 12,
                                     DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"dev/grid_12"},
                DeveloperTextureSpec{"stellar_dev_grid_12", 12, 12,
                                     DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"stellar_dev_grid_16"},
                DeveloperTextureSpec{"stellar_dev_grid_16", 16, 16,
                                     DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"dev/grid_16"},
                DeveloperTextureSpec{"stellar_dev_grid_16", 16, 16,
                                     DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"stellar_dev_grid_32"},
                DeveloperTextureSpec{"stellar_dev_grid_32", 32, 32,
                                     DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"dev/grid_32"},
                DeveloperTextureSpec{"stellar_dev_grid_32", 32, 32,
                                     DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"stellar_dev_grid_64"},
                DeveloperTextureSpec{"stellar_dev_grid_64", 64, 64,
                                     DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"dev/grid_64"},
                DeveloperTextureSpec{"stellar_dev_grid_64", 64, 64,
                                     DeveloperTextureSpec::Kind::kGrid}},
      std::pair{std::string_view{"stellar_dev_player_72"},
                DeveloperTextureSpec{"stellar_dev_player_72", 24, 72,
                                     DeveloperTextureSpec::Kind::kPlayer}},
      std::pair{std::string_view{"dev/player_72"},
                DeveloperTextureSpec{"stellar_dev_player_72", 24, 72,
                                     DeveloperTextureSpec::Kind::kPlayer}},
      std::pair{std::string_view{"stellar_dev_wall_96"},
                DeveloperTextureSpec{"stellar_dev_wall_96", 24, 96,
                                     DeveloperTextureSpec::Kind::kWall}},
      std::pair{std::string_view{"dev/wall_96"},
                DeveloperTextureSpec{"stellar_dev_wall_96", 24, 96,
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

[[nodiscard]] std::array<std::uint8_t, 4>
grid_color(std::uint32_t x, std::uint32_t y, std::uint32_t size) noexcept {
  if (x == 0 || y == 0 || x + 1 == size || y + 1 == size) {
    return {255U, 255U, 255U, 255U};
  }
  if (x % 4U == 0U || y % 4U == 0U) {
    return {96U, 140U, 180U, 255U};
  }
  const bool checker = ((x / 2U) + (y / 2U)) % 2U == 0U;
  return checker ? std::array<std::uint8_t, 4>{34U, 42U, 54U, 255U}
                 : std::array<std::uint8_t, 4>{45U, 55U, 70U, 255U};
}

[[nodiscard]] std::array<std::uint8_t, 4>
height_color(std::uint32_t x, std::uint32_t y, const DeveloperTextureSpec &spec) noexcept {
  const std::uint32_t major = spec.kind == DeveloperTextureSpec::Kind::kPlayer ? 12U : 16U;
  const bool edge = x == 0 || x + 1 == spec.width || y == 0 || y + 1 == spec.height;
  if (edge) {
    return spec.kind == DeveloperTextureSpec::Kind::kPlayer
               ? std::array<std::uint8_t, 4>{255U, 220U, 80U, 255U}
               : std::array<std::uint8_t, 4>{255U, 120U, 80U, 255U};
  }
  if (y % major == 0U) {
    return {255U, 255U, 255U, 255U};
  }
  if (spec.kind == DeveloperTextureSpec::Kind::kPlayer && y == 64U) {
    return {80U, 220U, 255U, 255U};
  }
  const bool column = x == spec.width / 2U;
  if (column) {
    return {100U, 130U, 170U, 255U};
  }
  return spec.kind == DeveloperTextureSpec::Kind::kPlayer
             ? std::array<std::uint8_t, 4>{38U, 48U, 68U, 255U}
             : std::array<std::uint8_t, 4>{48U, 42U, 38U, 255U};
}

} // namespace

std::optional<DeveloperTextureAsset>
make_developer_texture(std::string_view material_name, std::string_view source_uri) {
  const std::optional<DeveloperTextureSpec> spec = spec_for_name(material_name);
  if (!spec.has_value()) {
    return std::nullopt;
  }

  stellar::assets::ImageAsset image;
  image.name = std::string(spec->canonical_name);
  image.width = spec->width;
  image.height = spec->height;
  image.format = stellar::assets::ImageFormat::kR8G8B8A8;
  image.source_uri = std::string(source_uri) + "#developer_texture/" + image.name;
  image.pixels.reserve(static_cast<std::size_t>(image.width) * image.height * 4U);

  for (std::uint32_t y = 0; y < image.height; ++y) {
    for (std::uint32_t x = 0; x < image.width; ++x) {
      if (spec->kind == DeveloperTextureSpec::Kind::kGrid) {
        append_pixel(image, grid_color(x, y, spec->width));
      } else {
        append_pixel(image, height_color(x, y, *spec));
      }
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
