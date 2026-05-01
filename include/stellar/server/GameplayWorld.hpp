#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::server {

/** @brief Stable identifier for a local authoritative player slot. */
using PlayerId = std::uint32_t;

/** @brief Stable server-owned gameplay entity identifier allocated deterministically. */
using EntityId = std::uint32_t;

/** @brief Minimal server-owned gameplay entity category. */
enum class EntityKind {
    kPlayer,
    kSprite,
    kPickup,
    kDoor,
    kTrigger,
    kObjectCollider,
};

/** @brief Plain transform component copied from authored metadata or collision bounds. */
struct TransformComponent {
    /** @brief World-space entity origin or center. */
    std::array<float, 3> position{0.0F, 0.0F, 0.0F};

    /** @brief World-space orientation as x, y, z, w quaternion components. */
    std::array<float, 4> rotation{0.0F, 0.0F, 0.0F, 1.0F};

    /** @brief Authored scale or world-space size depending on marker source. */
    std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
};

/** @brief Serializable marker/collision metadata preserved on spawned gameplay entities. */
struct GameplayEntityMetadata {
    /** @brief Authored marker, collider, mesh, or entity name. */
    std::string name;

    /** @brief Authored gameplay archetype label. */
    std::string archetype;

    /** @brief Optional sprite/material identifier for billboard presentation binding. */
    std::string sprite_id;

    /** @brief Optional source marker type name for diagnostics and deterministic tests. */
    std::string source_type;

    /** @brief Optional source property payload retained as inert data; scripts are not executed. */
    std::string extras_json;

    /** @brief Authored display size or collider half extent when available. */
    std::array<float, 3> size{1.0F, 1.0F, 1.0F};

    /** @brief Authored alpha value for presentation binding; clamped later by presentation code. */
    float alpha = 1.0F;

    /** @brief Runtime object collider id when this entity came from an object-collider marker. */
    std::uint32_t object_collider_id = 0;

    /** @brief Collision mesh index when this entity came from named collision metadata. */
    std::uint32_t collision_mesh_index = 0;
};

/** @brief Minimal server-owned gameplay entity record with plain data components. */
struct GameplayEntity {
    /** @brief Stable deterministic entity identifier. */
    EntityId id = 0;

    /** @brief Server-owned entity category. */
    EntityKind kind = EntityKind::kSprite;

    /** @brief Plain transform component. */
    TransformComponent transform{};

    /** @brief Marker/collision metadata component. */
    GameplayEntityMetadata metadata{};
};

/** @brief Stable local player to entity binding emitted by server-owned gameplay world. */
struct PlayerEntityBinding {
    /** @brief Authoritative player slot identifier. */
    PlayerId player_id = 0;

    /** @brief Entity id representing this player. */
    EntityId entity_id = 0;
};

/** @brief Display-free snapshot of the minimal server-owned gameplay entity world. */
struct GameplayWorldSnapshot {
    /** @brief Deterministically ordered entity records. */
    std::vector<GameplayEntity> entities;

    /** @brief Deterministic player slot to entity bindings. */
    std::vector<PlayerEntityBinding> player_bindings;
};

/** @brief Minimal server-owned gameplay entity world spawned from runtime metadata. */
class GameplayWorld {
public:
    /** @brief Clear current entities and spawn deterministic records from runtime world metadata. */
    void reset_from_world(const stellar::world::RuntimeWorld& world, PlayerId local_player_id);

    /** @brief Return immutable entities in deterministic allocation order. */
    [[nodiscard]] std::span<const GameplayEntity> entities() const noexcept;

    /** @brief Return immutable player/entity bindings in deterministic allocation order. */
    [[nodiscard]] std::span<const PlayerEntityBinding> player_bindings() const noexcept;

    /** @brief Return the entity bound to a player slot, when one was spawned. */
    [[nodiscard]] std::optional<EntityId> entity_for_player(PlayerId player_id) const noexcept;

    /** @brief Return a display-free serializable entity snapshot. */
    [[nodiscard]] GameplayWorldSnapshot snapshot() const;

private:
    [[nodiscard]] EntityId allocate_entity_id() noexcept;

    std::vector<GameplayEntity> entities_;
    std::vector<PlayerEntityBinding> player_bindings_;
    EntityId next_entity_id_ = 1;
};

/** @brief Build a minimal server-owned gameplay world from runtime world metadata. */
[[nodiscard]] GameplayWorld build_gameplay_world(const stellar::world::RuntimeWorld& world,
                                                 PlayerId local_player_id);

/** @brief Return an immutable entity with a matching id, or nullptr when absent. */
[[nodiscard]] const GameplayEntity* find_entity(std::span<const GameplayEntity> entities,
                                                EntityId id) noexcept;

} // namespace stellar::server
