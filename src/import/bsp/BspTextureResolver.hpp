#pragma once

#include "BspBinary.hpp"
#include "Wad3Reader.hpp"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

namespace stellar::import::bsp::detail {

struct TextureBuildResult {
  std::optional<std::size_t> texture_index;
  std::optional<std::array<std::uint32_t, 2>> texel_size;
};

[[nodiscard]] std::string texture_name_for(const BspMap &map, const Face &face);
[[nodiscard]] const Miptex *texture_for_face(const BspMap &map,
                                             const Face &face) noexcept;
[[nodiscard]] std::string texture_lookup_key(std::string_view name);
[[nodiscard]] TextureBuildResult
texture_asset_index(stellar::assets::LevelGeometryAsset &geometry,
                    std::map<std::string, std::size_t> &textures,
                    const std::unordered_map<std::string, WadTextureAsset> &wad_textures,
                    const std::string &texture_name, const Miptex *miptex,
                    const std::string &source_uri, ImportReport *report);
[[nodiscard]] std::size_t
material_index(stellar::assets::LevelGeometryAsset &geometry,
               std::map<std::string, std::size_t> &materials,
               const std::string &name, std::optional<std::size_t> texture_index,
               std::optional<std::size_t> lightmap_index,
               std::optional<stellar::assets::MaterialAsset> resolved_material =
                   std::nullopt);

} // namespace stellar::import::bsp::detail
