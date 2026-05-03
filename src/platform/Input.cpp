#include "stellar/platform/Input.hpp"

#include <SDL2/SDL.h>

#include <algorithm>

namespace stellar::platform {

void Input::process_event(const SDL_Event& event) noexcept {
    switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.keysym.scancode >= 0 && event.key.keysym.scancode < kKeyCount) {
                keys_[event.key.keysym.scancode] = true;
                if (event.key.repeat == 0) {
                    keys_pressed_this_frame_[event.key.keysym.scancode] = true;
                }
            }
            break;
        case SDL_KEYUP:
            if (event.key.keysym.scancode >= 0 && event.key.keysym.scancode < kKeyCount) {
                keys_[event.key.keysym.scancode] = false;
            }
            break;
        case SDL_MOUSEMOTION:
            mouse_delta_x_ += event.motion.xrel;
            mouse_delta_y_ += event.motion.yrel;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button < kMouseButtonCount) {
                mouse_buttons_pressed_this_frame_[event.button.button] = true;
            }
            break;
    }
}

bool Input::is_key_pressed(SDL_Scancode key) const noexcept {
    if (key >= 0 && key < kKeyCount) {
        return keys_[key];
    }
    return false;
}

bool Input::was_key_pressed(SDL_Scancode key) const noexcept {
    if (key >= 0 && key < kKeyCount) {
        return keys_pressed_this_frame_[key];
    }
    return false;
}

int Input::mouse_delta_x() const noexcept {
    return mouse_delta_x_;
}

int Input::mouse_delta_y() const noexcept {
    return mouse_delta_y_;
}

bool Input::was_mouse_button_pressed(Uint8 button) const noexcept {
    if (button < kMouseButtonCount) {
        return mouse_buttons_pressed_this_frame_[button];
    }
    return false;
}

void Input::reset_frame_state() noexcept {
    std::fill(keys_pressed_this_frame_, keys_pressed_this_frame_ + kKeyCount, false);
    std::fill(mouse_buttons_pressed_this_frame_,
              mouse_buttons_pressed_this_frame_ + kMouseButtonCount, false);
    mouse_delta_x_ = 0;
    mouse_delta_y_ = 0;
}

} // namespace stellar::platform
