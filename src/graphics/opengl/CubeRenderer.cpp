#include "stellar/graphics/opengl/CubeRenderer.hpp"

#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "stellar/graphics/GraphicsDeviceFactory.hpp"

namespace stellar::graphics::opengl {

namespace {

using Vec3 = std::array<float, 3>;

stellar::assets::MeshPrimitive make_face_primitive(const std::array<Vec3, 4>& positions,
                                                   const Vec3& normal,
                                                   std::size_t material_index) {
    stellar::assets::MeshPrimitive primitive;
    primitive.topology = stellar::assets::PrimitiveTopology::kTriangles;
    primitive.vertices = {
        {positions[0], normal, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
        {positions[1], normal, {1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
        {positions[2], normal, {1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
        {positions[3], normal, {0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
    };
    primitive.indices = {0, 1, 2, 0, 2, 3};
    primitive.bounds_min = {-0.5f, -0.5f, -0.5f};
    primitive.bounds_max = {0.5f, 0.5f, 0.5f};
    primitive.material_index = material_index;
    return primitive;
}

std::array<float, 16> to_array(const glm::mat4& matrix) {
    std::array<float, 16> result{};
    const float* data = &matrix[0][0];
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = data[index];
    }
    return result;
}

} // namespace

std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
CubeRenderer::create_cube_mesh() {
    stellar::assets::MeshAsset mesh;
    mesh.name = "debug_cube";

    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f}, { 0.5f,  0.5f,  0.5f},
          {-0.5f,  0.5f,  0.5f}}},
        {0.0f, 0.0f, 1.0f}, 0));
    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5f, -0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f},
          { 0.5f, -0.5f, -0.5f}}},
        {0.0f, 0.0f, -1.0f}, 0));
    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f},
          {-0.5f,  0.5f, -0.5f}}},
        {-1.0f, 0.0f, 0.0f}, 1));
    mesh.primitives.push_back(make_face_primitive(
        {{{ 0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f,  0.5f}, { 0.5f,  0.5f,  0.5f},
          { 0.5f,  0.5f, -0.5f}}},
        {1.0f, 0.0f, 0.0f}, 1));
    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f,  0.5f},
          {-0.5f,  0.5f,  0.5f}}},
        {0.0f, 1.0f, 0.0f}, 2));
    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f,  0.5f},
          {-0.5f, -0.5f,  0.5f}}},
        {0.0f, -1.0f, 0.0f}, 2));

    return mesh;
}

stellar::assets::SceneAsset CubeRenderer::create_cube_scene() {
    stellar::assets::SceneAsset scene;
    scene.source_uri = "debug:cube";
    scene.meshes.push_back(create_cube_mesh().value());
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "debug_red",
        .base_color_factor = {1.0f, 0.0f, 0.0f, 1.0f},
    });
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "debug_green",
        .base_color_factor = {0.0f, 1.0f, 0.0f, 1.0f},
    });
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "debug_blue",
        .base_color_factor = {0.0f, 0.0f, 1.0f, 1.0f},
    });
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0}});
    scene.nodes.push_back(stellar::scene::Node{
        .name = "cube",
        .parent_index = std::nullopt,
        .children = {},
        .local_transform = stellar::scene::Transform{},
        .mesh_instances = {stellar::scene::MeshInstance{0}},
        .skin_index = std::nullopt,
    });
    scene.default_scene_index = 0;
    return scene;
}

CubeRenderer::~CubeRenderer() noexcept = default;

std::expected<void, stellar::platform::Error>
CubeRenderer::initialize(stellar::platform::Window& window) {
    auto scene = create_cube_scene();
    auto device = stellar::graphics::create_graphics_device();
    if (!device) {
        return std::unexpected(stellar::platform::Error("Failed to create graphics device"));
    }

    return scene_.initialize(std::move(device), window, std::move(scene));
}

void CubeRenderer::render(float elapsed_seconds,
                          float /*delta_seconds*/,
                          int width,
                          int height) noexcept {
    const float rotation_degrees = elapsed_seconds * 45.0F;
    if (scene_.node_transform(0).matrix.has_value()) {
        scene_.node_transform(0).matrix = std::nullopt;
    }

    const glm::mat4 projection = glm::perspective(
        glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f,
        100.0f);
    const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                                       glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 model = glm::rotate(
        glm::rotate(glm::mat4(1.0f), glm::radians(rotation_degrees), glm::vec3(0.0f, 1.0f, 0.0f)),
        glm::radians(rotation_degrees), glm::vec3(1.0f, 0.0f, 0.0f));

    scene_.node_transform(0).matrix = to_array(model);
    scene_.render(width, height, to_array(projection * view), to_array(view));
}

} // namespace stellar::graphics::opengl
