#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace stellar::assets {

/**
 * @brief CPU-side decoded image data.
 */
enum class ImageFormat {
    kUnknown,
    kR8G8B8,
    kR8G8B8A8,
};

/**
 * @brief Imported image payload and metadata.
 */
struct ImageAsset {
    std::string name;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    ImageFormat format = ImageFormat::kUnknown;
    std::vector<std::uint8_t> pixels;
    std::string source_uri;
};

} // namespace stellar::assets
