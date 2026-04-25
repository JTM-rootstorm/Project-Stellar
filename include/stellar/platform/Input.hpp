#pragma once

#include <SDL2/SDL.h>

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
    void process_event(const SDL_Event& event) noexcept;

    /**
     * @brief Check whether a key is currently held down.
     * @param key SDL key scancode (e.g., SDL_SCANCODE_ESCAPE).
     * @return true if the key is pressed, false otherwise.
     */
    [[nodiscard]] bool is_key_pressed(SDL_Scancode key) const noexcept;

    /**
     * @brief Reset all transient input state (e.g., single-press flags).
     *
     * Call once per frame after all logic has run.
     */
    void reset_frame_state() noexcept;

private:
    static constexpr int kKeyCount = SDL_NUM_SCANCODES;
    bool keys_[kKeyCount] = {};
};

} // namespace stellar::platform