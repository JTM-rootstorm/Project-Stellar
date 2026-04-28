#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "ImporterPrivate.hpp"


namespace stellar::import::gltf {

std::expected<stellar::assets::ImageAsset, stellar::platform::Error>
load_image_from_file(const std::filesystem::path& path, const char* name) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        return std::unexpected(stellar::platform::Error("Failed to decode image file"));
    }

    stellar::assets::ImageAsset image;
    image.name = duplicate_to_string(name);
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);
    image.format = stellar::assets::ImageFormat::kR8G8B8A8;
    image.pixels.assign(pixels, pixels + static_cast<std::size_t>(width * height * 4));
    image.source_uri = path.string();

    stbi_image_free(pixels);
    return image;
}

std::expected<stellar::assets::ImageAsset, stellar::platform::Error>
load_image_from_memory(std::span<const std::uint8_t> bytes, const char* name,
                                      std::string source_uri, const char* error_message) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()),
                                            static_cast<int>(bytes.size()), &width, &height,
                                            &channels, 4);
    if (!pixels) {
        return std::unexpected(stellar::platform::Error(error_message));
    }

    stellar::assets::ImageAsset image;
    image.name = duplicate_to_string(name);
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);
    image.format = stellar::assets::ImageFormat::kR8G8B8A8;
    image.pixels.assign(pixels, pixels + static_cast<std::size_t>(width * height * 4));
    image.source_uri = std::move(source_uri);

    stbi_image_free(pixels);
    return image;
}

} // namespace stellar::import::gltf
