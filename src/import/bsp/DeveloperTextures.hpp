#pragma once

#include <optional>
#include <string_view>

#include "stellar/assets/ImageAsset.hpp"
#include "stellar/assets/TextureAsset.hpp"

namespace stellar::import::bsp::detail {

/** @brief Procedural BSP developer texture payload for material fallback. */
struct DeveloperTextureAsset {
  stellar::assets::ImageAsset image;
  stellar::assets::TextureAsset texture;
  stellar::assets::SamplerAsset sampler;
};

/** @brief Returns a deterministic developer texture for a known BSP material name. */
[[nodiscard]] std::optional<DeveloperTextureAsset>
make_developer_texture(std::string_view material_name, std::string_view source_uri);

} // namespace stellar::import::bsp::detail
