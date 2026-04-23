#pragma once

namespace stellar::platform {

/**
 * @brief Simple input state tracker.
 *
 * Processes SDL events and tracks the current keyboard state.
 * No exceptions are thrown.
 */
class Input {
public:
    Input() noexcept = default;

    /**
     * @brief Process a single SDL event to update internal state.
     *
     * This should be called for every SDL_Event returned by SDL_PollEvent.
     */
    void process_event(const void* sdl_event) noexcept;

    /**
     * @brief Check whether a key is currently held down.
     * @param keycode SDL keycode (e.g., SDLK_ESCAPE).
     * @return true if the key is pressed, false otherwise.
     */
    [[nodiscard]] bool is_key_pressed(int keycode) const noexcept;

    /**
     * @brief Reset all transient input state (e.g., single-press flags).
     *
     * Call once per frame after all logic has run.
     */
    void reset_frame_state() noexcept;

private:
    static constexpr int kKeyCount = 512;
    bool keys_[kKeyCount] = {};
};

} // namespace stellar::platform
