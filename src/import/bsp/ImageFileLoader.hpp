#pragma once

#include "stellar/assets/ImageAsset.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace stellar::import::bsp::detail {

[[nodiscard]] std::expected<stellar::assets::ImageAsset, std::string>
load_image_file_rgba8(const std::filesystem::path &path, std::string name);

} // namespace stellar::import::bsp::detail
