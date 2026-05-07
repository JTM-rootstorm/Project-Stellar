#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <SDL2/SDL.h>

#include "stellar/graphics/vulkan/VulkanGraphicsDevice.hpp"
#include "stellar/platform/Window.hpp"

namespace {

constexpr int kSkip = 77;

bool context_tests_enabled() noexcept {
    const char* enabled = std::getenv("STELLAR_RUN_VULKAN_CONTEXT_TESTS");
    return enabled != nullptr && std::string_view(enabled) == "1";
}

bool is_skip_condition(const std::string& message) noexcept {
    return message.find("no physical device") != std::string::npos ||
           message.find("no device supports graphics and present") != std::string::npos;
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
        std::cout << "Skipping Vulkan context smoke: display unavailable\n";
        std::cerr << window_result.error().message << '\n';
        return kSkip;
    }

    stellar::graphics::vulkan::VulkanGraphicsDevice device;
    auto init_result = device.initialize(window);
    if (!init_result) {
        if (is_skip_condition(init_result.error().message)) {
            std::cout << "Skipping Vulkan context smoke: no suitable Vulkan device\n";
            std::cerr << init_result.error().message << '\n';
            return kSkip;
        }
        std::cerr << init_result.error().message << '\n';
        return 1;
    }

    device.begin_frame(16, 16);
    device.end_frame();
    return 0;
}
