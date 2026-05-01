#pragma once

#include <array>
#include <span>
#include <string>
#include <vector>

#include "stellar/assets/WorldMetadataAsset.hpp"
#include "stellar/world/SensorOverlapTracker.hpp"

namespace stellar::world {

struct RuntimeWorld;

/**
 * @brief Runtime axis-aligned trigger volume built from authored world metadata.
 *
 * The center and half extents are in world units. Trigger boxes are axis-aligned at runtime;
 * authored marker rotation is intentionally ignored by this simple backend-neutral system.
 */
struct TriggerVolume {
    /** @brief Stable authored trigger name used in overlap events. */
    std::string name;

    /** @brief World-space center of the axis-aligned trigger box. */
    std::array<float, 3> center{0.0F, 0.0F, 0.0F};

    /** @brief World-space positive half extents of the axis-aligned trigger box. */
    std::array<float, 3> half_extents{0.5F, 0.5F, 0.5F};
};

/**
 * @brief Per-frame overlap transition for one trigger volume.
 */
struct TriggerOverlap {
    /** @brief Authored trigger name. */
    std::string name;

    /** @brief True only on the first update where the sphere overlaps this trigger. */
    bool entered = false;

    /** @brief True on updates after enter while the sphere remains overlapping this trigger. */
    bool stayed = false;

    /** @brief True only on the first update where the sphere no longer overlaps this trigger. */
    bool exited = false;
};

/**
 * @brief Vertical capsule used for backend-neutral trigger overlap queries.
 *
 * The center is the world-space capsule center, height is the total capsule height including both
 * hemispherical ends, and up is normalized internally with world-Y fallback for zero vectors.
 */
struct TriggerCapsule {
    /** @brief World-space center of the capsule. */
    std::array<float, 3> center{};

    /** @brief Capsule axis direction; normalized by TriggerSystem during overlap queries. */
    std::array<float, 3> up{0.0F, 1.0F, 0.0F};

    /** @brief Capsule radius in world units. */
    float radius = 0.0F;

    /** @brief Total capsule height including hemispherical ends. */
    float height = 0.0F;
};

/**
 * @brief Deterministic backend-neutral trigger overlap state machine.
 *
 * Sphere-vs-AABB tests use an inclusive touch policy: if the closest point on the trigger AABB is
 * exactly radius distance from the sphere center, the sphere is considered overlapping.
 */
class TriggerSystem {
public:
    /**
     * @brief Replace runtime trigger volumes and clear previous overlap state.
     */
    void set_triggers(std::span<const TriggerVolume> triggers);

    /**
     * @brief Update one sphere against all triggers and return enter/stay/exit transitions.
     */
    [[nodiscard]] std::vector<TriggerOverlap> update_sphere(std::array<float, 3> center,
                                                            float radius) noexcept;

    /**
     * @brief Update one capsule against all triggers and return enter/stay/exit transitions.
     *
     * Capsule-vs-AABB tests use the same inclusive touch policy as sphere-vs-AABB tests. Invalid
     * capsule positions deterministically produce no current overlaps.
     */
    [[nodiscard]] std::vector<TriggerOverlap> update_capsule(
        const TriggerCapsule& capsule) noexcept;

    /**
     * @brief Return the current immutable trigger volume list.
     */
    [[nodiscard]] const std::vector<TriggerVolume>& triggers() const noexcept;

private:
    std::vector<TriggerVolume> triggers_;
    SensorOverlapTracker overlap_tracker_;
};

/**
 * @brief Build runtime trigger volumes from authored world metadata trigger markers.
 *
 * Only markers with type `WorldMarkerType::kTrigger` are converted. Marker position becomes the box
 * center and absolute marker scale becomes AABB half extents; zero scale is allowed and represents a
 * plane/line/point trigger that still follows the inclusive sphere-touch overlap policy.
 */
[[nodiscard]] std::vector<TriggerVolume> build_trigger_volumes(
    const stellar::assets::WorldMetadataAsset& metadata);

/**
 * @brief Build runtime trigger volumes from a runtime world's copied metadata.
 */
[[nodiscard]] std::vector<TriggerVolume> build_trigger_volumes(const RuntimeWorld& world);

} // namespace stellar::world
