#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace stellar::assets {

/**
 * @brief CPU-side skeletal skin data imported from a glTF skin.
 */
struct SkinAsset {
    /** @brief Optional authoring name for the skin. */
    std::string name;

    /** @brief Scene node indices used as joints by this skin. */
    std::vector<std::size_t> joints;

    /** @brief Optional scene node index of the skeleton root. */
    std::optional<std::size_t> skeleton_root;

    /** @brief Inverse bind matrices in joint order, column-major as stored by glTF. */
    std::vector<std::array<float, 16>> inverse_bind_matrices;
};

} // namespace stellar::assets
