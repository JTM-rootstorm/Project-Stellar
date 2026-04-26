#include "stellar/import/gltf/Loader.hpp"

#include "stellar/import/gltf/Importer.hpp"

namespace stellar::import::gltf {

std::expected<stellar::assets::SceneAsset, stellar::platform::Error>
load_scene(std::string_view path) {
    auto importer = create_importer();
    if (!importer) {
        return std::unexpected(importer.error());
    }

    return (*importer)->load_file(path);
}

} // namespace stellar::import::gltf
