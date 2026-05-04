#include "BspImportDiagnostics.hpp"

#include <utility>

namespace stellar::import::bsp::detail {

void add_warning(ImportReport *report, DiagnosticCode code,
                 const std::string &source_uri, std::string message,
                 std::optional<std::size_t> lump_index,
                 std::optional<std::size_t> face_index) {
  if (report == nullptr) {
    return;
  }
  report->diagnostics.push_back(Diagnostic{.severity = DiagnosticSeverity::kWarning,
                                           .code = code,
                                           .message = std::move(message),
                                           .source_uri = source_uri,
                                           .lump_index = lump_index,
                                           .entity_index = std::nullopt,
                                           .face_index = face_index});
}

void add_info(ImportReport *report, DiagnosticCode code,
              const std::string &source_uri, std::string message,
              std::optional<std::size_t> lump_index,
              std::optional<std::size_t> face_index) {
  if (report == nullptr) {
    return;
  }
  report->diagnostics.push_back(Diagnostic{.severity = DiagnosticSeverity::kInfo,
                                           .code = code,
                                           .message = std::move(message),
                                           .source_uri = source_uri,
                                           .lump_index = lump_index,
                                           .entity_index = std::nullopt,
                                           .face_index = face_index});
}

void add_entity_warning(ImportReport *report, DiagnosticCode code,
                        const std::string &source_uri, std::size_t entity_index,
                        std::string message) {
  if (report == nullptr) {
    return;
  }
  report->diagnostics.push_back(Diagnostic{.severity = DiagnosticSeverity::kWarning,
                                           .code = code,
                                           .message = std::move(message),
                                           .source_uri = source_uri,
                                           .lump_index = static_cast<std::size_t>(
                                               LumpIndex::kEntities),
                                           .entity_index = entity_index,
                                           .face_index = std::nullopt});
}

} // namespace stellar::import::bsp::detail
