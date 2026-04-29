#include "stellar/import/gltf/Importer.hpp"

#include "ImporterPrivate.hpp"

#include <memory>
#include <string_view>

namespace stellar::import::gltf {
namespace {

class CgltfImporter final : public Importer {
public:
    [[nodiscard]] std::expected<stellar::assets::SceneAsset, stellar::platform::Error>
    load_file(std::string_view path) override {
        return load_scene_from_file(path);
    }
};

} // namespace

std::expected<std::unique_ptr<Importer>, stellar::platform::Error> create_importer() {
    return std::make_unique<CgltfImporter>();
}

} // namespace stellar::import::gltf
