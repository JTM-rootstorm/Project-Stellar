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
    /** @brief Informational diagnostic that does not indicate a problem. */
    kInfo,
    /** @brief Recoverable issue that may affect imported data quality. */
    kWarning,
    /** @brief Recoverable validation error reported alongside a parsed result. */
    kError,
};

/**
 * @brief Stable machine-readable BSP import diagnostic code.
 */
enum class DiagnosticCode {
    /** @brief BSP version is not one of the supported classic formats. */
    kUnsupportedVersion,
    /** @brief Lump offset or size extends beyond the source byte range. */
    kLumpOutOfBounds,
    /** @brief Lump byte size is not valid for the expected element type. */
    kMalformedLumpSize,
    /** @brief Entity text could not be parsed as valid BSP key/value data. */
    kMalformedEntityText,
    /** @brief Required worldspawn entity was not present. */
    kMissingWorldspawn,
    /** @brief No player spawn marker could be derived from the entity data. */
    kMissingPlayerSpawn,
    /** @brief Referenced texture data was missing. */
    kMissingTexture,
    /** @brief Texture payload format is not supported by the importer. */
    kUnsupportedTextureFormat,
    /** @brief Face references invalid edges, planes, texinfo, or related lump data. */
    kInvalidFaceReference,
    /** @brief Model references invalid faces, nodes, leaves, or related lump data. */
    kInvalidModelReference,
    /** @brief Visibility lump data is missing, truncated, or malformed. */
    kInvalidVisibilityData,
    /** @brief Lighting lump data is missing, truncated, or malformed. */
    kInvalidLightingData,
    /** @brief Entity key is preserved but not interpreted by the importer. */
    kUnsupportedEntityKey,
    /** @brief Worldspawn contains no importable geometry. */
    kNoWorldspawnGeometry,
    /** @brief Collision import produced no triangle data. */
    kNoCollisionTriangles,
    /** @brief Face polygon was degenerate and skipped. */
    kDegenerateFacePolygon,
    /** @brief Imported collision mesh name duplicates another mesh. */
    kDuplicateCollisionMeshName,
    /** @brief Imported collision mesh name was empty. */
    kEmptyCollisionMeshName,
    /** @brief Surface material fell back because richer source data was unavailable. */
    kMaterialFallbackUsed,
    /** @brief Object collider marker lacked usable volume metadata. */
    kObjectColliderMissingVolume,
    /** @brief Trigger marker lacked usable volume metadata. */
    kTriggerMissingVolume,
    /** @brief Script path attempted to escape the configured script root. */
    kScriptPathEscape,
    /** @brief Informational texture import statistics. */
    kTextureStats,
    /** @brief Informational lightmap import statistics. */
    kLightmapStats,
    /** @brief Imported lightmap data contains only black texels. */
    kAllBlackLightmap,
    /** @brief Material sidecar was found and loaded. */
    kMaterialSidecarLoaded,
    /** @brief Material sidecar could not be parsed or validated. */
    kMaterialSidecarInvalid,
    /** @brief Material sidecar contained an unknown key. */
    kMaterialSidecarUnknownKey,
    /** @brief Material sidecar referenced an unsafe texture path. */
    kMaterialSidecarUnsafePath,
    /** @brief Material sidecar referenced a texture file that could not be loaded. */
    kMaterialSidecarTextureMissing,
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
