#pragma once

#include <SDL2/SDL.h>

namespace stellar::platform {

/**
 * @brief Simple input state tracker.
 *
 * Processes SDL events and tracks keyboard hold state plus per-frame transient input.
 * No exceptions are thrown.
 */
class Input {
public:
    /** @brief Construct an input tracker with all keys, buttons, and deltas cleared. */
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
     * @brief Check whether a key was pressed this frame without repeat.
     * @param key SDL key scancode (e.g., SDL_SCANCODE_F1).
     * @return true only for the frame in which a non-repeat keydown was processed.
     */
    [[nodiscard]] bool was_key_pressed(SDL_Scancode key) const noexcept;

    /** @brief Accumulated relative mouse X motion since the last frame reset. */
    [[nodiscard]] int mouse_delta_x() const noexcept;

    /** @brief Accumulated relative mouse Y motion since the last frame reset. */
    [[nodiscard]] int mouse_delta_y() const noexcept;

    /**
     * @brief Check whether an SDL mouse button was pressed this frame.
     * @param button SDL button id such as SDL_BUTTON_LEFT.
     */
    [[nodiscard]] bool was_mouse_button_pressed(Uint8 button) const noexcept;

    /**
     * @brief Reset all transient input state (e.g., single-press flags).
     *
     * Call once per frame after all logic has run.
     */
    void reset_frame_state() noexcept;

private:
    static constexpr int kKeyCount = SDL_NUM_SCANCODES;
    static constexpr int kMouseButtonCount = 8;
    bool keys_[kKeyCount] = {};
    bool keys_pressed_this_frame_[kKeyCount] = {};
    bool mouse_buttons_pressed_this_frame_[kMouseButtonCount] = {};
    int mouse_delta_x_ = 0;
    int mouse_delta_y_ = 0;
};

} // namespace stellar::platform
