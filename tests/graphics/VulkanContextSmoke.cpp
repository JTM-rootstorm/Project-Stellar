#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

#include <SDL2/SDL.h>

#include "stellar/platform/Window.hpp"

namespace {

constexpr int kSkip = 77;

bool context_tests_enabled() noexcept {
    const char* enabled = std::getenv("STELLAR_RUN_VULKAN_CONTEXT_TESTS");
    return enabled != nullptr && std::string_view(enabled) == "1";
}

} // namespace

int main() {
    if (!context_tests_enabled()) {
        std::cout << "Skipping Vulkan context smoke; set STELLAR_RUN_VULKAN_CONTEXT_TESTS=1\n";
        return kSkip;
    }

    stellar::platform::Window window;
    auto window_result = window.create(16, 16, "stellar-vulkan-smoke",
                                       SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
    if (!window_result) {
        std::cerr << window_result.error().message << '\n';
        return 1;
    }

    stellar::graphics::vulkan::VulkanGraphicsDevice device;
    auto init_result = device.initialize(window);
    if (!init_result) {
        std::cerr << init_result.error().message << '\n';
        return 1;
    }

    if (!device.is_initialized()) {
        std::cerr << "Vulkan device reported success but is not initialized\n";
        return 1;
    }

    device.begin_frame(16, 16);
    device.end_frame();

    return 0;
}
