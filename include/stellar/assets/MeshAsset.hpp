#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace stellar::assets {

/**
 * @brief Standard static mesh vertex.
 */
struct StaticVertex {
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    std::array<float, 3> normal{0.0f, 0.0f, 1.0f};
    std::array<float, 2> uv0{0.0f, 0.0f};
    std::array<float, 4> tangent{0.0f, 0.0f, 0.0f, 1.0f};
};

/**
 * @brief Primitive topology for mesh primitives.
 */
enum class PrimitiveTopology {
    kTriangles,
};

/**
 * @brief One drawable primitive of a mesh.
 */
struct MeshPrimitive {
    PrimitiveTopology topology = PrimitiveTopology::kTriangles;
    std::vector<StaticVertex> vertices;
    std::vector<std::uint32_t> indices;
    bool has_tangents = false;
    std::array<float, 3> bounds_min{0.0f, 0.0f, 0.0f};
    std::array<float, 3> bounds_max{0.0f, 0.0f, 0.0f};
    std::optional<std::size_t> material_index;
    std::optional<std::size_t> texture_index;
};

/**
 * @brief Imported mesh payload.
 */
struct MeshAsset {
    std::string name;
    std::vector<MeshPrimitive> primitives;
    std::string source_uri;
};

} // namespace stellar::assets
