#include "stellar/graphics/GraphicsDevice.hpp"
#include "stellar/graphics/Renderer.hpp"
#include "stellar/platform/Window.hpp"
#include "stellar/platform/Input.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <cstdio>
#include <cstdlib>

using stellar::platform::Error;
using stellar::platform::Input;
using stellar::platform::Window;
using stellar::graphics::Renderer;

namespace {

constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;
constexpr int kTargetFps = 60;
constexpr int kFrameDelayMs = 1000 / kTargetFps;

} // anonymous namespace

int main(int /*argc*/, char* /*argv*/[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    Window window;
    if (auto result = window.create(kWindowWidth, kWindowHeight,
                                    "Stellar Engine");
        !result) {
        std::fprintf(stderr, "Window creation failed: %s\n",
                     result.error().message.c_str());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    if (auto result = window.set_vsync(true); !result) {
        std::fprintf(stderr, "Failed to set VSync: %s\n",
                     result.error().message.c_str());
        // Non-fatal; continue without VSync.
    }

    auto device = stellar::graphics::create_opengl_device();
    if (auto result = device->initialize(window.native_handle()); !result) {
        std::fprintf(stderr, "GraphicsDevice initialization failed: %s\n",
                     result.error().message.c_str());
        window.destroy();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    Renderer renderer;
    if (auto result = renderer.initialize(); !result) {
        std::fprintf(stderr, "Renderer initialization failed: %s\n",
                     result.error().message.c_str());
        device->shutdown();
        window.destroy();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    Input input;

    while (!window.should_close()) {
        const Uint32 frame_start = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            input.process_event(&event);
            if (event.type == SDL_QUIT) {
                window.request_close();
            }
        }

        if (input.is_key_pressed(SDLK_ESCAPE)) {
            window.request_close();
        }

        device->begin_frame();
        device->clear(0.0f, 0.0f, 0.0f, 1.0f);
        renderer.render_text("working", 316.0f, 288.0f, 3.0f, 0xFFFFFFFF);
        device->end_frame();
        device->present();

        input.reset_frame_state();

        const Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < kFrameDelayMs) {
            SDL_Delay(kFrameDelayMs - frame_time);
        }
    }

    // Renderer destructor runs automatically here.
    device->shutdown();
    window.destroy();
    SDL_Quit();
    return EXIT_SUCCESS;
}
