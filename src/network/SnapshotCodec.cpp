#include "stellar/network/SnapshotCodec.hpp"

#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace stellar::network {
namespace {

constexpr std::uint32_t kSnapshotMagic = 0x53504E33U;
constexpr std::uint32_t kCommandMagic = 0x434D4E33U;
constexpr std::uint32_t kEventMagic = 0x45564E33U;
constexpr std::uint32_t kDeltaMagic = 0x444C4E33U;
constexpr std::uint16_t kCodecVersion = 1;

[[nodiscard]] CodecError error(std::string code, std::string message) {
    return CodecError{std::move(code), std::move(message)};
}

class Writer {
public:
    explicit Writer(CodecLimits limits) : limits_(limits) {}

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::vector<std::uint8_t> take_bytes() { return std::move(bytes_); }

    [[nodiscard]] std::expected<void, CodecError> write_bool(bool value) {
        bytes_.push_back(value ? std::uint8_t{1} : std::uint8_t{0});
        return {};
    }

    [[nodiscard]] std::expected<void, CodecError> write_u8(std::uint8_t value) {
        bytes_.push_back(value);
        return {};
    }

    [[nodiscard]] std::expected<void, CodecError> write_u16(std::uint16_t value) {
        for (int byte = 0; byte < 2; ++byte) {
            bytes_.push_back(static_cast<std::uint8_t>((value >> (byte * 8)) & 0xFFU));
        }
        return {};
    }

    [[nodiscard]] std::expected<void, CodecError> write_u32(std::uint32_t value) {
        for (int byte = 0; byte < 4; ++byte) {
            bytes_.push_back(static_cast<std::uint8_t>((value >> (byte * 8)) & 0xFFU));
        }
        return {};
    }

    [[nodiscard]] std::expected<void, CodecError> write_u64(std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            bytes_.push_back(static_cast<std::uint8_t>((value >> (byte * 8)) & 0xFFU));
        }
        return {};
    }

    [[nodiscard]] std::expected<void, CodecError> write_float(float value) {
        if (!std::isfinite(value)) {
            return std::unexpected(error("non_finite_float", "Refusing to encode non-finite float"));
        }
        return write_u32(std::bit_cast<std::uint32_t>(value));
    }

    [[nodiscard]] std::expected<void, CodecError> write_string(const std::string& value) {
        if (value.size() > limits_.max_string_bytes ||
            value.size() > std::numeric_limits<std::uint32_t>::max()) {
            return std::unexpected(error("string_too_large", "String exceeds codec limit"));
        }
        auto size_result = write_u32(static_cast<std::uint32_t>(value.size()));
        if (!size_result) {
            return size_result;
        }
        bytes_.insert(bytes_.end(), value.begin(), value.end());
        return {};
    }

    [[nodiscard]] std::expected<void, CodecError> write_count(std::size_t count) {
        if (count > limits_.max_vector_count || count > std::numeric_limits<std::uint32_t>::max()) {
            return std::unexpected(error("vector_too_large", "Vector exceeds codec limit"));
        }
        return write_u32(static_cast<std::uint32_t>(count));
    }

private:
    CodecLimits limits_{};
    std::vector<std::uint8_t> bytes_;
};

class Reader {
public:
    Reader(const std::vector<std::uint8_t>& bytes, CodecLimits limits)
        : bytes_(bytes), limits_(limits) {}

    [[nodiscard]] bool finished() const noexcept { return offset_ == bytes_.size(); }

    [[nodiscard]] std::expected<bool, CodecError> read_bool() {
        auto value = read_u8();
        if (!value) {
            return std::unexpected(value.error());
        }
        if (*value > 1U) {
            return std::unexpected(error("invalid_bool", "Decoded boolean is not 0 or 1"));
        }
        return *value == 1U;
    }

    [[nodiscard]] std::expected<std::uint8_t, CodecError> read_u8() {
        if (offset_ + 1 > bytes_.size()) {
            return std::unexpected(error("truncated", "Unexpected end of input"));
        }
        return bytes_[offset_++];
    }

    [[nodiscard]] std::expected<std::uint16_t, CodecError> read_u16() {
        if (offset_ + 2 > bytes_.size()) {
            return std::unexpected(error("truncated", "Unexpected end of input"));
        }
        std::uint16_t value = 0;
        for (int byte = 0; byte < 2; ++byte) {
            value |= static_cast<std::uint16_t>(bytes_[offset_++]) << (byte * 8);
        }
        return value;
    }

    [[nodiscard]] std::expected<std::uint32_t, CodecError> read_u32() {
        if (offset_ + 4 > bytes_.size()) {
            return std::unexpected(error("truncated", "Unexpected end of input"));
        }
        std::uint32_t value = 0;
        for (int byte = 0; byte < 4; ++byte) {
            value |= static_cast<std::uint32_t>(bytes_[offset_++]) << (byte * 8);
        }
        return value;
    }

    [[nodiscard]] std::expected<std::uint64_t, CodecError> read_u64() {
        if (offset_ + 8 > bytes_.size()) {
            return std::unexpected(error("truncated", "Unexpected end of input"));
        }
        std::uint64_t value = 0;
        for (int byte = 0; byte < 8; ++byte) {
            value |= static_cast<std::uint64_t>(bytes_[offset_++]) << (byte * 8);
        }
        return value;
    }

    [[nodiscard]] std::expected<float, CodecError> read_float() {
        auto bits = read_u32();
        if (!bits) {
            return std::unexpected(bits.error());
        }
        const float value = std::bit_cast<float>(*bits);
        if (!std::isfinite(value)) {
            return std::unexpected(error("non_finite_float", "Decoded non-finite float"));
        }
        return value;
    }

    [[nodiscard]] std::expected<std::string, CodecError> read_string() {
        auto size = read_u32();
        if (!size) {
            return std::unexpected(size.error());
        }
        if (*size > limits_.max_string_bytes) {
            return std::unexpected(error("string_too_large", "Decoded string exceeds codec limit"));
        }
        if (offset_ + *size > bytes_.size()) {
            return std::unexpected(error("truncated", "Unexpected end of input"));
        }
        std::string value(reinterpret_cast<const char*>(bytes_.data() + offset_), *size);
        offset_ += *size;
        return value;
    }

    [[nodiscard]] std::expected<std::uint32_t, CodecError> read_count() {
        auto count = read_u32();
        if (!count) {
            return std::unexpected(count.error());
        }
        if (*count > limits_.max_vector_count) {
            return std::unexpected(error("vector_too_large", "Decoded vector exceeds codec limit"));
        }
        return *count;
    }

private:
    const std::vector<std::uint8_t>& bytes_;
    CodecLimits limits_{};
    std::size_t offset_ = 0;
};

#define TRY_VOID(expr)      \
    do {                    \
        auto _result = expr; \
        if (!_result) {     \
            return std::unexpected(_result.error()); \
        }                   \
    } while (false)

#define TRY_VALUE(name, expr) \
    auto name##_result = expr; \
    if (!name##_result) {     \
        return std::unexpected(name##_result.error()); \
    }                         \
    auto name = *name##_result

template <std::size_t Count>
[[nodiscard]] std::expected<void, CodecError> write_float_array(Writer& writer,
                                                                const std::array<float, Count>& values) {
    for (float value : values) {
        TRY_VOID(writer.write_float(value));
    }
    return {};
}

template <std::size_t Count>
[[nodiscard]] std::expected<std::array<float, Count>, CodecError> read_float_array(Reader& reader) {
    std::array<float, Count> values{};
    for (float& value : values) {
        TRY_VALUE(decoded, reader.read_float());
        value = decoded;
    }
    return values;
}

[[nodiscard]] bool valid_entity_kind(std::uint8_t value) noexcept {
    return value <= static_cast<std::uint8_t>(stellar::server::EntityKind::kObjectCollider);
}

[[nodiscard]] bool valid_event_kind(std::uint8_t value) noexcept {
    return value >= static_cast<std::uint8_t>(GameplayEventKind::kPickupCollected) &&
           value <= static_cast<std::uint8_t>(GameplayEventKind::kScriptError);
}

[[nodiscard]] std::expected<void, CodecError> write_transform(
    Writer& writer,
    const stellar::server::TransformComponent& transform) {
    TRY_VOID(write_float_array(writer, transform.position));
    TRY_VOID(write_float_array(writer, transform.rotation));
    TRY_VOID(write_float_array(writer, transform.scale));
    return {};
}

[[nodiscard]] std::expected<stellar::server::TransformComponent, CodecError> read_transform(
    Reader& reader) {
    stellar::server::TransformComponent transform{};
    TRY_VALUE(position, read_float_array<3>(reader));
    TRY_VALUE(rotation, read_float_array<4>(reader));
    TRY_VALUE(scale, read_float_array<3>(reader));
    transform.position = position;
    transform.rotation = rotation;
    transform.scale = scale;
    return transform;
}

[[nodiscard]] std::expected<void, CodecError> write_metadata(
    Writer& writer,
    const stellar::server::GameplayEntityMetadata& metadata) {
    TRY_VOID(writer.write_string(metadata.name));
    TRY_VOID(writer.write_string(metadata.archetype));
    TRY_VOID(writer.write_string(metadata.sprite_id));
    TRY_VOID(writer.write_string(metadata.source_type));
    TRY_VOID(writer.write_string(metadata.extras_json));
    TRY_VOID(write_float_array(writer, metadata.size));
    TRY_VOID(writer.write_float(metadata.alpha));
    TRY_VOID(writer.write_u32(metadata.object_collider_id));
    TRY_VOID(writer.write_u32(metadata.collision_mesh_index));
    return {};
}

[[nodiscard]] std::expected<stellar::server::GameplayEntityMetadata, CodecError> read_metadata(
    Reader& reader) {
    stellar::server::GameplayEntityMetadata metadata{};
    TRY_VALUE(name, reader.read_string());
    TRY_VALUE(archetype, reader.read_string());
    TRY_VALUE(sprite_id, reader.read_string());
    TRY_VALUE(source_type, reader.read_string());
    TRY_VALUE(extras_json, reader.read_string());
    TRY_VALUE(size, read_float_array<3>(reader));
    TRY_VALUE(alpha, reader.read_float());
    TRY_VALUE(object_collider_id, reader.read_u32());
    TRY_VALUE(collision_mesh_index, reader.read_u32());
    metadata.name = std::move(name);
    metadata.archetype = std::move(archetype);
    metadata.sprite_id = std::move(sprite_id);
    metadata.source_type = std::move(source_type);
    metadata.extras_json = std::move(extras_json);
    metadata.size = size;
    metadata.alpha = alpha;
    metadata.object_collider_id = object_collider_id;
    metadata.collision_mesh_index = collision_mesh_index;
    return metadata;
}

[[nodiscard]] std::expected<void, CodecError> write_player(
    Writer& writer,
    const stellar::server::PlayerSnapshot& player) {
    TRY_VOID(writer.write_u32(player.player_id));
    TRY_VOID(write_float_array(writer, player.position));
    TRY_VOID(write_float_array(writer, player.velocity));
    TRY_VOID(write_float_array(writer, player.rotation));
    TRY_VOID(writer.write_bool(player.grounded));
    return {};
}

[[nodiscard]] std::expected<stellar::server::PlayerSnapshot, CodecError> read_player(Reader& reader) {
    stellar::server::PlayerSnapshot player{};
    TRY_VALUE(player_id, reader.read_u32());
    TRY_VALUE(position, read_float_array<3>(reader));
    TRY_VALUE(velocity, read_float_array<3>(reader));
    TRY_VALUE(rotation, read_float_array<4>(reader));
    TRY_VALUE(grounded, reader.read_bool());
    player.player_id = player_id;
    player.position = position;
    player.velocity = velocity;
    player.rotation = rotation;
    player.grounded = grounded;
    return player;
}

[[nodiscard]] std::expected<void, CodecError> write_movement_command(
    Writer& writer,
    const stellar::server::MovementCommand& movement) {
    TRY_VOID(write_float_array(writer, movement.wish_direction));
    TRY_VOID(writer.write_bool(movement.jump));
    return {};
}

[[nodiscard]] std::expected<stellar::server::MovementCommand, CodecError> read_movement_command(
    Reader& reader) {
    stellar::server::MovementCommand movement{};
    TRY_VALUE(wish_direction, read_float_array<3>(reader));
    TRY_VALUE(jump, reader.read_bool());
    movement.wish_direction = wish_direction;
    movement.jump = jump;
    return movement;
}

[[nodiscard]] std::expected<void, CodecError> write_command_body(
    Writer& writer,
    const NetworkPlayerCommand& command) {
    TRY_VOID(writer.write_u32(command.player_id));
    TRY_VOID(writer.write_u64(command.command_sequence));
    TRY_VOID(write_movement_command(writer, command.movement));
    return {};
}

[[nodiscard]] std::expected<NetworkPlayerCommand, CodecError> read_command_body(Reader& reader) {
    NetworkPlayerCommand command{};
    TRY_VALUE(player_id, reader.read_u32());
    TRY_VALUE(command_sequence, reader.read_u64());
    TRY_VALUE(movement, read_movement_command(reader));
    command.player_id = player_id;
    command.command_sequence = command_sequence;
    command.movement = movement;
    return command;
}

[[nodiscard]] std::expected<void, CodecError> write_entity(Writer& writer,
                                                           const NetworkGameplayEntity& entity) {
    TRY_VOID(writer.write_u32(entity.id));
    TRY_VOID(writer.write_u8(static_cast<std::uint8_t>(entity.kind)));
    TRY_VOID(write_transform(writer, entity.transform));
    TRY_VOID(write_metadata(writer, entity.metadata));
    TRY_VOID(writer.write_bool(entity.active));
    TRY_VOID(writer.write_bool(entity.open));
    return {};
}

[[nodiscard]] std::expected<NetworkGameplayEntity, CodecError> read_entity(Reader& reader) {
    NetworkGameplayEntity entity{};
    TRY_VALUE(id, reader.read_u32());
    TRY_VALUE(kind, reader.read_u8());
    if (!valid_entity_kind(kind)) {
        return std::unexpected(error("invalid_entity_kind", "Decoded entity kind is invalid"));
    }
    TRY_VALUE(transform, read_transform(reader));
    TRY_VALUE(metadata, read_metadata(reader));
    TRY_VALUE(active, reader.read_bool());
    TRY_VALUE(open, reader.read_bool());
    entity.id = id;
    entity.kind = static_cast<stellar::server::EntityKind>(kind);
    entity.transform = transform;
    entity.metadata = std::move(metadata);
    entity.active = active;
    entity.open = open;
    return entity;
}

[[nodiscard]] std::expected<void, CodecError> write_event(Writer& writer, const GameplayEvent& event) {
    TRY_VOID(writer.write_u8(static_cast<std::uint8_t>(event.kind)));
    TRY_VOID(writer.write_u64(event.tick));
    TRY_VOID(writer.write_u32(event.entity_id));
    TRY_VOID(writer.write_u32(event.player_id));
    TRY_VOID(writer.write_string(event.code));
    TRY_VOID(writer.write_string(event.message));
    return {};
}

[[nodiscard]] std::expected<GameplayEvent, CodecError> read_event(Reader& reader) {
    GameplayEvent event{};
    TRY_VALUE(kind, reader.read_u8());
    if (!valid_event_kind(kind)) {
        return std::unexpected(error("invalid_event_kind", "Decoded event kind is invalid"));
    }
    TRY_VALUE(tick, reader.read_u64());
    TRY_VALUE(entity_id, reader.read_u32());
    TRY_VALUE(player_id, reader.read_u32());
    TRY_VALUE(code, reader.read_string());
    TRY_VALUE(message, reader.read_string());
    event.kind = static_cast<GameplayEventKind>(kind);
    event.tick = tick;
    event.entity_id = entity_id;
    event.player_id = player_id;
    event.code = std::move(code);
    event.message = std::move(message);
    return event;
}

[[nodiscard]] std::expected<void, CodecError> write_snapshot_body(
    Writer& writer,
    const NetworkWorldSnapshot& snapshot) {
    TRY_VOID(writer.write_u64(snapshot.tick));
    TRY_VOID(writer.write_count(snapshot.players.size()));
    for (const stellar::server::PlayerSnapshot& player : snapshot.players) {
        TRY_VOID(write_player(writer, player));
    }
    TRY_VOID(writer.write_count(snapshot.entities.size()));
    for (const NetworkGameplayEntity& entity : snapshot.entities) {
        TRY_VOID(write_entity(writer, entity));
    }
    return {};
}

[[nodiscard]] std::expected<NetworkWorldSnapshot, CodecError> read_snapshot_body(Reader& reader) {
    NetworkWorldSnapshot snapshot{};
    TRY_VALUE(tick, reader.read_u64());
    TRY_VALUE(player_count, reader.read_count());
    snapshot.tick = tick;
    snapshot.players.reserve(player_count);
    for (std::uint32_t index = 0; index < player_count; ++index) {
        TRY_VALUE(player, read_player(reader));
        snapshot.players.push_back(player);
    }
    TRY_VALUE(entity_count, reader.read_count());
    snapshot.entities.reserve(entity_count);
    for (std::uint32_t index = 0; index < entity_count; ++index) {
        TRY_VALUE(entity, read_entity(reader));
        snapshot.entities.push_back(std::move(entity));
    }
    return snapshot;
}

[[nodiscard]] std::expected<void, CodecError> write_delta_body(Writer& writer,
                                                               const SnapshotDelta& delta) {
    TRY_VOID(writer.write_u64(delta.baseline_tick));
    TRY_VOID(writer.write_u64(delta.target_tick));
    TRY_VOID(writer.write_count(delta.added_entities.size()));
    for (const NetworkGameplayEntity& entity : delta.added_entities) {
        TRY_VOID(write_entity(writer, entity));
    }
    TRY_VOID(writer.write_count(delta.updated_entities.size()));
    for (const NetworkGameplayEntity& entity : delta.updated_entities) {
        TRY_VOID(write_entity(writer, entity));
    }
    TRY_VOID(writer.write_count(delta.removed_entity_ids.size()));
    for (stellar::server::EntityId id : delta.removed_entity_ids) {
        TRY_VOID(writer.write_u32(id));
    }
    TRY_VOID(writer.write_count(delta.updated_players.size()));
    for (const stellar::server::PlayerSnapshot& player : delta.updated_players) {
        TRY_VOID(write_player(writer, player));
    }
    TRY_VOID(writer.write_count(delta.removed_player_ids.size()));
    for (stellar::server::PlayerId id : delta.removed_player_ids) {
        TRY_VOID(writer.write_u32(id));
    }
    return {};
}

[[nodiscard]] std::expected<SnapshotDelta, CodecError> read_delta_body(Reader& reader) {
    SnapshotDelta delta{};
    TRY_VALUE(baseline_tick, reader.read_u64());
    TRY_VALUE(target_tick, reader.read_u64());
    delta.baseline_tick = baseline_tick;
    delta.target_tick = target_tick;
    TRY_VALUE(added_count, reader.read_count());
    delta.added_entities.reserve(added_count);
    for (std::uint32_t index = 0; index < added_count; ++index) {
        TRY_VALUE(entity, read_entity(reader));
        delta.added_entities.push_back(std::move(entity));
    }
    TRY_VALUE(updated_count, reader.read_count());
    delta.updated_entities.reserve(updated_count);
    for (std::uint32_t index = 0; index < updated_count; ++index) {
        TRY_VALUE(entity, read_entity(reader));
        delta.updated_entities.push_back(std::move(entity));
    }
    TRY_VALUE(removed_count, reader.read_count());
    delta.removed_entity_ids.reserve(removed_count);
    for (std::uint32_t index = 0; index < removed_count; ++index) {
        TRY_VALUE(id, reader.read_u32());
        delta.removed_entity_ids.push_back(id);
    }
    TRY_VALUE(player_count, reader.read_count());
    delta.updated_players.reserve(player_count);
    for (std::uint32_t index = 0; index < player_count; ++index) {
        TRY_VALUE(player, read_player(reader));
        delta.updated_players.push_back(player);
    }
    TRY_VALUE(removed_player_count, reader.read_count());
    delta.removed_player_ids.reserve(removed_player_count);
    for (std::uint32_t index = 0; index < removed_player_count; ++index) {
        TRY_VALUE(id, reader.read_u32());
        delta.removed_player_ids.push_back(id);
    }
    return delta;
}

[[nodiscard]] std::expected<void, CodecError> write_header(Writer& writer, std::uint32_t magic) {
    TRY_VOID(writer.write_u32(magic));
    TRY_VOID(writer.write_u16(kCodecVersion));
    return {};
}

[[nodiscard]] std::expected<void, CodecError> read_header(Reader& reader, std::uint32_t magic) {
    TRY_VALUE(decoded_magic, reader.read_u32());
    if (decoded_magic != magic) {
        return std::unexpected(error("invalid_magic", "Decoded message magic is invalid"));
    }
    TRY_VALUE(version, reader.read_u16());
    if (version != kCodecVersion) {
        return std::unexpected(error("unsupported_version", "Decoded codec version is unsupported"));
    }
    return {};
}

[[nodiscard]] std::expected<void, CodecError> require_finished(const Reader& reader) {
    if (!reader.finished()) {
        return std::unexpected(error("trailing_bytes", "Decoded message has trailing bytes"));
    }
    return {};
}

#undef TRY_VALUE
#undef TRY_VOID

} // namespace

std::expected<std::vector<std::uint8_t>, CodecError> encode_player_command(
    const NetworkPlayerCommand& command,
    CodecLimits limits) {
    Writer writer{limits};
#define TRY_ENCODE(expr)    \
    do {                    \
        auto _result = expr; \
        if (!_result) {     \
            return std::unexpected(_result.error()); \
        }                   \
    } while (false)
    TRY_ENCODE(write_header(writer, kCommandMagic));
    TRY_ENCODE(write_command_body(writer, command));
    return writer.take_bytes();
#undef TRY_ENCODE
}

std::expected<NetworkPlayerCommand, CodecError> decode_player_command(
    const std::vector<std::uint8_t>& bytes,
    CodecLimits limits) {
    Reader reader{bytes, limits};
#define TRY_DECODE_VOID(expr) \
    do {                      \
        auto _result = expr;   \
        if (!_result) {       \
            return std::unexpected(_result.error()); \
        }                     \
    } while (false)
    TRY_DECODE_VOID(read_header(reader, kCommandMagic));
    auto command = read_command_body(reader);
    if (!command) {
        return std::unexpected(command.error());
    }
    TRY_DECODE_VOID(require_finished(reader));
    return *command;
#undef TRY_DECODE_VOID
}

std::expected<std::vector<std::uint8_t>, CodecError> encode_snapshot(
    const NetworkWorldSnapshot& snapshot,
    CodecLimits limits) {
    Writer writer{limits};
#define TRY_ENCODE(expr)    \
    do {                    \
        auto _result = expr; \
        if (!_result) {     \
            return std::unexpected(_result.error()); \
        }                   \
    } while (false)
    TRY_ENCODE(write_header(writer, kSnapshotMagic));
    TRY_ENCODE(write_snapshot_body(writer, snapshot));
    return writer.take_bytes();
#undef TRY_ENCODE
}

std::expected<NetworkWorldSnapshot, CodecError> decode_snapshot(const std::vector<std::uint8_t>& bytes,
                                                                CodecLimits limits) {
    Reader reader{bytes, limits};
#define TRY_DECODE_VOID(expr) \
    do {                      \
        auto _result = expr;   \
        if (!_result) {       \
            return std::unexpected(_result.error()); \
        }                     \
    } while (false)
    TRY_DECODE_VOID(read_header(reader, kSnapshotMagic));
    auto snapshot = read_snapshot_body(reader);
    if (!snapshot) {
        return std::unexpected(snapshot.error());
    }
    TRY_DECODE_VOID(require_finished(reader));
    return *snapshot;
#undef TRY_DECODE_VOID
}

std::expected<std::vector<std::uint8_t>, CodecError> encode_gameplay_event(const GameplayEvent& event,
                                                                           CodecLimits limits) {
    Writer writer{limits};
#define TRY_ENCODE(expr)    \
    do {                    \
        auto _result = expr; \
        if (!_result) {     \
            return std::unexpected(_result.error()); \
        }                   \
    } while (false)
    TRY_ENCODE(write_header(writer, kEventMagic));
    TRY_ENCODE(write_event(writer, event));
    return writer.take_bytes();
#undef TRY_ENCODE
}

std::expected<GameplayEvent, CodecError> decode_gameplay_event(const std::vector<std::uint8_t>& bytes,
                                                               CodecLimits limits) {
    Reader reader{bytes, limits};
#define TRY_DECODE_VOID(expr) \
    do {                      \
        auto _result = expr;   \
        if (!_result) {       \
            return std::unexpected(_result.error()); \
        }                     \
    } while (false)
    TRY_DECODE_VOID(read_header(reader, kEventMagic));
    auto event = read_event(reader);
    if (!event) {
        return std::unexpected(event.error());
    }
    TRY_DECODE_VOID(require_finished(reader));
    return *event;
#undef TRY_DECODE_VOID
}

std::expected<std::vector<std::uint8_t>, CodecError> encode_snapshot_delta(const SnapshotDelta& delta,
                                                                           CodecLimits limits) {
    Writer writer{limits};
#define TRY_ENCODE(expr)    \
    do {                    \
        auto _result = expr; \
        if (!_result) {     \
            return std::unexpected(_result.error()); \
        }                   \
    } while (false)
    TRY_ENCODE(write_header(writer, kDeltaMagic));
    TRY_ENCODE(write_delta_body(writer, delta));
    return writer.take_bytes();
#undef TRY_ENCODE
}

std::expected<SnapshotDelta, CodecError> decode_snapshot_delta(const std::vector<std::uint8_t>& bytes,
                                                               CodecLimits limits) {
    Reader reader{bytes, limits};
#define TRY_DECODE_VOID(expr) \
    do {                      \
        auto _result = expr;   \
        if (!_result) {       \
            return std::unexpected(_result.error()); \
        }                     \
    } while (false)
    TRY_DECODE_VOID(read_header(reader, kDeltaMagic));
    auto delta = read_delta_body(reader);
    if (!delta) {
        return std::unexpected(delta.error());
    }
    TRY_DECODE_VOID(require_finished(reader));
    return *delta;
#undef TRY_DECODE_VOID
}

} // namespace stellar::network
