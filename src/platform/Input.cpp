#include "stellar/platform/Input.hpp"

#include <SDL2/SDL.h>
#include <cstring>

namespace stellar::platform {

void Input::process_event(const SDL_Event& event) noexcept {
    switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.keysym.scancode >= 0 && event.key.keysym.scancode < kKeyCount) {
                keys_[event.key.keysym.scancode] = true;
            }
            break;
        case SDL_KEYUP:
            if (event.key.keysym.scancode >= 0 && event.key.keysym.scancode < kKeyCount) {
                keys_[event.key.keysym.scancode] = false;
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

void Input::reset_frame_state() noexcept {
    // Currently all state is held-state; no transient press flags yet.
    // This is the hook for future single-press / repeat logic.
}

} // namespace stellar::platform