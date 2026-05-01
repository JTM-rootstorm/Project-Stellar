#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace stellar::world {

/**
 * @brief Current overlap sample for one deterministic sensor identity.
 */
struct SensorOverlapSample {
    /** @brief Stable sensor identifier used to preserve overlap state across updates. */
    std::uint32_t id = 0;

    /** @brief Human-readable sensor name copied into transition events. */
    std::string name;

    /** @brief True when the sensor is overlapping in the current query. */
    bool currently_overlapping = false;
};

/**
 * @brief Enter/stay/exit transition for one deterministic sensor identity.
 */
struct SensorOverlapTransition {
    /** @brief Stable sensor identifier copied from the sample that produced the transition. */
    std::uint32_t id = 0;

    /** @brief Human-readable sensor name copied from the sample or stored overlap state. */
    std::string name;

    /** @brief True only on the first update where overlap becomes current. */
    bool entered = false;

    /** @brief True on updates after enter while overlap remains current. */
    bool stayed = false;

    /** @brief True only when a previously current overlap ends. */
    bool exited = false;
};

/**
 * @brief Shared deterministic overlap state tracker for sensor-like gameplay systems.
 *
 * The tracker owns only previous/current overlap bookkeeping. Callers remain responsible for
 * deterministic sample ordering, duplicate-id policy, and geometry-specific overlap queries.
 */
class SensorOverlapTracker {
public:
    /** @brief Replace tracked state with the provided samples without emitting transitions. */
    void reset(std::span<const SensorOverlapSample> samples);

    /** @brief Update tracked state and return enter/stay/exit transitions in sample order. */
    [[nodiscard]] std::vector<SensorOverlapTransition> update(
        std::span<const SensorOverlapSample> samples);

    /** @brief Mark one tracked sensor inactive and return a synchronous exit if it overlapped. */
    [[nodiscard]] std::vector<SensorOverlapTransition> remove_or_disable(
        std::uint32_t id, std::string_view fallback_name) noexcept;

    /** @brief Return true when the tracker currently stores an overlapping state for the id. */
    [[nodiscard]] bool is_overlapping(std::uint32_t id) const noexcept;

    /** @brief Return the stored name for an id, or the fallback when the id is not tracked. */
    [[nodiscard]] std::string name_or(std::uint32_t id, std::string_view fallback_name) const;

private:
    struct Entry {
        std::uint32_t id = 0;
        std::string name;
        bool overlapping = false;
    };

    std::vector<Entry> entries_;
};

} // namespace stellar::world
