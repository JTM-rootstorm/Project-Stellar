#pragma once

#include <expected>
#include <memory>
#include <string_view>

#include "stellar/assets/SceneAsset.hpp"
#include "stellar/platform/Window.hpp"

namespace stellar::import::gltf {

/**
 * @brief Backend-neutral glTF importer interface.
 *
 * The importer owns no GPU state and returns CPU-side scene assets only.
 */
class Importer {
public:
    virtual ~Importer() = default;

    /**
     * @brief Load a glTF asset from disk.
     * @param path Path to a `.gltf` or `.glb` file.
     * @return Imported scene asset on success.
     */
    [[nodiscard]] virtual std::expected<stellar::assets::SceneAsset, stellar::platform::Error>
    load_file(std::string_view path) = 0;
};

/**
 * @brief Create the configured glTF importer implementation.
 */
[[nodiscard]] std::expected<std::unique_ptr<Importer>, stellar::platform::Error>
create_importer();

} // namespace stellar::import::gltf
