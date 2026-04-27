#pragma once

#include <expected>

#include "stellar/assets/MeshAsset.hpp"
#include "stellar/platform/Error.hpp"

namespace stellar::graphics {

/** @brief Create the built-in debug cube mesh used by prototype scene renderers. */
[[nodiscard]] std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
create_debug_cube_mesh();

} // namespace stellar::graphics
