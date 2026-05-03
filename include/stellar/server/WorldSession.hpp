#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "stellar/server/GameplayWorld.hpp"
#include "stellar/server/MovementSimulation.hpp"
#include "stellar/server/MovementTriggerIntegration.hpp"
#include "stellar/world/ObjectCollider.hpp"
#include "stellar/world/RuntimeCollisionState.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::server {

/** @brief Stable identifier for a local authoritative player slot. */
using PlayerId = std::uint32_t;

/** @brief Server-owned command for one simulation tick. */
struct PlayerCommand {
    /** @brief Player slot this command targets. */
    PlayerId player_id = 0;

    /** @brief Movement intent for this simulation tick. */
    MovementCommand movement{};
};

/** @brief Authoritative player state exposed in snapshots. */
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

/** @brief Authoritative object-collider overlap transition for one player and collider. */
struct ObjectColliderEvent {
    /** @brief Stable authoritative player slot identifier. */
    PlayerId player_id = 0;

    /** @brief Stable runtime object collider identifier. */
    std::uint32_t collider_id = 0;

    /** @brief Human-readable collider name copied from authoritative collider state. */
    std::string name;

    /** @brief Optional gameplay archetype copied from authoritative collider state. */
    std::string archetype;

    /** @brief True only on the first update where the player overlaps this collider. */
    bool entered = false;

    /** @brief True on updates after enter while overlap remains current. */
    bool stayed = false;

    /** @brief True only on the first update where overlap is no longer current. */
    bool exited = false;
};

/**
 * @brief Result for an authoritative object-collider mutation.
 *
 * Mutation-generated exit events are returned synchronously here and are not replayed by
 * snapshot(); tick snapshots contain only events produced by that tick's overlap update.
 */
struct ObjectColliderMutationResult {
    /** @brief True when the requested mutation changed or confirmed server state. */
    bool applied = false;

    /** @brief Stable machine-readable result code. */
    std::string code;

    /** @brief Human-readable mutation result message for logs and tests. */
    std::string message;

    /** @brief Synchronous exit events emitted by this mutation, if any. */
    std::vector<ObjectColliderEvent> object_collider_events;
};

/** @brief Result for authoritative pickup collection from object-collider interaction. */
struct PickupCollectionResult {
    /** @brief True when an active pickup was collected and state changed. */
    bool applied = false;

    /** @brief Stable machine-readable result code. */
    std::string code;

    /** @brief Human-readable collection result message for logs and tests. */
    std::string message;

    /** @brief Synchronous collider exit events emitted while disabling the pickup collider. */
    std::vector<ObjectColliderEvent> object_collider_events;
};

/** @brief Authoritative runtime state for a transformable brush model. */
enum class BrushMoverPhase : std::uint8_t {
    kClosed,
    kOpening,
    kOpen,
    kClosing,
    kPressed,
};

/** @brief Server-owned moving brush snapshot replicated to clients as gameplay entities. */
struct BrushMoverSnapshot {
    /** @brief Stable authoritative entity id associated with this mover. */
    EntityId entity_id = 0;

    /** @brief Authored targetname or generated name. */
    std::string name;

    /** @brief Authored class such as func_door or func_button. */
    std::string classname;

    /** @brief Current server-owned translation relative to imported closed position. */
    std::array<float, 3> translation{0.0F, 0.0F, 0.0F};

    /** @brief True when the mover has reached its activated/open extent. */
    bool open = false;
};

/** @brief Snapshot emitted by the authoritative world session after a tick. */
struct WorldSnapshot {
    /** @brief Number of completed authoritative simulation ticks. */
    std::uint64_t tick = 0;

    /** @brief Authoritative player snapshots. */
    std::vector<PlayerSnapshot> players;

    /** @brief Trigger events produced by the tick that emitted this snapshot. */
    std::vector<MovementTriggerEvent> trigger_events;

    /** @brief Object-collider events produced by the tick that emitted this snapshot. */
    std::vector<ObjectColliderEvent> object_collider_events;

    /** @brief Display-free server-owned gameplay entity snapshot. */
    GameplayWorldSnapshot gameplay_world;

    /** @brief Server-owned moving brush transforms visible to presentation-only clients. */
    std::vector<BrushMoverSnapshot> brush_movers;
};

/** @brief Deterministic config for a local authoritative world session. */
struct WorldSessionConfig {
    /** @brief Deterministic movement settings used by each authoritative tick. */
    MovementSimulationConfig movement{};

    /** @brief Single local player slot owned by this phase's session implementation. */
    PlayerId local_player_id = 1;
};

/**
 * @brief Backend-neutral authoritative session over one runtime world.
 *
 * The session stores a pointer to caller-owned RuntimeWorld data. The caller must keep that
 * RuntimeWorld, and the LevelAsset backing it, alive for the lifetime of the session or until the
 * next reset call supplies a replacement world.
 */
class WorldSession {
public:
    /** @brief Construct a session and initialize player state from the runtime world's spawn data. */
    explicit WorldSession(const stellar::world::RuntimeWorld& world,
                          WorldSessionConfig config = {});

    /** @brief Replace the runtime world, config, movement state, tick index, and trigger tracker. */
    void reset(const stellar::world::RuntimeWorld& world, WorldSessionConfig config = {});

    /** @brief Return current authoritative state without ticking or replaying trigger events. */
    [[nodiscard]] WorldSnapshot snapshot() const;

    /** @brief Return current server-owned gameplay entity world without presentation handles. */
    [[nodiscard]] GameplayWorldSnapshot gameplay_snapshot() const;

    /** @brief Return the entity id bound to a local player slot, if that player was spawned. */
    [[nodiscard]] std::optional<EntityId> entity_for_player(PlayerId player_id) const noexcept;

    /** @brief Advance one authoritative tick using the local player's command if present. */
    [[nodiscard]] WorldSnapshot tick(std::span<const PlayerCommand> commands) noexcept;

    /**
     * @brief Authoritatively enable or disable named static collision meshes for future ticks.
     *
     * The immutable RuntimeWorld and imported assets are not modified. Duplicate authored names are
     * toggled together by RuntimeCollisionState in deterministic mesh-index order.
     */
    [[nodiscard]] stellar::world::RuntimeCollisionStateResult set_collision_mesh_enabled(
        std::string_view name,
        bool enabled) noexcept;

    /** @brief Hard-reset authoritative object colliders and clear all object overlap state. */
    void set_object_colliders(std::span<const stellar::world::ObjectCollider> colliders);

    /** @brief Replace colliders by id and synchronously return any removed/disabled exits. */
    [[nodiscard]] std::vector<ObjectColliderEvent> replace_object_colliders_preserving_overlaps(
        std::span<const stellar::world::ObjectCollider> colliders) noexcept;

    /** @brief Enable or disable one collider and synchronously return mutation exits only. */
    [[nodiscard]] ObjectColliderMutationResult set_object_collider_enabled(
        std::uint32_t collider_id, bool enabled) noexcept;

    /** @brief Collect an active pickup by object-collider id and disable its server collider. */
    [[nodiscard]] PickupCollectionResult collect_pickup(std::uint32_t collider_id) noexcept;

    /** @brief Fire an authored targetname through the server-owned target router. */
    [[nodiscard]] bool fire_target(std::string_view targetname) noexcept;

    /** @brief Return deterministic target/mover diagnostics accumulated by this session. */
    [[nodiscard]] std::span<const std::string> target_diagnostics() const noexcept;

    /** @brief Insert or replace one collider and synchronously return mutation exits only. */
    [[nodiscard]] ObjectColliderMutationResult upsert_object_collider(
        const stellar::world::ObjectCollider& collider) noexcept;

    /** @brief Remove one collider and synchronously return mutation exits only. */
    [[nodiscard]] ObjectColliderMutationResult remove_object_collider(
        std::uint32_t collider_id) noexcept;

    /** @brief Return the number of completed authoritative simulation ticks. */
    [[nodiscard]] std::uint64_t tick_index() const noexcept;

private:
    [[nodiscard]] WorldSnapshot make_snapshot(
        std::vector<MovementTriggerEvent> trigger_events = {},
        std::vector<ObjectColliderEvent> object_collider_events = {}) const;
    [[nodiscard]] MovementCommand select_local_command(
        std::span<const PlayerCommand> commands) const noexcept;
    void apply_view_angles(const MovementCommand& command) noexcept;

    const stellar::world::RuntimeWorld* world_ = nullptr;
    WorldSessionConfig config_{};
    MovementState player_state_{};
    float player_yaw_degrees_ = 0.0F;
    float player_pitch_degrees_ = 0.0F;
    GameplayWorld gameplay_world_{};
    MovementTriggerTracker trigger_tracker_{};
    stellar::world::ObjectColliderSystem object_collider_system_{};
    stellar::world::RuntimeCollisionState collision_state_{};
    struct BrushMover {
        EntityId entity_id = 0;
        std::uint32_t collision_mesh_index = 0;
        std::string name;
        std::string classname;
        std::string target;
        std::array<float, 3> direction{0.0F, 1.0F, 0.0F};
        float distance = 0.0F;
        float speed = 100.0F;
        float wait = 1.0F;
        float delay = 0.0F;
        float progress = 0.0F;
        float wait_remaining = 0.0F;
        BrushMoverPhase phase = BrushMoverPhase::kClosed;
    };
    struct ScheduledTargetFire {
        std::string targetname;
        float remaining_seconds = 0.0F;
    };
    std::vector<BrushMover> brush_movers_;
    std::vector<ScheduledTargetFire> scheduled_target_fires_;
    std::vector<std::string> target_diagnostics_;
    std::uint64_t tick_index_ = 0;
};

} // namespace stellar::server
