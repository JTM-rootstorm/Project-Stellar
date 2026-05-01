#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <optional>

#include "stellar/assets/LevelAsset.hpp"
#include "stellar/assets/MeshAsset.hpp"
#include "stellar/graphics/GraphicsBackend.hpp"
#include "stellar/graphics/RenderLevel.hpp"
#include "stellar/graphics/Renderer.hpp"

namespace stellar::graphics {

/**
 * @brief Axis-aligned world-space bounds for imported level geometry.
 */
struct LevelBounds {
  /** @brief World-space minimum corner. */
  std::array<float, 3> min{-0.5F, -0.5F, -0.5F};

  /** @brief World-space maximum corner. */
  std::array<float, 3> max{0.5F, 0.5F, 0.5F};

  /** @brief World-space bounds center. */
  std::array<float, 3> center{0.0F, 0.0F, 0.0F};

  /** @brief Radius of a sphere enclosing the bounds. */
  float radius = 1.0F;
};

/**
 * @brief Camera parameters fitted to level bounds.
 */
struct LevelCameraFit {
  /** @brief Camera eye position. */
  std::array<float, 3> eye{0.0F, 0.0F, 3.0F};

  /** @brief Camera look-at target. */
  std::array<float, 3> target{0.0F, 0.0F, 0.0F};

  /** @brief Near clip plane. */
  float near_plane = 0.1F;

  /** @brief Far clip plane. */
  float far_plane = 100.0F;
};

/** @brief Compute aggregate world-space bounds for static level geometry. */
[[nodiscard]] LevelBounds
compute_level_bounds(const stellar::assets::LevelAsset &level) noexcept;

/** @brief Compute a stable camera fit for the provided level bounds. */
[[nodiscard]] LevelCameraFit fit_camera_to_bounds(const LevelBounds &bounds,
                                                  float vertical_fov_degrees,
                                                  float aspect_ratio) noexcept;

/**
 * @brief Generic static level renderer with a debug cube fallback.
 */
class LevelRenderer final : public Renderer {
public:
  /**
   * @brief Construct a renderer from an optional CPU-side level asset using
   * OpenGL.
   */
  explicit LevelRenderer(
      std::optional<stellar::assets::LevelAsset> level = std::nullopt) noexcept;

  /**
   * @brief Construct a renderer for a specific runtime backend.
   */
  explicit LevelRenderer(
      GraphicsBackend backend,
      std::optional<stellar::assets::LevelAsset> level = std::nullopt) noexcept;

  /**
   * @brief Release renderer-owned level resources.
   */
  ~LevelRenderer() noexcept;

  LevelRenderer(const LevelRenderer &) = delete;
  LevelRenderer &operator=(const LevelRenderer &) = delete;

  /**
   * @brief Initialize GPU resources for the provided level or fallback cube.
   */
  [[nodiscard]] std::expected<void, stellar::platform::Error>
  initialize(stellar::platform::Window &window) override;

  /**
   * @brief Render the loaded level.
   */
  void render(float elapsed_seconds, float delta_seconds, int width,
              int height) noexcept override;

private:
  [[nodiscard]] static std::expected<stellar::assets::MeshAsset,
                                     stellar::platform::Error>
  create_cube_mesh();

  [[nodiscard]] static stellar::assets::LevelAsset create_cube_level();

  GraphicsBackend backend_ = GraphicsBackend::kOpenGL;
  std::optional<stellar::assets::LevelAsset> source_level_;
  LevelBounds level_bounds_;
  RenderLevel level_;
};

/** @brief Temporary compatibility alias for old renderer naming during BSP
 * migration. */
using SceneBounds = LevelBounds;

/** @brief Temporary compatibility alias for old camera fit naming during BSP
 * migration. */
using SceneCameraFit = LevelCameraFit;

/** @brief Temporary compatibility alias for old renderer naming during BSP
 * migration. */
using SceneRenderer = LevelRenderer;

} // namespace stellar::graphics
