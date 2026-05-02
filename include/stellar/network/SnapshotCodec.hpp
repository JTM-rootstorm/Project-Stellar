#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "stellar/network/Messages.hpp"
#include "stellar/network/Session.hpp"
#include "stellar/network/SnapshotDelta.hpp"

namespace stellar::network {

/** @brief Binary snapshot/event codec failure. */
struct CodecError {
    /** @brief Stable machine-readable error code. */
    std::string code;

    /** @brief Human-readable deterministic diagnostic. */
    std::string message;
};

/** @brief Conservative bounds for deterministic local transport serialization. */
struct CodecLimits {
    /** @brief Maximum decoded string byte length. */
    std::uint32_t max_string_bytes = 4096;

    /** @brief Maximum decoded vector element count. */
    std::uint32_t max_vector_count = 4096;
};

/** @brief Encode an authoritative network snapshot into deterministic little-endian bytes. */
[[nodiscard]] std::expected<std::vector<std::uint8_t>, CodecError> encode_snapshot(
    const NetworkWorldSnapshot& snapshot,
    CodecLimits limits = {});

/** @brief Encode a client movement command request into deterministic little-endian bytes. */
[[nodiscard]] std::expected<std::vector<std::uint8_t>, CodecError> encode_player_command(
    const NetworkPlayerCommand& command,
    CodecLimits limits = {});

/** @brief Decode a client movement command request from deterministic little-endian bytes. */
[[nodiscard]] std::expected<NetworkPlayerCommand, CodecError> decode_player_command(
    const std::vector<std::uint8_t>& bytes,
    CodecLimits limits = {});

/** @brief Encode a client session hello into deterministic little-endian bytes. */
[[nodiscard]] std::expected<std::vector<std::uint8_t>, CodecError> encode_client_hello(
    const ClientHello& hello,
    CodecLimits limits = {});

/** @brief Decode a client session hello from deterministic little-endian bytes. */
[[nodiscard]] std::expected<ClientHello, CodecError> decode_client_hello(
    const std::vector<std::uint8_t>& bytes,
    CodecLimits limits = {});

/** @brief Encode a server session welcome/rejection into deterministic little-endian bytes. */
[[nodiscard]] std::expected<std::vector<std::uint8_t>, CodecError> encode_server_welcome(
    const ServerWelcome& welcome,
    CodecLimits limits = {});

/** @brief Decode a server session welcome/rejection from deterministic little-endian bytes. */
[[nodiscard]] std::expected<ServerWelcome, CodecError> decode_server_welcome(
    const std::vector<std::uint8_t>& bytes,
    CodecLimits limits = {});

/** @brief Decode an authoritative network snapshot from deterministic little-endian bytes. */
[[nodiscard]] std::expected<NetworkWorldSnapshot, CodecError> decode_snapshot(
    const std::vector<std::uint8_t>& bytes,
    CodecLimits limits = {});

/** @brief Encode one server-approved gameplay event into deterministic little-endian bytes. */
[[nodiscard]] std::expected<std::vector<std::uint8_t>, CodecError> encode_gameplay_event(
    const GameplayEvent& event,
    CodecLimits limits = {});

/** @brief Decode one server-approved gameplay event from deterministic little-endian bytes. */
[[nodiscard]] std::expected<GameplayEvent, CodecError> decode_gameplay_event(
    const std::vector<std::uint8_t>& bytes,
    CodecLimits limits = {});

/** @brief Encode a structural snapshot delta into deterministic little-endian bytes. */
[[nodiscard]] std::expected<std::vector<std::uint8_t>, CodecError> encode_snapshot_delta(
    const SnapshotDelta& delta,
    CodecLimits limits = {});

/** @brief Decode a structural snapshot delta from deterministic little-endian bytes. */
[[nodiscard]] std::expected<SnapshotDelta, CodecError> decode_snapshot_delta(
    const std::vector<std::uint8_t>& bytes,
    CodecLimits limits = {});

} // namespace stellar::network
