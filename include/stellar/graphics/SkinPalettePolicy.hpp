#pragma once

#include <cstddef>

namespace stellar::graphics {

/**
 * @brief Backend-neutral runtime skin palette capacity shared by OpenGL and Vulkan draws.
 *
 * Import may preserve larger skins in CPU assets, but render backends fail mesh upload when a
 * primitive references a joint index at or above this capacity and skip draw calls whose provided
 * skin palette is empty, undersized for the primitive's referenced joint indices, or larger than
 * this capacity. The shared storage-buffer path keeps OpenGL and Vulkan behavior aligned.
 */
inline constexpr std::size_t kMaxSkinPaletteJoints = 256;

} // namespace stellar::graphics
