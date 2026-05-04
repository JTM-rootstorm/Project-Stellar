#include "stellar/import/bsp/Loader.hpp"

#include "BspBinary.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

namespace stellar::import::bsp {

std::expected<stellar::assets::LevelAsset, stellar::platform::Error>
load_level(std::string_view path, const LoadOptions &options) {
  auto result = load_level_with_report(path, options);
  if (!result) {
    return std::unexpected(result.error());
  }
  return std::move(result->asset);
}

std::expected<LoadResult, stellar::platform::Error>
load_level_with_report(std::string_view path, const LoadOptions &options) {
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
  LoadOptions disk_options = options;
  if (disk_options.parse_material_sidecars) {
    const std::filesystem::path material_root =
        std::filesystem::path(std::string(path)).parent_path() / "materials";
    disk_options.material_search_roots.push_back(material_root);
  }
  return load_level_from_memory_with_report(bytes, std::string(path), disk_options);
}

std::expected<stellar::assets::LevelAsset, stellar::platform::Error>
load_level_from_memory(std::span<const std::byte> bytes, std::string source_uri,
                       const LoadOptions &options) {
  auto result = load_level_from_memory_with_report(bytes, std::move(source_uri),
                                                  options);
  if (!result) {
    return std::unexpected(result.error());
  }
  return std::move(result->asset);
}

std::expected<LoadResult, stellar::platform::Error>
load_level_from_memory_with_report(std::span<const std::byte> bytes,
                                   std::string source_uri,
                                   const LoadOptions &options) {
  auto map = detail::parse_bsp(bytes, source_uri, options);
  if (!map) {
    return std::unexpected(map.error());
  }
  auto entities = detail::parse_entities(map->entity_text, source_uri);
  if (!entities) {
    return std::unexpected(entities.error());
  }
  ImportReport report{};
  auto asset = detail::build_level_asset(std::move(*map), std::move(*entities),
                                         std::move(source_uri), options,
                                         &report);
  if (!asset) {
    return std::unexpected(asset.error());
  }
  return LoadResult{.asset = std::move(*asset), .report = std::move(report)};
}

} // namespace stellar::import::bsp
