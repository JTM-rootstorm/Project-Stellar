#pragma once

#include <cstdint>

namespace stellar::graphics {

/**
 * @brief Opaque GPU mesh handle.
 *
 * The device that created the handle owns the underlying GPU resource.
 * Handles are valid until explicitly destroyed on the same device or the
 * device is destroyed.
 */
struct MeshHandle {
    std::uint64_t value = 0;

    [[nodiscard]] explicit operator bool() const noexcept {
        return value != 0;
    }

    friend bool operator==(const MeshHandle&, const MeshHandle&) = default;
};

/**
 * @brief Opaque GPU texture handle.
 */
struct TextureHandle {
    std::uint64_t value = 0;

    [[nodiscard]] explicit operator bool() const noexcept {
        return value != 0;
    }

    friend bool operator==(const TextureHandle&, const TextureHandle&) = default;
};

/**
 * @brief Opaque GPU material handle.
 */
struct MaterialHandle {
    std::uint64_t value = 0;

    [[nodiscard]] explicit operator bool() const noexcept {
        return value != 0;
    }

    friend bool operator==(const MaterialHandle&, const MaterialHandle&) = default;
};

} // namespace stellar::graphics
