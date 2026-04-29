#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>

#include "stellar/assets/MeshAsset.hpp"
#include "stellar/assets/SceneAsset.hpp"
#include "stellar/graphics/GraphicsBackend.hpp"
#include "stellar/graphics/RenderScene.hpp"
#include "stellar/graphics/Renderer.hpp"
#include "stellar/scene/AnimationRuntime.hpp"

namespace stellar::graphics {

/**
 * @brief Imported animation playback selection for scene rendering.
 */
struct SceneRendererAnimationOptions {
    /** @brief Optional animation index to select. Defaults to zero when animations exist. */
    std::optional<std::size_t> animation_index;

    /** @brief Optional animation name to select. Takes precedence over animation_index. */
    std::optional<std::string> animation_name;

    /** @brief Whether playback should loop when an animation reaches its duration. */
    bool loop = true;
};

/**
 * @brief Axis-aligned world-space bounds for an imported scene.
 */
struct SceneBounds {
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
 * @brief Camera parameters fitted to scene bounds.
 */
struct SceneCameraFit {
    /** @brief Camera eye position. */
    std::array<float, 3> eye{0.0F, 0.0F, 3.0F};

    /** @brief Camera look-at target. */
    std::array<float, 3> target{0.0F, 0.0F, 0.0F};

    /** @brief Near clip plane. */
    float near_plane = 0.1F;

    /** @brief Far clip plane. */
    float far_plane = 100.0F;
};

/** @brief Compute aggregate world-space bounds for the default scene. */
[[nodiscard]] SceneBounds compute_scene_bounds(
    const stellar::assets::SceneAsset& scene) noexcept;

/** @brief Compute a stable camera fit for the provided scene bounds. */
[[nodiscard]] SceneCameraFit fit_camera_to_bounds(const SceneBounds& bounds,
                                                  float vertical_fov_degrees,
                                                  float aspect_ratio) noexcept;

/**
 * @brief Generic static scene renderer with a debug cube fallback.
 */
class SceneRenderer final : public Renderer {
public:
    /**
     * @brief Construct a renderer from an optional CPU-side scene asset using OpenGL.
     */
    explicit SceneRenderer(
        std::optional<stellar::assets::SceneAsset> scene = std::nullopt) noexcept;

    /**
     * @brief Construct a renderer for a specific runtime backend.
     */
    explicit SceneRenderer(GraphicsBackend backend,
                           std::optional<stellar::assets::SceneAsset> scene = std::nullopt,
                           SceneRendererAnimationOptions animation_options = {}) noexcept;

    /**
     * @brief Release renderer-owned scene resources.
     */
    ~SceneRenderer() noexcept;

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    /**
     * @brief Initialize GPU resources for the provided scene or fallback cube.
     */
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& window) override;

    /**
     * @brief Render the loaded scene, rotating only the debug cube fallback.
     */
    void render(float elapsed_seconds,
                float delta_seconds,
                int width,
                int height) noexcept override;

private:
    [[nodiscard]] static std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
    create_cube_mesh();

    [[nodiscard]] static stellar::assets::SceneAsset create_cube_scene();

    GraphicsBackend backend_ = GraphicsBackend::kOpenGL;
    SceneRendererAnimationOptions animation_options_;
    std::optional<stellar::assets::SceneAsset> source_scene_;
    std::optional<stellar::assets::SceneAsset> animation_scene_;
    std::optional<stellar::scene::AnimationPlayer> animation_player_;
    SceneBounds scene_bounds_;
    RenderScene scene_;
    bool using_debug_cube_ = false;
};

} // namespace stellar::graphics
