#pragma once

#include "stellar/assets/LevelAsset.hpp"
#include "stellar/import/bsp/Diagnostics.hpp"
#include "stellar/platform/Error.hpp"

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace stellar::import::bsp {

/**
 * @brief Options controlling classic BSP level import behavior.
 */
struct LoadOptions {
  /** @brief Preserve raw entity key/value metadata on generated world markers.
   */
  bool preserve_raw_entities = true;

  /** @brief Build triangle collision data from BSP polygon faces. */
  bool build_triangle_collision_from_faces = true;

  /** @brief Parse visibility lump presence and store availability metadata. */
  bool parse_visibility = true;

  /** @brief Parse lighting lump presence for diagnostics and future lightmap
   * import. */
  bool parse_lightmaps = true;
};

/**
 * @brief BSP import result containing the asset and non-fatal import report.
 */
struct LoadResult {
  /** @brief Imported source-neutral level asset. */
  stellar::assets::LevelAsset asset;

  /** @brief Non-fatal diagnostics collected during import. */
  ImportReport report;
};

/**
 * @brief Load a classic BSP29/BSP30 file from disk into a source-neutral
 * LevelAsset.
 */
[[nodiscard]] std::expected<stellar::assets::LevelAsset,
                            stellar::platform::Error>
load_level(std::string_view path, const LoadOptions &options = {});

/**
 * @brief Load a classic BSP29/BSP30 file from disk with non-fatal diagnostics.
 */
[[nodiscard]] std::expected<LoadResult, stellar::platform::Error>
load_level_with_report(std::string_view path, const LoadOptions &options = {});

/**
 * @brief Load classic BSP29/BSP30 bytes into a source-neutral LevelAsset.
 */
[[nodiscard]] std::expected<stellar::assets::LevelAsset,
                            stellar::platform::Error>
load_level_from_memory(std::span<const std::byte> bytes, std::string source_uri,
                       const LoadOptions &options = {});

/**
 * @brief Load classic BSP29/BSP30 bytes with non-fatal diagnostics.
 */
[[nodiscard]] std::expected<LoadResult, stellar::platform::Error>
load_level_from_memory_with_report(std::span<const std::byte> bytes,
                                   std::string source_uri,
                                   const LoadOptions &options = {});

} // namespace stellar::import::bsp
