#pragma once

#include "stellar/assets/LevelAsset.hpp"

#include <string_view>

namespace stellar::import::bsp::detail {

[[nodiscard]] const char *texture_filter_name(
    stellar::assets::TextureFilter filter) noexcept;
[[nodiscard]] const char *texture_wrap_name(
    stellar::assets::TextureWrapMode wrap) noexcept;
[[nodiscard]] const char *image_format_name(stellar::assets::ImageFormat format) noexcept;
[[nodiscard]] const char *texture_source_kind(std::string_view source_uri) noexcept;

} // namespace stellar::import::bsp::detail
