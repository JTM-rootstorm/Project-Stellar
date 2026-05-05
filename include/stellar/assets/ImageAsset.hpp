#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace stellar::assets {

/**
 * @brief CPU-side decoded image data.
 */
enum class ImageFormat {
    /** @brief Image format is unknown or unsupported. */
    kUnknown,
    /** @brief Three-channel 8-bit RGB image data. */
    kR8G8B8,
    /** @brief Four-channel 8-bit RGBA image data. */
    kR8G8B8A8,
};

/**
 * @brief Imported image payload and metadata.
 */
struct ImageAsset {
    /** @brief Stable image name used by textures and diagnostics. */
    std::string name;

    /** @brief Image width in texels. */
    std::uint32_t width = 0;

    /** @brief Image height in texels. */
    std::uint32_t height = 0;

    /** @brief Pixel storage format for the decoded payload. */
    ImageFormat format = ImageFormat::kUnknown;

    /** @brief Tightly packed pixel bytes in ImageFormat channel order. */
    std::vector<std::uint8_t> pixels;

    /** @brief Source URI or logical asset identifier used for diagnostics. */
    std::string source_uri;
};

} // namespace stellar::assets
