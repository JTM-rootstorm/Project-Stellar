#pragma once

#include "BspBinary.hpp"

#include <optional>
#include <string>

namespace stellar::import::bsp::detail {

void add_warning(ImportReport *report, DiagnosticCode code,
                 const std::string &source_uri, std::string message,
                 std::optional<std::size_t> lump_index = std::nullopt,
                 std::optional<std::size_t> face_index = std::nullopt);
void add_info(ImportReport *report, DiagnosticCode code,
              const std::string &source_uri, std::string message,
              std::optional<std::size_t> lump_index = std::nullopt,
              std::optional<std::size_t> face_index = std::nullopt);
void add_entity_warning(ImportReport *report, DiagnosticCode code,
                        const std::string &source_uri, std::size_t entity_index,
                        std::string message);

} // namespace stellar::import::bsp::detail
