#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace stellar::protocol {

/** @brief Wire protocol version required by the first-stage session handshake. */
using ProtocolVersion = std::uint32_t;

/** @brief Transport-local connection identifier reserved for future socket backends. */
using ConnectionId = std::uint32_t;

/** @brief Server-authored session identifier assigned after a successful welcome. */
using SessionId = std::uint64_t;

/** @brief Stable authoritative player slot identifier. */
using PlayerId = std::uint32_t;

/** @brief Stable authoritative gameplay entity identifier. */
using EntityId = std::uint32_t;

/** @brief Current deterministic socket-transport protocol version. */
inline constexpr ProtocolVersion kCurrentProtocolVersion = 1;

/** @brief Client-to-server transport message categories. */
enum class ClientMessageType : std::uint8_t {
    /** @brief Client movement/view intent that must be validated by authority. */
    kInputCommand = 1,

    /** @brief Session handshake request sent before authoritative input is accepted. */
    kClientHello = 2,
};

/** @brief Server-to-client transport message categories. */
enum class ServerMessageType : std::uint8_t {
    /** @brief Session handshake response accepting or rejecting the client. */
    kWelcome = 1,

    /** @brief Complete authoritative world snapshot for client presentation. */
    kWorldSnapshot = 2,

    /** @brief Delta from an explicit authoritative snapshot baseline. */
    kWorldDelta = 3,

    /** @brief Server-approved gameplay event intended for presentation systems. */
    kGameplayEvent = 4,
};

/** @brief Current deterministic client/server session lifecycle state. */
enum class SessionState : std::uint8_t {
    /** @brief No active transport session exists. */
    kDisconnected,

    /** @brief Client hello has been sent or a connection attempt is in progress. */
    kConnecting,

    /** @brief Session handshake succeeded and authoritative messages may flow. */
    kConnected,

    /** @brief Server rejected the session deterministically. */
    kRejected,

    /** @brief Session expired after transport or authority progress stopped. */
    kTimedOut,
};

/** @brief Minimal protocol gameplay entity category. */
enum class EntityKind : std::uint8_t {
    /** @brief Authoritative player-controlled entity. */
    kPlayer,

    /** @brief Presentation billboard or authored sprite marker. */
    kSprite,

    /** @brief Authority-owned pickup entity that can emit collection events. */
    kPickup,

    /** @brief Authority-owned door or gate entity with open/closed state. */
    kDoor,

    /** @brief Authored trigger volume represented in protocol metadata. */
    kTrigger,

    /** @brief Authored object-collider marker exposed to protocol consumers. */
    kObjectCollider,
};

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
    PlayerId assigned_player_id = 0;

    /** @brief Accepted server map id or expected map id on map rejection. */
    std::string map_id;

    /** @brief Stable rejection code, empty when accepted. */
    std::string rejection_code;

    /** @brief Human-readable deterministic diagnostic. */
    std::string message;
};

/** @brief Client-requested movement intent for protocol transport. */
struct MovementCommand {
    /** @brief Requested movement direction; sanitized and clamped by the server. */
    std::array<float, 3> wish_direction{};

    /** @brief Requested jump intent; interpreted only by authority. */
    bool jump = false;

    /** @brief Requested authoritative view yaw in degrees around +Z. */
    float view_yaw_degrees = 0.0F;

    /** @brief Requested authoritative view pitch in degrees around camera right. */
    float view_pitch_degrees = 0.0F;

    /** @brief True when view angles should update server-owned view state. */
    bool has_view_angles = false;
};

/** @brief Transport-ready client movement command request. */
struct NetworkPlayerCommand {
    /** @brief Authoritative player slot requested by the client. */
    PlayerId player_id = 0;

    /** @brief Monotonic client command sequence for later acknowledgement paths. */
    std::uint64_t command_sequence = 0;

    /** @brief Movement intent to be validated and sanitized by the server. */
    MovementCommand movement{};
};

/** @brief Plain transform component copied to protocol snapshots. */
struct TransformComponent {
    /** @brief World-space entity origin or center. */
    std::array<float, 3> position{0.0F, 0.0F, 0.0F};

    /** @brief World-space orientation as x, y, z, w quaternion components. */
    std::array<float, 4> rotation{0.0F, 0.0F, 0.0F, 1.0F};

    /** @brief Authored scale or world-space size depending on marker source. */
    std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
};

/** @brief Serializable marker/collision metadata preserved on protocol entities. */
struct GameplayEntityMetadata {
    /** @brief Authored marker, collider, mesh, or entity name. */
    std::string name;

    /** @brief Authored gameplay archetype label. */
    std::string archetype;

    /** @brief Optional sprite/material identifier for billboard presentation binding. */
    std::string sprite_id;

    /** @brief Optional source marker type name for diagnostics and deterministic tests. */
    std::string source_type;

    /** @brief Optional source property payload retained as inert data. */
    std::string extras_json;

    /** @brief Authored display size or collider half extent when available. */
    std::array<float, 3> size{1.0F, 1.0F, 1.0F};

    /** @brief Authored alpha value for presentation binding. */
    float alpha = 1.0F;

    /** @brief Runtime object collider id when this entity came from an object-collider marker. */
    std::uint32_t object_collider_id = 0;

    /** @brief Collision mesh index when this entity came from named collision metadata. */
    std::uint32_t collision_mesh_index = 0;
};

/** @brief Transport-ready server-owned gameplay entity state. */
struct NetworkGameplayEntity {
    /** @brief Stable authoritative entity identifier. */
    EntityId id = 0;

    /** @brief Authoritative entity category. */
    EntityKind kind = EntityKind::kSprite;

    /** @brief Plain authoritative transform. */
    TransformComponent transform{};

    /** @brief Serializable authoritative gameplay metadata. */
    GameplayEntityMetadata metadata{};

    /** @brief True when the authoritative entity is active/presentable. */
    bool active = true;

    /** @brief Door/gate authoritative open state when applicable. */
    bool open = false;
};

/** @brief Authoritative player state exposed in protocol snapshots. */
struct PlayerSnapshot {
    /** @brief Stable authoritative player slot identifier. */
    PlayerId player_id = 0;

    /** @brief Authoritative world-space character center position. */
    std::array<float, 3> position{};

    /** @brief Authoritative world-space character velocity. */
    std::array<float, 3> velocity{};

    /** @brief Authoritative world-space orientation as x, y, z, w quaternion components. */
    std::array<float, 4> rotation{0.0F, 0.0F, 0.0F, 1.0F};

    /** @brief True when the authoritative player is supported by walkable collision. */
    bool grounded = false;
};

/** @brief Transport-ready authoritative world snapshot. */
struct NetworkWorldSnapshot {
    /** @brief Authoritative server tick that produced this snapshot. */
    std::uint64_t tick = 0;

    /** @brief Deterministically ordered authoritative player states. */
    std::vector<PlayerSnapshot> players;

    /** @brief Deterministically ordered authoritative gameplay entity states. */
    std::vector<NetworkGameplayEntity> entities;
};

/** @brief Server-authored gameplay/presentation event categories. */
enum class GameplayEventKind : std::uint8_t {
    /** @brief Authority confirmed a pickup collection. */
    kPickupCollected = 1,

    /** @brief Authority changed a door or gate open state. */
    kDoorStateChanged = 2,

    /** @brief Authority applied a scripted command result. */
    kScriptCommandApplied = 3,

    /** @brief Authority surfaced a deterministic script error diagnostic. */
    kScriptError = 4,
};

/** @brief Server-approved presentation event derived from authoritative runtime activity. */
struct GameplayEvent {
    /** @brief Event category. */
    GameplayEventKind kind = GameplayEventKind::kScriptCommandApplied;

    /** @brief Authoritative tick associated with the event. */
    std::uint64_t tick = 0;

    /** @brief Related authoritative entity/collider id when known, otherwise zero. */
    std::uint32_t entity_id = 0;

    /** @brief Related authoritative player id when known, otherwise zero. */
    std::uint32_t player_id = 0;

    /** @brief Stable event or result code for tools/presentation routing. */
    std::string code;

    /** @brief Human-readable deterministic event diagnostic. */
    std::string message;
};

/** @brief Return a deterministic FNV-1a hash for short identity strings. */
[[nodiscard]] std::uint64_t deterministic_identity_hash(std::string_view value) noexcept;

/** @brief Build a deterministic map identity from a source URI. */
[[nodiscard]] MapIdentity make_map_identity(std::string_view source_uri);

} // namespace stellar::protocol
