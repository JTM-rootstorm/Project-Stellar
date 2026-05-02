#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "stellar/server/WorldSession.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::network {

/** @brief Wire protocol version required by the first-stage session handshake. */
using ProtocolVersion = std::uint32_t;

/** @brief Transport-local connection identifier reserved for future socket backends. */
using ConnectionId = std::uint32_t;

/** @brief Server-authored session identifier assigned after a successful welcome. */
using SessionId = std::uint64_t;

/** @brief Current deterministic client/server session lifecycle state. */
enum class SessionState : std::uint8_t {
    kDisconnected,
    kConnecting,
    kConnected,
    kRejected,
    kTimedOut,
};

/** @brief Current deterministic socket-transport protocol version. */
inline constexpr ProtocolVersion kCurrentProtocolVersion = 1;

/** @brief Stable map compatibility identity exchanged during session handshakes. */
struct MapIdentity {
    /** @brief Canonical compatibility id, normally derived from the source URI basename. */
    std::string map_id;

    /** @brief Imported source URI or logical asset identifier used for diagnostics. */
    std::string source_uri;

    /** @brief Optional deterministic non-cryptographic source URI/content hash. */
    std::uint64_t content_hash = 0;
};

/** @brief Client-to-server session hello sent before any authoritative input is accepted. */
struct ClientHello {
    /** @brief Protocol version supported by the client. */
    ProtocolVersion protocol_version = kCurrentProtocolVersion;

    /** @brief Bounded diagnostic client name. */
    std::string client_name;

    /** @brief Requested map compatibility id. */
    std::string requested_map_id;

    /** @brief Client-generated nonce echoed only through deterministic diagnostics later. */
    std::uint64_t client_nonce = 0;
};

/** @brief Server-to-client welcome or deterministic rejection for a session hello. */
struct ServerWelcome {
    /** @brief True when the server accepted the session. */
    bool accepted = false;

    /** @brief Server protocol version used for compatibility checks. */
    ProtocolVersion protocol_version = kCurrentProtocolVersion;

    /** @brief Server-assigned session id, zero on rejection. */
    SessionId session_id = 0;

    /** @brief Server-assigned authoritative player id, zero on rejection. */
    stellar::server::PlayerId assigned_player_id = 0;

    /** @brief Accepted server map id or expected map id on map rejection. */
    std::string map_id;

    /** @brief Stable rejection code, empty when accepted. */
    std::string rejection_code;

    /** @brief Human-readable deterministic diagnostic. */
    std::string message;
};

/** @brief Return a deterministic FNV-1a hash for short identity strings. */
[[nodiscard]] std::uint64_t deterministic_identity_hash(std::string_view value) noexcept;

/** @brief Build a deterministic map identity from a source URI. */
[[nodiscard]] MapIdentity make_map_identity(std::string_view source_uri);

/** @brief Build a deterministic map identity from runtime world source metadata. */
[[nodiscard]] MapIdentity make_map_identity(const stellar::world::RuntimeWorld& world);

} // namespace stellar::network
