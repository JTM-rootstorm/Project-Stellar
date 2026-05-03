#include "stellar/platform/Input.hpp"

#include <SDL2/SDL.h>

#include <cassert>

namespace {

void synthesize_key(stellar::platform::Input& input,
                    SDL_Scancode scancode,
                    Uint32 type,
                    Uint8 repeat = 0) {
    SDL_Event event{};
    event.type = type;
    event.key.type = type;
    event.key.keysym.scancode = scancode;
    event.key.repeat = repeat;
    input.process_event(event);
}

void synthesize_mouse_motion(stellar::platform::Input& input, int xrel, int yrel) {
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.type = SDL_MOUSEMOTION;
    event.motion.xrel = xrel;
    event.motion.yrel = yrel;
    input.process_event(event);
}

void synthesize_mouse_button(stellar::platform::Input& input, Uint8 button) {
    SDL_Event event{};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = button;
    input.process_event(event);
}

void mouse_deltas_accumulate_and_reset() {
    stellar::platform::Input input;

    synthesize_mouse_motion(input, 4, -2);
    synthesize_mouse_motion(input, -1, 5);

    assert(input.mouse_delta_x() == 3);
    assert(input.mouse_delta_y() == 3);

    input.reset_frame_state();

    assert(input.mouse_delta_x() == 0);
    assert(input.mouse_delta_y() == 0);
}

void key_held_survives_reset_but_pressed_is_transient() {
    stellar::platform::Input input;

    synthesize_key(input, SDL_SCANCODE_W, SDL_KEYDOWN);
    assert(input.is_key_pressed(SDL_SCANCODE_W));
    assert(input.was_key_pressed(SDL_SCANCODE_W));

    input.reset_frame_state();

    assert(input.is_key_pressed(SDL_SCANCODE_W));
    assert(!input.was_key_pressed(SDL_SCANCODE_W));

    synthesize_key(input, SDL_SCANCODE_W, SDL_KEYUP);
    assert(!input.is_key_pressed(SDL_SCANCODE_W));
}

void repeat_keydown_does_not_set_pressed_this_frame() {
    stellar::platform::Input input;

    synthesize_key(input, SDL_SCANCODE_SPACE, SDL_KEYDOWN, 1);

    assert(input.is_key_pressed(SDL_SCANCODE_SPACE));
    assert(!input.was_key_pressed(SDL_SCANCODE_SPACE));
}

void mouse_button_pressed_is_transient() {
    stellar::platform::Input input;

    synthesize_mouse_button(input, SDL_BUTTON_LEFT);
    assert(input.was_mouse_button_pressed(SDL_BUTTON_LEFT));

    input.reset_frame_state();
    assert(!input.was_mouse_button_pressed(SDL_BUTTON_LEFT));
}

} // namespace

int main() {
    mouse_deltas_accumulate_and_reset();
    key_held_survives_reset_but_pressed_is_transient();
    repeat_keydown_does_not_set_pressed_this_frame();
    mouse_button_pressed_is_transient();
    return 0;
}
