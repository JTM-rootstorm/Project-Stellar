#pragma once

#include <expected>
#include <string_view>

#include "stellar/assets/SceneAsset.hpp"
#include "stellar/platform/Window.hpp"

namespace stellar::import::gltf {

/**
 * @brief Load a glTF scene into CPU-side engine assets.
 *
 * This entry point is intentionally backend-neutral and returns only
 * `SceneAsset` data.
 */
[[nodiscard]] std::expected<stellar::assets::SceneAsset, stellar::platform::Error>
load_scene(std::string_view path);

} // namespace stellar::import::gltf
