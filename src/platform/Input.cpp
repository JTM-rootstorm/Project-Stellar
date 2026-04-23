#include "stellar/platform/Input.hpp"

#include <SDL2/SDL.h>
#include <cstring>

namespace stellar::platform {

void Input::process_event(const void* sdl_event) noexcept {
    if (sdl_event == nullptr) {
        return;
    }

    const auto* event = static_cast<const SDL_Event*>(sdl_event);
    if (event->type == SDL_KEYDOWN) {
        const int sc = event->key.keysym.scancode;
        if (sc >= 0 && sc < kKeyCount) {
            keys_[sc] = true;
        }
    } else if (event->type == SDL_KEYUP) {
        const int sc = event->key.keysym.scancode;
        if (sc >= 0 && sc < kKeyCount) {
            keys_[sc] = false;
        }
    }
}

bool Input::is_key_pressed(int keycode) const noexcept {
    const int sc = SDL_GetScancodeFromKey(keycode);
    if (sc >= 0 && sc < kKeyCount) {
        return keys_[sc];
    }
    return false;
}

void Input::reset_frame_state() noexcept {
    // Currently all state is held-state; no transient press flags yet.
    // This is the hook for future single-press / repeat logic.
}

} // namespace stellar::platform
