#include "stellar/platform/Input.hpp"

#include <cstring>

namespace stellar::platform {

void Input::process_event(const sf::Event& event) noexcept {
    if (event.is<sf::Event::KeyPressed>()) {
        const auto* key_event = event.getIf<sf::Event::KeyPressed>();
        if (key_event) {
            const auto key = key_event->code;
            const auto key_val = static_cast<int>(key);
            if (key_val >= 0 && key_val < kKeyCount) {
                keys_[key_val] = true;
            }
        }
    } else if (event.is<sf::Event::KeyReleased>()) {
        const auto* key_event = event.getIf<sf::Event::KeyReleased>();
        if (key_event) {
            const auto key = key_event->code;
            const auto key_val = static_cast<int>(key);
            if (key_val >= 0 && key_val < kKeyCount) {
                keys_[key_val] = false;
            }
        }
    }
}

bool Input::is_key_pressed(sf::Keyboard::Key key) const noexcept {
    const auto key_val = static_cast<int>(key);
    if (key_val >= 0 && key_val < kKeyCount) {
        return keys_[key_val];
    }
    return false;
}

void Input::reset_frame_state() noexcept {
    // Currently all state is held-state; no transient press flags yet.
    // This is the hook for future single-press / repeat logic.
}

} // namespace stellar::platform