#include "stellar/client/HudPresentation.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

stellar::network::GameplayEvent event(stellar::network::GameplayEventKind kind,
                                      std::uint64_t tick,
                                      std::uint32_t entity_id,
                                      std::string code,
                                      std::string message) {
    return stellar::network::GameplayEvent{.kind = kind,
                                           .tick = tick,
                                           .entity_id = entity_id,
                                           .player_id = 7,
                                           .code = std::move(code),
                                           .message = std::move(message)};
}

void pickup_collected_increments_count_and_records_message() {
    stellar::client::HudPresentationState state;

    stellar::client::apply_gameplay_event(
        state, event(stellar::network::GameplayEventKind::kPickupCollected, 10, 42, "gem",
                     "Gem collected"));

    assert(state.collected_pickup_count == 1);
    assert(state.recent_messages.size() == 1);
    assert(state.recent_messages[0] == "Gem collected");
}

void duplicate_event_identity_is_ignored_deterministically() {
    stellar::client::HudPresentationState state;
    const auto pickup = event(stellar::network::GameplayEventKind::kPickupCollected, 10, 42, "gem",
                              "Gem collected");

    stellar::client::apply_gameplay_event(state, pickup);
    stellar::client::apply_gameplay_event(state, pickup);

    assert(state.collected_pickup_count == 1);
    assert(state.recent_messages.size() == 1);
    assert(state.processed_events.size() == 1);
}

void same_entity_on_later_tick_is_a_new_authoritative_event() {
    stellar::client::HudPresentationState state;

    stellar::client::apply_gameplay_event(
        state, event(stellar::network::GameplayEventKind::kPickupCollected, 10, 42, "gem",
                     "Gem collected"));
    stellar::client::apply_gameplay_event(
        state, event(stellar::network::GameplayEventKind::kPickupCollected, 11, 42, "gem",
                     "Gem collected"));

    assert(state.collected_pickup_count == 2);
    assert(state.recent_messages.size() == 2);
}

void door_event_adds_recent_message_without_affecting_inventory_count() {
    stellar::client::HudPresentationState state;

    stellar::client::apply_gameplay_event(
        state,
        event(stellar::network::GameplayEventKind::kDoorStateChanged, 12, 0, "DoorBlocker", ""));

    assert(state.collected_pickup_count == 0);
    assert(state.recent_messages.size() == 1);
    assert(state.recent_messages[0] == "Door/gate state changed: DoorBlocker");
}

void reset_clears_disposable_state_for_new_session() {
    stellar::client::HudPresentationState state;
    stellar::client::apply_gameplay_events(
        state, {event(stellar::network::GameplayEventKind::kPickupCollected, 10, 42, "gem",
                      "Gem collected"),
                event(stellar::network::GameplayEventKind::kDoorStateChanged, 12, 0, "DoorBlocker",
                      "Door opened")});

    stellar::client::reset_hud_presentation(state);

    assert(state.collected_pickup_count == 0);
    assert(state.recent_messages.empty());
    assert(state.processed_events.empty());
}

void recent_messages_are_bounded() {
    stellar::client::HudPresentationState state;

    for (std::uint64_t tick = 0; tick < 12; ++tick) {
        stellar::client::apply_gameplay_event(
            state, event(stellar::network::GameplayEventKind::kDoorStateChanged, tick, 0,
                         "Door" + std::to_string(tick), ""));
    }

    assert(state.recent_messages.size() == 8);
    assert(state.recent_messages.front() == "Door/gate state changed: Door4");
    assert(state.recent_messages.back() == "Door/gate state changed: Door11");
}

} // namespace

int main() {
    pickup_collected_increments_count_and_records_message();
    duplicate_event_identity_is_ignored_deterministically();
    same_entity_on_later_tick_is_a_new_authoritative_event();
    door_event_adds_recent_message_without_affecting_inventory_count();
    reset_clears_disposable_state_for_new_session();
    recent_messages_are_bounded();
    return 0;
}
