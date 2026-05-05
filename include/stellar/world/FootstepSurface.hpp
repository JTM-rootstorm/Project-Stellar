#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace stellar::world {

/** @brief Return a stable footstep surface id from a source material or texture name. */
[[nodiscard]] inline std::string resolve_footstep_surface_id(
    std::string_view source_material_name) {
    std::string normalized(source_material_name);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    const std::size_t slash = normalized.find_last_of('/');
    const std::size_t dot = normalized.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        normalized.erase(dot);
    }

    const auto contains = [&normalized](std::string_view token) {
        return normalized.find(token) != std::string::npos;
    };

    if (contains("metal") || contains("grate") || contains("pipe") || contains("vent")) {
        return "metal";
    }
    if (contains("wood") || contains("plank") || contains("crate") || contains("board")) {
        return "wood";
    }
    if (contains("stone") || contains("rock") || contains("brick") || contains("tile")) {
        return "stone";
    }
    if (contains("dirt") || contains("mud") || contains("soil") || contains("sand")) {
        return "dirt";
    }
    if (contains("grass") || contains("moss") || contains("foliage")) {
        return "grass";
    }
    if (contains("water") || contains("slime") || contains("liquid")) {
        return "water";
    }
    if (contains("concrete") || contains("cement") || contains("asphalt") ||
        contains("dev/grid") || contains("stellar_dev_grid")) {
        return "concrete";
    }
    return "generic";
}

} // namespace stellar::world
