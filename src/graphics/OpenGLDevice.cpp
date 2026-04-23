#include "stellar/graphics/GraphicsDevice.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL.h>

#include <memory>
#include <utility>

namespace stellar::graphics {

namespace {

/**
 * @brief OpenGL 4.5+ implementation of GraphicsDevice.
 *
 * Manages basic framebuffer operations. The Window class owns the
 * OpenGL context and VSync configuration.
 */
class OpenGLDevice : public GraphicsDevice {
public:
    OpenGLDevice() = default;
    ~OpenGLDevice() override { shutdown(); }

    std::expected<void, Error> initialize(void* window_handle) override {
        window_ = static_cast<SDL_Window*>(window_handle);
        if (!window_) {
            return std::unexpected<Error>(
                Error{-1, "Invalid window handle"});
        }

        // Set viewport to match drawable size.
        int w = 0;
        int h = 0;
        SDL_GetWindowSize(window_, &w, &h);
        glViewport(0, 0, w, h);

        return {};
    }

    void shutdown() override {
        window_ = nullptr;
    }

    void begin_frame() override {
        // No per-frame OpenGL setup required.
    }

    void end_frame() override {
        // No per-frame OpenGL teardown required.
    }

    void clear(float r, float g, float b, float a) override {
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void present() override {
        SDL_GL_SwapWindow(window_);
    }

private:
    SDL_Window* window_ = nullptr;
};

} // anonymous namespace

std::unique_ptr<GraphicsDevice> make_opengl_device() {
    return std::make_unique<OpenGLDevice>();
}

} // namespace stellar::graphics
