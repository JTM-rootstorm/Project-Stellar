#include "stellar/protocol/Types.hpp"

#include <string_view>

namespace stellar::protocol {
namespace {

[[nodiscard]] std::string basename_from_uri(std::string_view source_uri) {
    const std::size_t slash = source_uri.find_last_of("/\\");
    const std::size_t offset = slash == std::string_view::npos ? 0 : slash + 1;
    std::string base{source_uri.substr(offset)};
    return base.empty() ? std::string{"local"} : base;
}

} // namespace

std::uint64_t deterministic_identity_hash(std::string_view value) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char byte : value) {
        hash ^= static_cast<std::uint8_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

MapIdentity make_map_identity(std::string_view source_uri) {
    const std::string uri{source_uri};
    return MapIdentity{.map_id = basename_from_uri(source_uri),
                       .source_uri = uri,
                       .content_hash = deterministic_identity_hash(uri)};
}

} // namespace stellar::protocol
