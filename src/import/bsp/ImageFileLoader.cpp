#include "ImageFileLoader.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <cstdint>

namespace stellar::import::bsp::detail {

std::expected<stellar::assets::ImageAsset, std::string>
load_image_file_rgba8(const std::filesystem::path &path, std::string name) {
  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc *pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
  if (pixels == nullptr) {
    return std::unexpected("failed to decode image '" + path.string() + "'");
  }
  if (width <= 0 || height <= 0) {
    stbi_image_free(pixels);
    return std::unexpected("decoded image has invalid dimensions '" + path.string() + "'");
  }

  stellar::assets::ImageAsset image;
  image.name = std::move(name);
  image.width = static_cast<std::uint32_t>(width);
  image.height = static_cast<std::uint32_t>(height);
  image.format = stellar::assets::ImageFormat::kR8G8B8A8;
  image.source_uri = path.string();
  const std::size_t size =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
  image.pixels.assign(pixels, pixels + size);
  stbi_image_free(pixels);
  return image;
}

} // namespace stellar::import::bsp::detail
