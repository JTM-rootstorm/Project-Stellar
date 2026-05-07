#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

#include "stellar/platform/Error.hpp"

namespace stellar::graphics {

/**
 * @brief CPU-readable RGBA8 snapshot of a presented graphics frame.
 */
struct FrameReadback {
  /** @brief Frame width in drawable pixels. */
  int width = 0;

  /** @brief Frame height in drawable pixels. */
  int height = 0;

  /** @brief Tightly packed RGBA8 pixels ordered top-to-bottom, left-to-right. */
  std::vector<std::uint8_t> rgba_pixels;
};

/**
 * @brief Optional graphics-device extension for opt-in framebuffer readback.
 */
class FrameReadbackDevice {
public:
  /** @brief Release the readback extension through the concrete graphics device. */
  virtual ~FrameReadbackDevice() = default;

  /** @brief Request that the next rendered frame be copied into CPU-readable memory. */
  virtual void request_frame_readback() noexcept = 0;

  /**
   * @brief Take the most recently completed frame readback, when available.
   */
  [[nodiscard]] virtual std::expected<std::optional<FrameReadback>,
                                      stellar::platform::Error>
  take_frame_readback() noexcept = 0;
};

} // namespace stellar::graphics
