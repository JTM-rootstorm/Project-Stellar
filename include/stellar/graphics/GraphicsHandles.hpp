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
    /** @brief Backend-assigned nonzero identifier for a GPU mesh resource. */
    std::uint64_t value = 0;

    /** @brief Return true when this handle names a live or potentially live mesh resource. */
    [[nodiscard]] explicit operator bool() const noexcept {
        return value != 0;
    }

    /** @brief Compare mesh handles by opaque identifier value. */
    friend bool operator==(const MeshHandle&, const MeshHandle&) = default;
};

/**
 * @brief Opaque GPU texture handle.
 */
struct TextureHandle {
    /** @brief Backend-assigned nonzero identifier for a GPU texture resource. */
    std::uint64_t value = 0;

    /** @brief Return true when this handle names a live or potentially live texture resource. */
    [[nodiscard]] explicit operator bool() const noexcept {
        return value != 0;
    }

    /** @brief Compare texture handles by opaque identifier value. */
    friend bool operator==(const TextureHandle&, const TextureHandle&) = default;
};

/**
 * @brief Opaque GPU material handle.
 */
struct MaterialHandle {
    /** @brief Backend-assigned nonzero identifier for a GPU material resource. */
    std::uint64_t value = 0;

    /** @brief Return true when this handle names a live or potentially live material resource. */
    [[nodiscard]] explicit operator bool() const noexcept {
        return value != 0;
    }

    /** @brief Compare material handles by opaque identifier value. */
    friend bool operator==(const MaterialHandle&, const MaterialHandle&) = default;
};

} // namespace stellar::graphics
