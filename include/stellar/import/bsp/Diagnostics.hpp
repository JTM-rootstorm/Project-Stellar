#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace stellar::import::bsp {

/**
 * @brief Severity level for BSP import diagnostics that do not replace fatal errors.
 */
enum class DiagnosticSeverity {
    kInfo,
    kWarning,
    kError,
};

/**
 * @brief Stable machine-readable BSP import diagnostic code.
 */
enum class DiagnosticCode {
    kUnsupportedVersion,
    kLumpOutOfBounds,
    kMalformedLumpSize,
    kMalformedEntityText,
    kMissingWorldspawn,
    kMissingPlayerSpawn,
    kMissingTexture,
    kUnsupportedTextureFormat,
    kInvalidFaceReference,
    kInvalidModelReference,
    kInvalidVisibilityData,
    kInvalidLightingData,
    kUnsupportedEntityKey,
    kNoWorldspawnGeometry,
    kNoCollisionTriangles,
    kDegenerateFacePolygon,
    kDuplicateCollisionMeshName,
    kEmptyCollisionMeshName,
    kMaterialFallbackUsed,
    kObjectColliderMissingVolume,
    kTriggerMissingVolume,
    kScriptPathEscape,
};

/**
 * @brief Structured BSP import diagnostic for warnings and recoverable validation issues.
 */
struct Diagnostic {
    /** @brief Diagnostic severity. */
    DiagnosticSeverity severity = DiagnosticSeverity::kInfo;

    /** @brief Stable diagnostic code. */
    DiagnosticCode code = DiagnosticCode::kUnsupportedEntityKey;

    /** @brief Human-readable diagnostic message. */
    std::string message;

    /** @brief Source URI associated with the diagnostic. */
    std::string source_uri;

    /** @brief Optional BSP lump index associated with the diagnostic. */
    std::optional<std::size_t> lump_index;

    /** @brief Optional entity index associated with the diagnostic. */
    std::optional<std::size_t> entity_index;

    /** @brief Optional face index associated with the diagnostic. */
    std::optional<std::size_t> face_index;
};

/**
 * @brief Accumulated non-fatal BSP import diagnostics.
 */
struct ImportReport {
    /** @brief Diagnostics emitted during import. */
    std::vector<Diagnostic> diagnostics;

    /** @brief True when report diagnostics include recoverable error severity entries. */
    bool has_errors = false;
};

} // namespace stellar::import::bsp
