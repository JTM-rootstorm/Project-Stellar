#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "stellar/assets/LevelAsset.hpp"

namespace stellar::assets {

/**
 * @brief Find the first visibility leaf whose bounds contain a world-space point.
 *
 * This helper is source-neutral and intended for renderer-side presentation
 * culling only. If visibility data is unavailable, the point contains
 * non-finite values, or no leaf bounds contain the point, no leaf is returned.
 */
[[nodiscard]] inline std::optional<std::size_t>
find_level_leaf_for_point(const LevelVisibilityAsset &visibility,
                          std::array<float, 3> position) noexcept {
    if (!visibility.available) {
        return std::nullopt;
    }

    for (const float coordinate : position) {
        if (!(coordinate <= std::numeric_limits<float>::max() &&
              coordinate >= -std::numeric_limits<float>::max())) {
            return std::nullopt;
        }
    }

    for (std::size_t leaf_index = 0; leaf_index < visibility.leaves.size();
         ++leaf_index) {
        const LevelLeaf &leaf = visibility.leaves[leaf_index];
        bool contains = true;
        for (std::size_t axis = 0; axis < position.size(); ++axis) {
            if (position[axis] < leaf.bounds_min[axis] ||
                position[axis] > leaf.bounds_max[axis]) {
                contains = false;
                break;
            }
        }
        if (contains) {
            return leaf_index;
        }
    }

    return std::nullopt;
}

namespace detail {

/**
 * @brief Decompress classic BSP run-length encoded PVS bytes into leaf visibility bits.
 */
[[nodiscard]] inline std::optional<std::vector<bool>> decompress_level_pvs_bits(
    const LevelVisibilityAsset &visibility, std::size_t offset,
    std::size_t bit_count) noexcept {
    if (bit_count == 0 || offset >= visibility.compressed_pvs.size()) {
        return std::nullopt;
    }

    std::vector<bool> visible_leaves(bit_count, false);
    std::size_t pvs_offset = offset;
    std::size_t leaf_bit = 0;
    while (leaf_bit < bit_count) {
        if (pvs_offset >= visibility.compressed_pvs.size()) {
            return std::nullopt;
        }

        const auto value =
            std::to_integer<std::uint8_t>(visibility.compressed_pvs[pvs_offset]);
        ++pvs_offset;
        if (value == 0) {
            if (pvs_offset >= visibility.compressed_pvs.size()) {
                return std::nullopt;
            }
            const auto zero_run = std::to_integer<std::uint8_t>(
                visibility.compressed_pvs[pvs_offset]);
            ++pvs_offset;
            if (zero_run == 0) {
                return std::nullopt;
            }
            leaf_bit = std::min(
                bit_count, leaf_bit + static_cast<std::size_t>(zero_run) * 8U);
            continue;
        }

        for (std::size_t bit = 0; bit < 8 && leaf_bit < bit_count;
             ++bit, ++leaf_bit) {
            visible_leaves[leaf_bit] = ((value >> bit) & 0x1U) != 0;
        }
    }

    return visible_leaves;
}

} // namespace detail

/**
 * @brief Build a per-surface visibility mask from a classic BSP-style leaf PVS row.
 *
 * Returns an all-visible mask when visibility data is unavailable, the leaf
 * index is invalid, or the compressed PVS stream is malformed. PVS is
 * presentation-only and never gameplay authority.
 */
[[nodiscard]] inline std::vector<bool>
visible_level_surfaces_from_leaf(const LevelAsset &level, std::size_t leaf_index) {
    const LevelVisibilityAsset &visibility = level.visibility;
    std::vector<bool> visible_surfaces(level.geometry.surfaces.size(), true);
    if (!visibility.available || leaf_index >= visibility.leaves.size()) {
        return visible_surfaces;
    }

    const LevelLeaf &camera_leaf = visibility.leaves[leaf_index];
    if (!camera_leaf.compressed_pvs_offset.has_value() ||
        *camera_leaf.compressed_pvs_offset >= visibility.compressed_pvs.size()) {
        return visible_surfaces;
    }

    const std::size_t leaf_count =
        visibility.cluster_count == 0 ? visibility.leaves.size() : visibility.cluster_count;
    if (leaf_count == 0 || leaf_count > visibility.leaves.size()) {
        return visible_surfaces;
    }

    auto visible_leaves = detail::decompress_level_pvs_bits(
        visibility, *camera_leaf.compressed_pvs_offset, leaf_count);
    if (!visible_leaves.has_value()) {
        return visible_surfaces;
    }

    std::fill(visible_surfaces.begin(), visible_surfaces.end(), false);
    for (std::size_t visible_leaf_index = 0; visible_leaf_index < leaf_count;
         ++visible_leaf_index) {
        if (!(*visible_leaves)[visible_leaf_index]) {
            continue;
        }
        for (const std::size_t surface_index :
             visibility.leaves[visible_leaf_index].surface_indices) {
            if (surface_index < visible_surfaces.size()) {
                visible_surfaces[surface_index] = true;
            }
        }
    }

    return visible_surfaces;
}

} // namespace stellar::assets
