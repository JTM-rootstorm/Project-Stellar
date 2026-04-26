#include "stellar/client/ApplicationConfig.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace {

void write_text_file(const std::filesystem::path& path, std::string_view content) {
    std::ofstream file(path, std::ios::binary);
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "stellar_client_asset_smoke";
    std::filesystem::create_directories(root);

    const auto gltf_path = root / "valid_scene.gltf";
    write_text_file(gltf_path,
                    "{\n"
                    "  \"asset\": { \"version\": \"2.0\" },\n"
                    "  \"scenes\": [{ \"name\": \"empty\", \"nodes\": [] }],\n"
                    "  \"scene\": 0\n"
                    "}\n");

    stellar::client::ApplicationConfig config;
    config.asset_path = gltf_path.string();
    config.validate_only = true;

    const auto result = stellar::client::validate_application_config(config);

#if defined(STELLAR_ENABLE_GLTF)
    assert(result.has_value());
#else
    assert(!result.has_value());
    assert(result.error().message.find("STELLAR_ENABLE_GLTF=ON") != std::string::npos);
#endif

    return 0;
}
