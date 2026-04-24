#pragma once

#include <SFML/Window.hpp>

namespace stellar::platform {

/**
 * @brief Simple input state tracker.
 *
 * Processes SFML events and tracks the current keyboard state.
 * No exceptions are thrown.
 */
class Input {
public:
    Input() noexcept = default;

    /**
     * @brief Process a single SFML event to update internal state.
     *
     * This should be called for every sf::Event returned by window.pollEvent.
     */
    void process_event(const sf::Event& event) noexcept;

    /**
     * @brief Check whether a key is currently held down.
     * @param key SFML key code (e.g., sf::Keyboard::Escape).
     * @return true if the key is pressed, false otherwise.
     */
    [[nodiscard]] bool is_key_pressed(sf::Keyboard::Key key) const noexcept;

    /**
     * @brief Reset all transient input state (e.g., single-press flags).
     *
     * Call once per frame after all logic has run.
     */
    void reset_frame_state() noexcept;

private:
    static constexpr int kKeyCount = sf::Keyboard::KeyCount;
    bool keys_[kKeyCount] = {};
};

} // namespace stellar::platform