#include "BspAssetDebugNames.hpp"

namespace stellar::import::bsp::detail {

const char *texture_filter_name(stellar::assets::TextureFilter filter) noexcept {
  switch (filter) {
  case stellar::assets::TextureFilter::kUnspecified:
    return "unspecified";
  case stellar::assets::TextureFilter::kNearest:
    return "nearest";
  case stellar::assets::TextureFilter::kLinear:
    return "linear";
  case stellar::assets::TextureFilter::kNearestMipmapNearest:
    return "nearest_mipmap_nearest";
  case stellar::assets::TextureFilter::kLinearMipmapNearest:
    return "linear_mipmap_nearest";
  case stellar::assets::TextureFilter::kNearestMipmapLinear:
    return "nearest_mipmap_linear";
  case stellar::assets::TextureFilter::kLinearMipmapLinear:
    return "linear_mipmap_linear";
  }
  return "unknown";
}

const char *texture_wrap_name(stellar::assets::TextureWrapMode wrap) noexcept {
  switch (wrap) {
  case stellar::assets::TextureWrapMode::kClampToEdge:
    return "clamp_to_edge";
  case stellar::assets::TextureWrapMode::kMirroredRepeat:
    return "mirrored_repeat";
  case stellar::assets::TextureWrapMode::kRepeat:
    return "repeat";
  }
  return "unknown";
}

const char *image_format_name(stellar::assets::ImageFormat format) noexcept {
  switch (format) {
  case stellar::assets::ImageFormat::kUnknown:
    return "unknown";
  case stellar::assets::ImageFormat::kR8G8B8:
    return "r8g8b8";
  case stellar::assets::ImageFormat::kR8G8B8A8:
    return "r8g8b8a8";
  }
  return "unknown";
}

const char *texture_source_kind(std::string_view source_uri) noexcept {
  if (source_uri.find("#miptex/") != std::string_view::npos) {
    return "embedded_miptex";
  }
  if (source_uri.find("#wad3/") != std::string_view::npos) {
    return "external_wad";
  }
  if (source_uri.find("#developer_texture/") != std::string_view::npos) {
    return "procedural_developer";
  }
  return "unknown";
}

} // namespace stellar::import::bsp::detail
