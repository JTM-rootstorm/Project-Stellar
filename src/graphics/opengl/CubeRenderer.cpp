#include "stellar/graphics/opengl/CubeRenderer.hpp"

#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "stellar/graphics/DebugCubeMesh.hpp"
#include "stellar/graphics/GraphicsDeviceFactory.hpp"

namespace stellar::graphics::opengl {

namespace {

std::array<float, 16> to_array(const glm::mat4 &matrix) {
  std::array<float, 16> result{};
  const float *data = &matrix[0][0];
  for (std::size_t index = 0; index < result.size(); ++index) {
    result[index] = data[index];
  }
  return result;
}

} // namespace

std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
CubeRenderer::create_cube_mesh() {
  return stellar::graphics::create_debug_cube_mesh();
}

stellar::assets::LevelAsset CubeRenderer::create_cube_level() {
  stellar::assets::LevelAsset level;
  level.source_uri = "debug:cube";
  level.geometry.meshes.push_back(create_cube_mesh().value());
  level.geometry.materials.push_back(
      stellar::assets::LevelSurfaceMaterial{.name = "debug_red"});
  level.geometry.materials.push_back(
      stellar::assets::LevelSurfaceMaterial{.name = "debug_green"});
  level.geometry.materials.push_back(
      stellar::assets::LevelSurfaceMaterial{.name = "debug_blue"});
  for (std::size_t primitive_index = 0;
       primitive_index < level.geometry.meshes[0].primitives.size();
       ++primitive_index) {
    const auto &primitive =
        level.geometry.meshes[0].primitives[primitive_index];
    level.geometry.surfaces.push_back(stellar::assets::LevelSurface{
        .name = "debug_cube_surface",
        .mesh_index = 0,
        .primitive_index = primitive_index,
        .material_index = primitive.material_index,
        .bounds_min = primitive.bounds_min,
        .bounds_max = primitive.bounds_max,
    });
  }
  return level;
}

CubeRenderer::~CubeRenderer() noexcept = default;

std::expected<void, stellar::platform::Error>
CubeRenderer::initialize(stellar::platform::Window &window) {
  auto level = create_cube_level();
  auto device = stellar::graphics::create_graphics_device();
  if (!device) {
    return std::unexpected(
        stellar::platform::Error("Failed to create graphics device"));
  }

  return level_.initialize(std::move(device), window, std::move(level));
}

void CubeRenderer::render(float elapsed_seconds, float /*delta_seconds*/,
                          int width, int height) noexcept {
  const glm::mat4 projection = glm::perspective(
      glm::radians(45.0f),
      static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);
  const glm::mat4 view =
      glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 1.0f, 0.0f));
  level_.render(width, height, to_array(projection * view), to_array(view));
}

} // namespace stellar::graphics::opengl
