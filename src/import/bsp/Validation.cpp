#include "stellar/import/bsp/Validation.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace stellar::import::bsp {
namespace {

void add_diagnostic(ImportReport &report, DiagnosticSeverity severity,
                    DiagnosticCode code, std::string source_uri,
                    std::string message) {
  if (severity == DiagnosticSeverity::kError) {
    report.has_errors = true;
  }
  report.diagnostics.push_back(Diagnostic{.severity = severity,
                                          .code = code,
                                          .message = std::move(message),
                                          .source_uri = std::move(source_uri)});
}

[[nodiscard]] DiagnosticCode classify_fatal_error(std::string_view message) {
  if (message.find("unsupported BSP version") != std::string_view::npos) {
    return DiagnosticCode::kUnsupportedVersion;
  }
  if (message.find("out of bounds") != std::string_view::npos ||
      message.find("negative offset or size") != std::string_view::npos) {
    return DiagnosticCode::kLumpOutOfBounds;
  }
  if (message.find("multiple of record size") != std::string_view::npos ||
      message.find("too small") != std::string_view::npos ||
      message.find("directory is invalid") != std::string_view::npos) {
    return DiagnosticCode::kMalformedLumpSize;
  }
  if (message.find("script binding") != std::string_view::npos ||
      message.find("parent path escape") != std::string_view::npos) {
    return DiagnosticCode::kScriptPathEscape;
  }
  if (message.find("entity") != std::string_view::npos ||
      message.find("unterminated") != std::string_view::npos) {
    return DiagnosticCode::kMalformedEntityText;
  }
  if (message.find("model references") != std::string_view::npos) {
    return DiagnosticCode::kInvalidModelReference;
  }
  return DiagnosticCode::kInvalidFaceReference;
}

[[nodiscard]] bool has_zero_scale(const stellar::assets::WorldMarker &marker) {
  return marker.scale[0] == 0.0F && marker.scale[1] == 0.0F &&
         marker.scale[2] == 0.0F;
}

void add_asset_diagnostics(const stellar::assets::LevelAsset &asset,
                           bool warn_on_missing_collision, ImportReport &report) {
  if (asset.geometry.meshes.empty() || asset.geometry.surfaces.empty()) {
    add_diagnostic(report, DiagnosticSeverity::kWarning,
                   DiagnosticCode::kNoWorldspawnGeometry, asset.source_uri,
                   asset.source_uri + ": BSP contains no imported worldspawn geometry");
  }

  std::size_t triangle_count = 0;
  std::unordered_set<std::string> collision_names;
  if (asset.level_collision.has_value()) {
    for (const auto &mesh : asset.level_collision->meshes) {
      triangle_count += mesh.triangles.size();
      if (mesh.name.empty()) {
        add_diagnostic(report, DiagnosticSeverity::kWarning,
                       DiagnosticCode::kEmptyCollisionMeshName, asset.source_uri,
                       asset.source_uri + ": BSP collision mesh has an empty name");
      }
      if (!collision_names.insert(mesh.name).second) {
        add_diagnostic(report, DiagnosticSeverity::kWarning,
                       DiagnosticCode::kDuplicateCollisionMeshName, asset.source_uri,
                       asset.source_uri + ": BSP collision mesh name is duplicated: " +
                           mesh.name);
      }
    }
  }
  if (warn_on_missing_collision && triangle_count == 0) {
    add_diagnostic(report, DiagnosticSeverity::kWarning,
                   DiagnosticCode::kNoCollisionTriangles, asset.source_uri,
                   asset.source_uri + ": BSP contains no collision triangles");
  }

  for (const auto &material : asset.geometry.materials) {
    if (!material.texture_index.has_value() && !material.source_name.empty()) {
      add_diagnostic(report, DiagnosticSeverity::kInfo,
                     DiagnosticCode::kMaterialFallbackUsed, asset.source_uri,
                     asset.source_uri + ": BSP material uses fallback texture data: " +
                         material.source_name);
    }
  }

  for (std::size_t index = 0; index < asset.world_metadata.markers.size(); ++index) {
    const auto &marker = asset.world_metadata.markers[index];
    if (marker.type == stellar::assets::WorldMarkerType::kTrigger && has_zero_scale(marker)) {
      add_diagnostic(report, DiagnosticSeverity::kWarning,
                     DiagnosticCode::kTriggerMissingVolume, asset.source_uri,
                     asset.source_uri + ": BSP trigger marker has no model bounds or extents: " +
                         marker.name);
      report.diagnostics.back().entity_index = index;
    }
    if (marker.type == stellar::assets::WorldMarkerType::kObjectCollider &&
        has_zero_scale(marker)) {
      add_diagnostic(report, DiagnosticSeverity::kWarning,
                     DiagnosticCode::kObjectColliderMissingVolume, asset.source_uri,
                     asset.source_uri +
                         ": BSP object collider marker has no model bounds or extents: " +
                         marker.name);
      report.diagnostics.back().entity_index = index;
    }
  }
}

[[nodiscard]] std::expected<std::vector<std::byte>, stellar::platform::Error>
read_file_bytes(std::string_view path) {
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
  return bytes;
}

} // namespace

std::expected<ValidationResult, stellar::platform::Error>
validate_level(std::string_view path, const ValidationOptions &options) {
  auto bytes = read_file_bytes(path);
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  return validate_level_from_memory(*bytes, std::string(path), options);
}

std::expected<ValidationResult, stellar::platform::Error>
validate_level_from_memory(std::span<const std::byte> bytes,
                           std::string source_uri,
                           const ValidationOptions &options) {
  ValidationResult validation{};
  auto loaded = load_level_from_memory_with_report(bytes, source_uri,
                                                   options.load_options);
  if (!loaded) {
    add_diagnostic(validation.report, DiagnosticSeverity::kError,
                   classify_fatal_error(loaded.error().message), source_uri,
                   loaded.error().message);
    validation.valid = false;
    return validation;
  }

  validation.report = loaded->report;
  validation.report.has_errors = std::ranges::any_of(
      validation.report.diagnostics, [](const Diagnostic &diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::kError;
      });
  add_asset_diagnostics(loaded->asset, options.warn_on_missing_collision,
                        validation.report);
  validation.valid = !validation.report.has_errors;
  validation.loaded_level = std::move(*loaded);
  return validation;
}

} // namespace stellar::import::bsp
