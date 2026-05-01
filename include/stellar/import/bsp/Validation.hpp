#pragma once

#include "stellar/import/bsp/Diagnostics.hpp"
#include "stellar/import/bsp/Loader.hpp"
#include "stellar/platform/Error.hpp"

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace stellar::import::bsp {

/**
 * @brief Options controlling deterministic BSP validation behavior.
 */
struct ValidationOptions {
  /** @brief Loader behavior used while validating the BSP. */
  LoadOptions load_options{};

  /** @brief Emit a warning when imported level collision has no triangles. */
  bool warn_on_missing_collision = true;
};

/**
 * @brief Structured display-free BSP validation result.
 */
struct ValidationResult {
  /** @brief Imported level and non-fatal import report when parsing succeeded. */
  std::optional<LoadResult> loaded_level;

  /** @brief Deterministic diagnostics collected during validation. */
  ImportReport report;

  /** @brief True when validation found no error-severity diagnostics. */
  bool valid = true;
};

/**
 * @brief Validate a classic BSP29/BSP30 file from disk without renderer resources.
 */
[[nodiscard]] std::expected<ValidationResult, stellar::platform::Error>
validate_level(std::string_view path, const ValidationOptions &options = {});

/**
 * @brief Validate classic BSP29/BSP30 bytes without renderer resources.
 */
[[nodiscard]] std::expected<ValidationResult, stellar::platform::Error>
validate_level_from_memory(std::span<const std::byte> bytes,
                           std::string source_uri,
                           const ValidationOptions &options = {});

} // namespace stellar::import::bsp
