#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "stellar/world/TriggerSystem.hpp"

namespace stellar::world {

/** @brief Backend-neutral runtime collider shape kind for kinematic objects. */
enum class ObjectColliderShapeType {
    kSphere,
    kAabb,
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

/** @brief Return true when a capsule overlaps an object collider sphere using inclusive contact. */
[[nodiscard]] bool capsule_overlaps_object_sphere(const TriggerCapsule& capsule,
                                                  const ObjectColliderShape& sphere) noexcept;

/** @brief Return true when a capsule overlaps an object collider AABB using inclusive contact. */
[[nodiscard]] bool capsule_overlaps_object_aabb(const TriggerCapsule& capsule,
                                                const ObjectColliderShape& aabb) noexcept;

/** @brief Return true when two capsules overlap using inclusive contact. */
[[nodiscard]] bool capsule_overlaps_object_capsule(const TriggerCapsule& capsule,
                                                   const ObjectColliderShape& other) noexcept;

/** @brief Backend-neutral state machine for kinematic object collider overlaps. */
class ObjectColliderSystem {
public:
    /** @brief Replace runtime colliders in caller-provided order and clear overlap state. */
    void set_colliders(std::span<const ObjectCollider> colliders);

    /** @brief Update one player capsule against all colliders and return enter/stay/exit events. */
    [[nodiscard]] std::vector<ObjectColliderOverlapEvent> update_player_capsule(
        const TriggerCapsule& player_capsule) noexcept;

    /** @brief Return the current immutable collider list in deterministic query order. */
    [[nodiscard]] const std::vector<ObjectCollider>& colliders() const noexcept;

private:
    std::vector<ObjectCollider> colliders_;
    std::vector<bool> previous_overlaps_;
};

} // namespace stellar::world
