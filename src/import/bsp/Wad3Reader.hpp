#pragma once

#include "stellar/assets/LevelAsset.hpp"
#include "stellar/platform/Error.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace stellar::import::bsp::detail {

struct WadTextureAsset {
  stellar::assets::ImageAsset image;
};

struct WadResolveContext {
  std::filesystem::path map_directory;
  std::vector<std::filesystem::path> search_roots;
  bool allow_absolute_wad_paths = false;
};

struct WadResolveResult {
  std::unordered_map<std::string, WadTextureAsset> textures;
  std::vector<std::string> diagnostics;
};

[[nodiscard]] WadResolveContext make_wad_resolve_context(std::string_view source_uri);

[[nodiscard]] WadResolveResult resolve_wad_textures(std::string_view wad_key,
                                                    const WadResolveContext &context);

} // namespace stellar::import::bsp::detail
