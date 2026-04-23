#pragma once

#include <expected>
#include <memory>
#include <string>

namespace stellar::graphics {

/**
 * @brief Simple error type for graphics operations.
 */
struct Error {
    int code;            ///< Numeric error code.
    std::string message; ///< Human-readable description.
};

/**
 * @brief Abstract interface for graphics API backends.
 *
 * Provides a common contract for OpenGL and Vulkan renderers.
 * All fallible operations return std::expected<void, Error>.
 */
class GraphicsDevice {
public:
    virtual ~GraphicsDevice() = default;

    /**
     * @brief Initialize the graphics device with a platform window handle.
     * @param window_handle Platform-specific window handle (e.g., SDL_Window*).
     * @return void on success, Error on failure.
     */
    virtual std::expected<void, Error> initialize(void* window_handle) = 0;

    /**
     * @brief Shutdown and release all graphics resources.
     */
    virtual void shutdown() = 0;

    /**
     * @brief Prepare for rendering a new frame.
     */
    virtual void begin_frame() = 0;

    /**
     * @brief Finalize the current frame.
     */
    virtual void end_frame() = 0;

    /**
     * @brief Clear the screen with a solid color.
     * @param r Red component [0, 1].
     * @param g Green component [0, 1].
     * @param b Blue component [0, 1].
     * @param a Alpha component [0, 1].
     */
    virtual void clear(float r, float g, float b, float a) = 0;

    /**
     * @brief Present the rendered frame to the screen.
     */
    virtual void present() = 0;
};

/**
 * @brief Create an OpenGL graphics device.
 * @return Unique pointer to a GraphicsDevice implementation.
 */
std::unique_ptr<GraphicsDevice> create_opengl_device();

} // namespace stellar::graphics
