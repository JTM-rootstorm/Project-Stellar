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
    /** @brief Object-space vertex position. */
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};

    /** @brief Object-space vertex normal. */
    std::array<float, 3> normal{0.0f, 0.0f, 1.0f};

    /** @brief Primary texture coordinate set. */
    std::array<float, 2> uv0{0.0f, 0.0f};

    /** @brief Object-space tangent with handedness in w. */
    std::array<float, 4> tangent{0.0f, 0.0f, 0.0f, 1.0f};

    /** @brief Secondary texture coordinate set. */
    std::array<float, 2> uv1{0.0f, 0.0f};

    /** @brief Vertex color multiplier in linear RGBA. */
    std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
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
    /** @brief Primitive topology used to draw the index stream. */
    PrimitiveTopology topology = PrimitiveTopology::kTriangles;

    /** @brief Backend-neutral static vertex buffer. */
    std::vector<StaticVertex> vertices;

    /** @brief 32-bit index buffer into vertices. */
    std::vector<std::uint32_t> indices;

    /** @brief True when every vertex has valid tangent data. */
    bool has_tangents = false;

    /** @brief True when every vertex has authored COLOR_0 data. */
    bool has_colors = false;

    /** @brief Object-space axis-aligned bounds minimum. */
    std::array<float, 3> bounds_min{0.0f, 0.0f, 0.0f};

    /** @brief Object-space axis-aligned bounds maximum. */
    std::array<float, 3> bounds_max{0.0f, 0.0f, 0.0f};

    /** @brief Optional material index for this primitive. */
    std::optional<std::size_t> material_index;

    /** @brief Legacy optional texture index retained for older static paths. */
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
