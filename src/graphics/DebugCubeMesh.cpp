#include "stellar/graphics/DebugCubeMesh.hpp"

#include <array>
#include <cstddef>

namespace stellar::graphics {

namespace {

using Vec3 = std::array<float, 3>;

stellar::assets::MeshPrimitive make_face_primitive(const std::array<Vec3, 4>& positions,
                                                   const Vec3& normal,
                                                   std::size_t material_index) {
    stellar::assets::MeshPrimitive primitive;
    primitive.topology = stellar::assets::PrimitiveTopology::kTriangles;
    primitive.vertices = {
        {positions[0], normal, {0.0F, 0.0F}, {0.0F, 0.0F, 0.0F, 1.0F}},
        {positions[1], normal, {1.0F, 0.0F}, {0.0F, 0.0F, 0.0F, 1.0F}},
        {positions[2], normal, {1.0F, 1.0F}, {0.0F, 0.0F, 0.0F, 1.0F}},
        {positions[3], normal, {0.0F, 1.0F}, {0.0F, 0.0F, 0.0F, 1.0F}},
    };
    primitive.indices = {0, 1, 2, 0, 2, 3};
    primitive.bounds_min = {-0.5F, -0.5F, -0.5F};
    primitive.bounds_max = {0.5F, 0.5F, 0.5F};
    primitive.material_index = material_index;
    return primitive;
}

} // namespace

std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
create_debug_cube_mesh() {
    stellar::assets::MeshAsset mesh;
    mesh.name = "debug_cube";

    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5F, -0.5F, 0.5F}, {0.5F, -0.5F, 0.5F}, {0.5F, 0.5F, 0.5F},
          {-0.5F, 0.5F, 0.5F}}},
        {0.0F, 0.0F, 1.0F}, 0));
    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5F, -0.5F, -0.5F}, {-0.5F, 0.5F, -0.5F}, {0.5F, 0.5F, -0.5F},
          {0.5F, -0.5F, -0.5F}}},
        {0.0F, 0.0F, -1.0F}, 0));
    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5F, -0.5F, -0.5F}, {-0.5F, -0.5F, 0.5F}, {-0.5F, 0.5F, 0.5F},
          {-0.5F, 0.5F, -0.5F}}},
        {-1.0F, 0.0F, 0.0F}, 1));
    mesh.primitives.push_back(make_face_primitive(
        {{{0.5F, -0.5F, -0.5F}, {0.5F, 0.5F, -0.5F}, {0.5F, 0.5F, 0.5F},
          {0.5F, -0.5F, 0.5F}}},
        {1.0F, 0.0F, 0.0F}, 1));
    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5F, 0.5F, -0.5F}, {-0.5F, 0.5F, 0.5F}, {0.5F, 0.5F, 0.5F},
          {0.5F, 0.5F, -0.5F}}},
        {0.0F, 1.0F, 0.0F}, 2));
    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5F, -0.5F, -0.5F}, {0.5F, -0.5F, -0.5F}, {0.5F, -0.5F, 0.5F},
          {-0.5F, -0.5F, 0.5F}}},
        {0.0F, -1.0F, 0.0F}, 2));

    return mesh;
}

} // namespace stellar::graphics
