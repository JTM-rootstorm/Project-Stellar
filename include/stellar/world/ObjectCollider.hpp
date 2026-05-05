#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "stellar/assets/WorldMetadataAsset.hpp"
#include "stellar/world/SensorOverlapTracker.hpp"
#include "stellar/world/TriggerSystem.hpp"

namespace stellar::world {

struct RuntimeWorld;

/** @brief Backend-neutral runtime collider shape kind for kinematic objects. */
enum class ObjectColliderShapeType {
    /** @brief Spherical overlap volume. */
    kSphere,
    /** @brief Axis-aligned box overlap volume. */
    kAabb,
    /** @brief Vertical capsule overlap volume. */
    kCapsule,
};

/** @brief Backend-neutral runtime collider shape data for kinematic objects. */
struct ObjectColliderShape {
    /** @brief Primitive kind used by this shape. */
    ObjectColliderShapeType type = ObjectColliderShapeType::kSphere;

    /** @brief World-space shape center. */
    std::array<float, 3> center{};

    /** @brief Positive AABB half extents; non-finite values sanitize to zero. */
    std::array<float, 3> half_extents{0.5F, 0.5F, 0.5F};

    /** @brief Sphere or capsule radius; negative or non-finite values sanitize to zero. */
    float radius = 0.5F;

    /** @brief Capsule total height including hemispherical ends. */
    float height = 1.0F;
};

/** @brief Stable object collider owned by runtime/server code. */
struct ObjectCollider {
    /** @brief Stable caller-owned collider identifier used for overlap state reporting. */
    std::uint32_t id = 0;

    /** @brief Human-readable collider name; duplicate names are allowed. */
    std::string name;

    /** @brief Optional gameplay archetype label carried as plain data. */
    std::string archetype;

    /** @brief Backend-neutral primitive shape. */
    ObjectColliderShape shape{};

    /** @brief Disabled colliders do not produce current overlaps. */
    bool enabled = true;
};

/** @brief Deterministic overlap event between the player capsule and one object collider. */
struct ObjectColliderOverlapEvent {
    /** @brief Stable identifier copied from the collider that produced the event. */
    std::uint32_t collider_id = 0;

    /** @brief Collider name copied for diagnostics and script-friendly routing later. */
    std::string name;

    /** @brief True only on the first update where the player overlaps this collider. */
    bool entered = false;

    /** @brief True on updates after enter while overlap remains current. */
    bool stayed = false;

    /** @brief True only on the first update where overlap is no longer current. */
    bool exited = false;
};

/** @brief Severity for deterministic object-collider diagnostics. */
enum class ObjectColliderDiagnosticSeverity {
    /** @brief Non-fatal object-collider authoring or runtime issue. */
    kWarning,
    /** @brief Fatal object-collider issue that should block authoritative use. */
    kError,
};

/** @brief Deterministic diagnostic produced for object-collider authoring/runtime issues. */
struct ObjectColliderDiagnostic {
    /** @brief Machine-readable severity for this diagnostic. */
    ObjectColliderDiagnosticSeverity severity = ObjectColliderDiagnosticSeverity::kWarning;

    /** @brief Stable machine-readable diagnostic code. */
    std::string code;

    /** @brief Human-readable diagnostic message for logs and tests. */
    std::string message;

    /** @brief Collider id associated with this diagnostic, when available. */
    std::uint32_t collider_id = 0;

    /** @brief Collider index in the current deterministic collider order. */
    std::size_t collider_index = 0;
};

/** @brief Result for a live object-collider mutation. */
struct ObjectColliderMutationResult {
    /** @brief True when the requested mutation changed or confirmed system state. */
    bool applied = false;

    /** @brief Stable machine-readable result code. */
    std::string code;

    /** @brief Human-readable mutation result message for logs and tests. */
    std::string message;

    /** @brief Exit events emitted synchronously by this mutation, if any. */
    std::vector<ObjectColliderOverlapEvent> exit_events;
};

/** @brief Return true when a capsule overlaps an object collider sphere using inclusive contact. */
[[nodiscard]] bool capsule_overlaps_object_sphere(const TriggerCapsule& capsule,
                                                  const ObjectColliderShape& sphere) noexcept;

/** @brief Return true when a capsule overlaps an object collider AABB using inclusive contact. */
[[nodiscard]] bool capsule_overlaps_object_aabb(const TriggerCapsule& capsule,
                                                const ObjectColliderShape& aabb) noexcept;

/** @brief Return true when two capsules overlap using inclusive contact. */
[[nodiscard]] bool capsule_overlaps_object_capsule(const TriggerCapsule& capsule,
                                                   const ObjectColliderShape& other) noexcept;

/** @brief Build runtime AABB object colliders from authored world metadata collider markers. */
[[nodiscard]] std::vector<ObjectCollider> build_object_colliders(
    const stellar::assets::WorldMetadataAsset& metadata);

/** @brief Build runtime AABB object colliders from a RuntimeWorld's copied metadata. */
[[nodiscard]] std::vector<ObjectCollider> build_object_colliders(const RuntimeWorld& world);

/** @brief Backend-neutral state machine for kinematic object collider overlaps. */
class ObjectColliderSystem {
public:
    /** @brief Hard-reset runtime colliders in caller-provided order and clear all overlap state. */
    void set_colliders(std::span<const ObjectCollider> colliders);

    /** @brief Replace colliders while preserving overlap state by stable collider id. */
    [[nodiscard]] std::vector<ObjectColliderOverlapEvent> replace_colliders_preserving_overlaps(
        std::span<const ObjectCollider> colliders) noexcept;

    /** @brief Enable or disable one collider by stable id and return a disable exit if needed. */
    [[nodiscard]] ObjectColliderMutationResult set_collider_enabled(
        std::uint32_t collider_id, bool enabled) noexcept;

    /** @brief Insert or replace one collider by stable id while preserving unrelated overlaps. */
    [[nodiscard]] ObjectColliderMutationResult upsert_collider(
        const ObjectCollider& collider) noexcept;

    /** @brief Remove one collider by stable id and return an exit if it was overlapped. */
    [[nodiscard]] ObjectColliderMutationResult remove_collider(std::uint32_t collider_id) noexcept;

    /** @brief Update one player capsule against all colliders and return enter/stay/exit events. */
    [[nodiscard]] std::vector<ObjectColliderOverlapEvent> update_player_capsule(
        const TriggerCapsule& player_capsule) noexcept;

    /** @brief Return the current immutable collider list in deterministic query order. */
    [[nodiscard]] const std::vector<ObjectCollider>& colliders() const noexcept;

    /** @brief Return deterministic diagnostics for current object-collider data. */
    [[nodiscard]] std::vector<ObjectColliderDiagnostic> diagnostics() const;

private:
    std::vector<ObjectCollider> colliders_;
    SensorOverlapTracker overlap_tracker_;
};

} // namespace stellar::world
