#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

#include <SDL2/SDL.h>

#include "stellar/graphics/metal/MetalGraphicsDevice.hpp"
#include "stellar/platform/Window.hpp"

namespace {

constexpr int kSkip = 77;

bool context_tests_enabled() noexcept {
  const char *enabled = std::getenv("STELLAR_RUN_METAL_CONTEXT_TESTS");
  return enabled != nullptr && std::string_view(enabled) == "1";
}

} // namespace

int main() {
  if (!context_tests_enabled()) {
    std::cout << "Skipping Metal render readback; set "
                 "STELLAR_RUN_METAL_CONTEXT_TESTS=1\n";
    return kSkip;
  }

  stellar::platform::Window window;
  auto window_result = window.create(
      16, 16, "stellar-metal-readback",
      SDL_WINDOW_METAL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN);
  if (!window_result) {
    std::cerr << window_result.error().message << '\n';
    return 1;
  }

  stellar::graphics::metal::MetalGraphicsDevice device;
  auto init_result = device.initialize(window);
  if (!init_result) {
    std::cerr << init_result.error().message << '\n';
    return 1;
  }

  device.request_frame_readback();
  device.begin_frame(16, 16);
  device.end_frame();

  auto readback_result = device.take_frame_readback();
  if (!readback_result) {
    std::cerr << readback_result.error().message << '\n';
    return 1;
  }
  assert(readback_result->has_value());
  const auto &readback = **readback_result;
  assert(readback.width > 0);
  assert(readback.height > 0);
  assert(readback.rgba_pixels.size() ==
         static_cast<std::size_t>(readback.width) *
             static_cast<std::size_t>(readback.height) * 4U);
  assert(readback.rgba_pixels[0] == 0U);
  assert(readback.rgba_pixels[1] == 0U);
  assert(readback.rgba_pixels[2] == 0U);
  assert(readback.rgba_pixels[3] == 255U);
  return 0;
}
