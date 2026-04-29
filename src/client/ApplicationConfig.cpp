#include "stellar/client/ApplicationConfig.hpp"

#if defined(STELLAR_ENABLE_GLTF)
#include "stellar/import/gltf/Loader.hpp"
#endif

#include <utility>

namespace stellar::client {

std::expected<ApplicationValidation, stellar::platform::Error>
validate_application_config(const ApplicationConfig& config) {
    ApplicationValidation validation;

    if (!config.asset_path.has_value()) {
        return validation;
    }

#if defined(STELLAR_ENABLE_GLTF)
    auto loaded_scene = stellar::import::gltf::load_scene(*config.asset_path);
    if (!loaded_scene) {
        return std::unexpected(loaded_scene.error());
    }

    validation.scene = std::move(*loaded_scene);
    return validation;
#else
    return std::unexpected(stellar::platform::Error(
        "--asset requires a build configured with STELLAR_ENABLE_GLTF=ON"));
#endif
}

} // namespace stellar::client
