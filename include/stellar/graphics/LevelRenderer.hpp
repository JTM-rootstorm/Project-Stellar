#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <vector>

#include "stellar/assets/LevelAsset.hpp"
#include "stellar/assets/MeshAsset.hpp"
#include "stellar/core/WorldAxes.hpp"
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

/**
 * @brief Backend-neutral world-space camera used by level presentation.
 */
struct LevelRenderView {
  /** @brief Camera eye position in level world space. */
  std::array<float, 3> eye{0.0F, 0.0F, 3.0F};

  /** @brief Camera look-at target in level world space. */
  std::array<float, 3> target{0.0F, 0.0F, 0.0F};

  /** @brief Camera up direction in level world space. */
  std::array<float, 3> up{stellar::core::kWorldUp};

  /** @brief Vertical perspective field of view in degrees. */
  float vertical_fov_degrees = 45.0F;

  /** @brief Near clip plane. */
  float near_plane = 0.1F;

  /** @brief Far clip plane. */
  float far_plane = 100.0F;

  /** @brief Whether the eye position should drive optional visibility culling. */
  bool visibility_culling = true;
};

/**
 * @brief Resolved per-frame render state for a level view.
 */
struct LevelRenderState {
  /** @brief Column-major view-projection matrix. */
  std::array<float, 16> view_projection{};

  /** @brief Column-major view matrix. */
  std::array<float, 16> view{};

  /** @brief Optional camera position used for presentation-only visibility culling. */
  std::optional<std::array<float, 3>> camera_world_position;
};

/**
 * @brief Backend-neutral presentation data submitted after static level geometry.
 */
struct LevelPresentationState {
  /** @brief Gameplay and presentation billboards for the current rendered frame. */
  std::vector<BillboardSprite> sprites;
};

/** @brief Compute aggregate world-space bounds for static level geometry. */
[[nodiscard]] LevelBounds
compute_level_bounds(const stellar::assets::LevelAsset &level) noexcept;

/** @brief Compute a stable camera fit for the provided level bounds. */
[[nodiscard]] LevelCameraFit fit_camera_to_bounds(const LevelBounds &bounds,
                                                  float vertical_fov_degrees,
                                                  float aspect_ratio) noexcept;

/** @brief Compute backend-neutral render state from a world-space level view. */
[[nodiscard]] LevelRenderState compute_level_render_state(
    const LevelRenderView &view, GraphicsBackend backend, float aspect_ratio) noexcept;

/** @brief Compute billboard camera data from an already-resolved level render state. */
[[nodiscard]] BillboardView
compute_billboard_view(const LevelRenderState &state) noexcept;

/**
 * @brief Generic static level renderer with an optional level asset.
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
   * @brief Initialize GPU resources for the provided level or an empty/static-less level.
   */
  [[nodiscard]] std::expected<void, stellar::platform::Error>
  initialize(stellar::platform::Window &window) override;

  /**
   * @brief Render the loaded level.
   */
  void render(float elapsed_seconds, float delta_seconds, int width,
              int height) noexcept override;

  /** @brief Override the fallback bounds-fit camera with a provided view. */
  void set_render_view(LevelRenderView view) noexcept;

  /** @brief Return to the automatic bounds-fit fallback camera. */
  void clear_render_view() noexcept;

  /** @brief Replace the retained presentation state used by subsequent render calls. */
  void set_presentation_state(LevelPresentationState state) noexcept;

  /** @brief Clear retained presentation data for no-map or missing-runtime fallback frames. */
  void clear_presentation_state() noexcept;

  /** @brief Inspect retained presentation data without mutating renderer state. */
  [[nodiscard]] const LevelPresentationState &presentation_state() const noexcept;

private:
  GraphicsBackend backend_ = GraphicsBackend::kOpenGL;
  std::optional<stellar::assets::LevelAsset> source_level_;
  std::optional<LevelRenderView> render_view_;
  LevelPresentationState presentation_state_;
  LevelBounds level_bounds_;
  RenderLevel level_;
  std::size_t debug_render_frame_index_ = 0;
  std::size_t debug_render_frame_limit_ = 0;
  bool debug_render_enabled_ = false;
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
