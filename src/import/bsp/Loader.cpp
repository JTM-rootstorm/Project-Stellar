#include "stellar/import/bsp/Loader.hpp"

#include "BspBinary.hpp"

#include <fstream>
#include <iterator>
#include <vector>

namespace stellar::import::bsp {

std::expected<stellar::assets::LevelAsset, stellar::platform::Error>
load_level(std::string_view path, const LoadOptions &options) {
  std::ifstream file(std::string(path), std::ios::binary);
  if (!file) {
    return std::unexpected(stellar::platform::Error(
        "Failed to open BSP file: " + std::string(path)));
  }
  std::vector<std::byte> bytes;
  char ch = 0;
  while (file.get(ch)) {
    bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
  }
  return load_level_from_memory(bytes, std::string(path), options);
}

std::expected<stellar::assets::LevelAsset, stellar::platform::Error>
load_level_from_memory(std::span<const std::byte> bytes, std::string source_uri,
                       const LoadOptions &options) {
  auto map = detail::parse_bsp(bytes, source_uri, options);
  if (!map) {
    return std::unexpected(map.error());
  }
  auto entities = detail::parse_entities(map->entity_text, source_uri);
  if (!entities) {
    return std::unexpected(entities.error());
  }
  return detail::build_level_asset(std::move(*map), std::move(*entities),
                                   std::move(source_uri), options);
}

} // namespace stellar::import::bsp
